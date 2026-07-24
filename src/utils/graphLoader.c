#include "utils/graphLoader.h"
#include "core/alloc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_ID_UNMAPPED ((size_t)-1)

void gvizEdgesFileOptionsInit(gvizEdgesFileOptions *opts) {
  opts->directed = 0;
  opts->skip_header = 0;
}

static int parse_edge_line(const char *line, long long *u, long long *v) {
  while (*line == ' ' || *line == '\t')
    line++;
  if (*line == '\0' || *line == '\n' || *line == '\r' || *line == '%')
    return 0;

  char *end = NULL;
  long long a = strtoll(line, &end, 10);
  if (end == line)
    return -1;

  line = end;
  while (*line == ' ' || *line == '\t')
    line++;
  long long b = strtoll(line, &end, 10);
  if (end == line)
    return -1;

  *u = a;
  *v = b;
  return 1;
}

static void update_extremes(long long id, long long *min_id, long long *max_id,
                            int *have_id) {
  if (!*have_id) {
    *min_id = id;
    *max_id = id;
    *have_id = 1;
    return;
  }
  if (id < *min_id)
    *min_id = id;
  if (id > *max_id)
    *max_id = id;
}

typedef struct {
  long long min_id;
  long long max_id;
  size_t edge_count;
  int have_id;
  int skip_next_data_line;
} gvizEdgesScanState;

static int scan_edges_file(const char *path, const gvizEdgesFileOptions *opts,
                           gvizEdgesScanState *state) {
  FILE *file = fopen(path, "r");
  if (!file)
    return -1;

  state->min_id = 0;
  state->max_id = 0;
  state->edge_count = 0;
  state->have_id = 0;
  state->skip_next_data_line = opts->skip_header;

  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;
  while ((nread = getline(&line, &cap, file)) != -1) {
    long long u = 0;
    long long v = 0;
    int parsed = parse_edge_line(line, &u, &v);
    if (parsed < 0) {
      free(line);
      fclose(file);
      return -1;
    }
    if (parsed == 0)
      continue;

    if (state->skip_next_data_line) {
      state->skip_next_data_line = 0;
      continue;
    }

    if (u < 0 || v < 0) {
      free(line);
      fclose(file);
      return -1;
    }

    update_extremes(u, &state->min_id, &state->max_id, &state->have_id);
    update_extremes(v, &state->min_id, &state->max_id, &state->have_id);
    state->edge_count++;
  }

  free(line);
  fclose(file);
  return state->have_id ? 0 : -1;
}

static size_t external_to_index(long long id, long long min_id) {
  return (size_t)(id - min_id);
}

static size_t ensure_internal_vertex(gvizGraph *g, size_t *id_map, size_t map_len,
                                     size_t external_idx, size_t *next_internal) {
  if (external_idx >= map_len)
    return GVIZ_ID_UNMAPPED;
  if (id_map[external_idx] != GVIZ_ID_UNMAPPED)
    return id_map[external_idx];

  size_t internal = *next_internal;
  if (gvizGraphAddVertex(g, NULL, NULL, NULL) < 0)
    return GVIZ_ID_UNMAPPED;

  id_map[external_idx] = internal;
  (*next_internal)++;
  return internal;
}

static int load_edges_pass(const char *path, const gvizEdgesFileOptions *opts,
                           long long min_id, size_t *id_map, size_t map_len,
                           gvizGraph *g, size_t *next_internal) {
  FILE *file = fopen(path, "r");
  if (!file)
    return -1;

  int skip_next = opts->skip_header;
  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;
  while ((nread = getline(&line, &cap, file)) != -1) {
    long long u = 0;
    long long v = 0;
    int parsed = parse_edge_line(line, &u, &v);
    if (parsed < 0) {
      free(line);
      fclose(file);
      return -1;
    }
    if (parsed == 0)
      continue;

    if (skip_next) {
      skip_next = 0;
      continue;
    }

    if (u < 0 || v < 0) {
      free(line);
      fclose(file);
      return -1;
    }

    size_t from =
        ensure_internal_vertex(g, id_map, map_len, external_to_index(u, min_id),
                               next_internal);
    size_t to =
        ensure_internal_vertex(g, id_map, map_len, external_to_index(v, min_id),
                               next_internal);
    if (from == GVIZ_ID_UNMAPPED || to == GVIZ_ID_UNMAPPED) {
      free(line);
      fclose(file);
      return -1;
    }

    if (gvizGraphAddEdge(g, from, to) < 0) {
      free(line);
      fclose(file);
      return -1;
    }
  }

  free(line);
  fclose(file);
  return 0;
}

static char *gexf_read_file(const char *path, size_t *out_len) {
  FILE *file = fopen(path, "rb");
  if (!file)
    return NULL;

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return NULL;
  }
  long size = ftell(file);
  if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return NULL;
  }

  char *buf = GVIZ_ALLOC((size_t)size + 1);
  if (!buf) {
    fclose(file);
    return NULL;
  }

  size_t nread = fread(buf, 1, (size_t)size, file);
  fclose(file);
  buf[nread] = '\0';
  *out_len = nread;
  return buf;
}

static int gexf_tag_char_valid(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '/' ||
        c == '>';
}

static const char *gexf_next_tag(const char *from, const char *name,
                                 size_t name_len) {
  const char *p = from;
  while ((p = strstr(p, name)) != NULL) {
    if (gexf_tag_char_valid(p[name_len]))
      return p;
    p += name_len;
  }
  return NULL;
}

static int gexf_count_tags(const char *buf, const char *name) {
  size_t name_len = strlen(name);
  int count = 0;
  const char *p = buf;
  while ((p = gexf_next_tag(p, name, name_len)) != NULL) {
    count++;
    p += name_len;
  }
  return count;
}

static const char *gexf_bounded_strstr(const char *hay, const char *hay_end,
                                       const char *needle) {
  size_t needle_len = strlen(needle);
  if (needle_len == 0 || hay_end < hay)
    return NULL;
  size_t hay_len = (size_t)(hay_end - hay);
  if (needle_len > hay_len)
    return NULL;
  for (size_t i = 0; i + needle_len <= hay_len; i++) {
    if (memcmp(hay + i, needle, needle_len) == 0)
      return hay + i;
  }
  return NULL;
}

static const char *gexf_next_tag_bounded(const char *from, const char *end,
                                         const char *name, size_t name_len) {
  const char *p = from;
  while (p < end) {
    const char *found = gexf_bounded_strstr(p, end, name);
    if (!found)
      return NULL;
    if (found + name_len <= end && gexf_tag_char_valid(found[name_len]))
      return found;
    p = found + name_len;
  }
  return NULL;
}

static int gexf_extract_attr(const char *tag_start, const char *tag_end,
                             const char *attr_name, char *out,
                             size_t out_cap) {
  char pattern[64];
  snprintf(pattern, sizeof(pattern), "%s=\"", attr_name);
  char quote = '"';
  const char *p = gexf_bounded_strstr(tag_start, tag_end, pattern);
  if (!p) {
    snprintf(pattern, sizeof(pattern), "%s='", attr_name);
    quote = '\'';
    p = gexf_bounded_strstr(tag_start, tag_end, pattern);
  }
  if (!p)
    return -1;

  p += strlen(pattern);
  const char *val_end = memchr(p, quote, (size_t)(tag_end - p));
  if (!val_end)
    return -1;

  size_t len = (size_t)(val_end - p);
  if (len >= out_cap)
    return -1;

  memcpy(out, p, len);
  out[len] = '\0';
  return 0;
}

typedef struct {
  char *key;
  size_t value;
} gvizGexfIdSlot;

typedef struct {
  gvizGexfIdSlot *slots;
  size_t capacity;
} gvizGexfIdTable;

static size_t gexf_fnv1a(const char *s) {
  size_t h = 1469598103934665603ULL;
  for (; *s; s++) {
    h ^= (unsigned char)*s;
    h *= 1099511628211ULL;
  }
  return h;
}

static size_t gexf_next_pow2(size_t n) {
  size_t p = 1;
  while (p < n)
    p <<= 1;
  return p;
}

static int gexf_id_table_init(gvizGexfIdTable *t, size_t expected_count) {
  t->capacity = gexf_next_pow2(expected_count * 2 + 8);
  t->slots = GVIZ_ALLOC(t->capacity * sizeof(gvizGexfIdSlot));
  if (!t->slots)
    return -1;
  for (size_t i = 0; i < t->capacity; i++)
    t->slots[i].key = NULL;
  return 0;
}

static void gexf_id_table_release(gvizGexfIdTable *t) {
  for (size_t i = 0; i < t->capacity; i++)
    GVIZ_DEALLOC(t->slots[i].key);
  GVIZ_DEALLOC(t->slots);
}

static int gexf_id_table_insert(gvizGexfIdTable *t, const char *key,
                                size_t value) {
  size_t idx = gexf_fnv1a(key) & (t->capacity - 1);
  for (size_t i = 0; i < t->capacity; i++) {
    size_t probe = (idx + i) & (t->capacity - 1);
    if (t->slots[probe].key == NULL) {
      t->slots[probe].key = GVIZ_ALLOC(strlen(key) + 1);
      if (!t->slots[probe].key)
        return -1;
      strcpy(t->slots[probe].key, key);
      t->slots[probe].value = value;
      return 0;
    }
    if (strcmp(t->slots[probe].key, key) == 0)
      return 0;
  }
  return -1;
}

static int gexf_id_table_lookup(const gvizGexfIdTable *t, const char *key,
                                size_t *out) {
  size_t idx = gexf_fnv1a(key) & (t->capacity - 1);
  for (size_t i = 0; i < t->capacity; i++) {
    size_t probe = (idx + i) & (t->capacity - 1);
    if (t->slots[probe].key == NULL)
      return -1;
    if (strcmp(t->slots[probe].key, key) == 0) {
      *out = t->slots[probe].value;
      return 0;
    }
  }
  return -1;
}

typedef enum {
  GVIZ_GEXF_ATTR_STRING = 0,
  GVIZ_GEXF_ATTR_NUMBER = 1,
  GVIZ_GEXF_ATTR_BOOL = 2,
} gvizGexfAttrKind;

static gvizGexfAttrKind gexf_attr_kind_from_type(const char *type) {
  if (strcmp(type, "integer") == 0 || strcmp(type, "long") == 0 ||
      strcmp(type, "float") == 0 || strcmp(type, "double") == 0)
    return GVIZ_GEXF_ATTR_NUMBER;
  if (strcmp(type, "boolean") == 0)
    return GVIZ_GEXF_ATTR_BOOL;
  return GVIZ_GEXF_ATTR_STRING;
}

static int gexf_load_attribute_types(const char *buf, gvizGexfIdTable *types) {
  const char *p = buf;
  const char *tag_start;
  while ((tag_start = gexf_next_tag(p, "<attributes", 11)) != NULL) {
    const char *tag_end = strchr(tag_start, '>');
    if (!tag_end)
      return -1;

    char class_val[32];
    int is_edge_class =
        gexf_extract_attr(tag_start, tag_end, "class", class_val,
                          sizeof(class_val)) == 0 &&
        strcmp(class_val, "edge") == 0;

    const char *body_start = tag_end + 1;
    const char *body_end = strstr(body_start, "</attributes>");
    if (!body_end)
      return -1;

    if (!is_edge_class) {
      const char *ap = body_start;
      const char *attr_tag;
      while ((attr_tag = gexf_next_tag_bounded(ap, body_end, "<attribute",
                                               10)) != NULL) {
        const char *attr_tag_end =
            memchr(attr_tag, '>', (size_t)(body_end - attr_tag));
        if (!attr_tag_end)
          return -1;

        char id[256];
        char type[32];
        if (gexf_extract_attr(attr_tag, attr_tag_end, "id", id, sizeof(id)) <
            0)
          return -1;
        if (gexf_extract_attr(attr_tag, attr_tag_end, "type", type,
                              sizeof(type)) < 0)
          strcpy(type, "string");

        if (gexf_id_table_insert(types, id,
                                 (size_t)gexf_attr_kind_from_type(type)) < 0)
          return -1;

        ap = attr_tag_end + 1;
      }
    }

    p = body_end + strlen("</attributes>");
  }
  return 0;
}

typedef struct {
  char *data;
  size_t len;
  size_t cap;
} gvizStrBuf;

static int strbuf_init(gvizStrBuf *sb) {
  sb->cap = 128;
  sb->len = 0;
  sb->data = GVIZ_ALLOC(sb->cap);
  if (!sb->data)
    return -1;
  sb->data[0] = '\0';
  return 0;
}

static void strbuf_release(gvizStrBuf *sb) {
  GVIZ_DEALLOC(sb->data);
  sb->data = NULL;
  sb->len = 0;
  sb->cap = 0;
}

static int strbuf_append(gvizStrBuf *sb, const char *s, size_t n) {
  if (sb->len + n + 1 > sb->cap) {
    size_t new_cap = sb->cap;
    while (new_cap < sb->len + n + 1)
      new_cap *= 2;
    char *next = GVIZ_REALLOC(sb->data, new_cap);
    if (!next)
      return -1;
    sb->data = next;
    sb->cap = new_cap;
  }
  memcpy(sb->data + sb->len, s, n);
  sb->len += n;
  sb->data[sb->len] = '\0';
  return 0;
}

static int strbuf_append_str(gvizStrBuf *sb, const char *s) {
  return strbuf_append(sb, s, strlen(s));
}

static int gexf_json_append_escaped(gvizStrBuf *sb, const char *s) {
  if (strbuf_append(sb, "\"", 1) < 0)
    return -1;
  for (const unsigned char *c = (const unsigned char *)s; *c; c++) {
    char esc[8];
    int esc_len;
    switch (*c) {
    case '"':
      esc_len = snprintf(esc, sizeof(esc), "\\\"");
      break;
    case '\\':
      esc_len = snprintf(esc, sizeof(esc), "\\\\");
      break;
    case '\n':
      esc_len = snprintf(esc, sizeof(esc), "\\n");
      break;
    case '\r':
      esc_len = snprintf(esc, sizeof(esc), "\\r");
      break;
    case '\t':
      esc_len = snprintf(esc, sizeof(esc), "\\t");
      break;
    default:
      if (*c < 0x20) {
        esc_len = snprintf(esc, sizeof(esc), "\\u%04x", *c);
      } else {
        if (strbuf_append(sb, (const char *)c, 1) < 0)
          return -1;
        continue;
      }
      break;
    }
    if (strbuf_append(sb, esc, (size_t)esc_len) < 0)
      return -1;
  }
  return strbuf_append(sb, "\"", 1);
}

static int gexf_looks_like_number(const char *s) {
  if (*s == '-' || *s == '+')
    s++;
  int digits = 0;
  while (isdigit((unsigned char)*s)) {
    s++;
    digits++;
  }
  if (*s == '.') {
    s++;
    while (isdigit((unsigned char)*s)) {
      s++;
      digits++;
    }
  }
  if (digits == 0)
    return 0;
  if (*s == 'e' || *s == 'E') {
    s++;
    if (*s == '+' || *s == '-')
      s++;
    if (!isdigit((unsigned char)*s))
      return 0;
    while (isdigit((unsigned char)*s))
      s++;
  }
  return *s == '\0';
}

static int gexf_looks_like_bool(const char *s) {
  return strcmp(s, "true") == 0 || strcmp(s, "false") == 0;
}

static int gexf_json_append_value(gvizStrBuf *sb, gvizGexfAttrKind kind,
                                  const char *value) {
  if (kind == GVIZ_GEXF_ATTR_NUMBER && gexf_looks_like_number(value))
    return strbuf_append_str(sb, value);
  if (kind == GVIZ_GEXF_ATTR_BOOL && gexf_looks_like_bool(value))
    return strbuf_append_str(sb, value);
  return gexf_json_append_escaped(sb, value);
}

#define GEXF_JSON_INDENT "  "

static int gexf_json_append_field(gvizStrBuf *sb, int *has_fields,
                                  const char *key, gvizGexfAttrKind kind,
                                  const char *value) {
  const char *sep = *has_fields ? ",\n" GEXF_JSON_INDENT : "\n" GEXF_JSON_INDENT;
  if (strbuf_append_str(sb, sep) < 0)
    return -1;
  if (gexf_json_append_escaped(sb, key) < 0)
    return -1;
  if (strbuf_append_str(sb, ": ") < 0)
    return -1;
  if (gexf_json_append_value(sb, kind, value) < 0)
    return -1;
  *has_fields = 1;
  return 0;
}

static char *gexf_build_node_json(const char *label, const char *body_start,
                                  const char *body_end,
                                  const gvizGexfIdTable *attr_types) {
  gvizStrBuf sb;
  if (strbuf_init(&sb) < 0)
    return NULL;
  if (strbuf_append_str(&sb, "{") < 0)
    goto fail;

  int has_fields = 0;
  if (label && gexf_json_append_field(&sb, &has_fields, "label",
                                      GVIZ_GEXF_ATTR_STRING, label) < 0)
    goto fail;

  const char *ap = body_start;
  const char *attr_tag;
  while ((attr_tag = gexf_next_tag_bounded(ap, body_end, "<attvalue", 9)) !=
        NULL) {
    const char *attr_tag_end =
        memchr(attr_tag, '>', (size_t)(body_end - attr_tag));
    if (!attr_tag_end)
      goto fail;

    char for_id[256];
    char value[512];
    if ((gexf_extract_attr(attr_tag, attr_tag_end, "for", for_id,
                           sizeof(for_id)) < 0 &&
        gexf_extract_attr(attr_tag, attr_tag_end, "id", for_id,
                          sizeof(for_id)) < 0) ||
        gexf_extract_attr(attr_tag, attr_tag_end, "value", value,
                          sizeof(value)) < 0)
      goto fail;

    size_t kind_val = GVIZ_GEXF_ATTR_STRING;
    gexf_id_table_lookup(attr_types, for_id, &kind_val);

    if (gexf_json_append_field(&sb, &has_fields, for_id,
                               (gvizGexfAttrKind)kind_val, value) < 0)
      goto fail;

    ap = attr_tag_end + 1;
  }

  if (strbuf_append_str(&sb, has_fields ? "\n}" : "}") < 0)
    goto fail;

  char *result = GVIZ_REALLOC(sb.data, sb.len + 1);
  return result ? result : sb.data;

fail:
  strbuf_release(&sb);
  return NULL;
}

static int gexf_load_nodes(const char *buf, gvizGraph *out,
                           gvizGexfIdTable *ids,
                           const gvizGexfIdTable *attr_types) {
  const char *p = buf;
  const char *tag_start;
  while ((tag_start = gexf_next_tag(p, "<node", 5)) != NULL) {
    const char *tag_end = strchr(tag_start, '>');
    if (!tag_end)
      return -1;

    char id[256];
    if (gexf_extract_attr(tag_start, tag_end, "id", id, sizeof(id)) < 0)
      return -1;

    char label[512];
    int has_label = gexf_extract_attr(tag_start, tag_end, "label", label,
                                      sizeof(label)) == 0;

    int self_closing = tag_end > tag_start && *(tag_end - 1) == '/';
    const char *body_start = tag_end + 1;
    const char *body_end = body_start;
    const char *next_p;
    if (self_closing) {
      next_p = tag_end + 1;
    } else {
      const char *node_close = strstr(body_start, "</node>");
      if (!node_close)
        return -1;
      body_end = node_close;
      next_p = node_close + strlen("</node>");
    }

    char *json = gexf_build_node_json(has_label ? label : NULL, body_start,
                                      body_end, attr_types);
    if (!json)
      return -1;

    if (gvizGraphAddVertex(out, json, NULL, NULL) < 0) {
      GVIZ_DEALLOC(json);
      return -1;
    }

    if (gexf_id_table_insert(ids, id, gvizGraphSize(out) - 1) < 0)
      return -1;

    p = next_p;
  }
  return 0;
}

static int gexf_load_edges(const char *buf, gvizGraph *out,
                           const gvizGexfIdTable *ids) {
  const char *p = buf;
  const char *tag_start;
  while ((tag_start = gexf_next_tag(p, "<edge", 5)) != NULL) {
    p = tag_start + 5;
    const char *tag_end = strchr(tag_start, '>');
    if (!tag_end)
      return -1;

    char source[256];
    char target[256];
    if (gexf_extract_attr(tag_start, tag_end, "source", source,
                          sizeof(source)) < 0 ||
        gexf_extract_attr(tag_start, tag_end, "target", target,
                          sizeof(target)) < 0)
      return -1;

    size_t from;
    size_t to;
    if (gexf_id_table_lookup(ids, source, &from) < 0 ||
        gexf_id_table_lookup(ids, target, &to) < 0)
      return -1;

    if (gvizGraphAddEdge(out, from, to) < 0)
      return -1;

    p = tag_end + 1;
  }
  return 0;
}

int gvizGraphLoadFromGexfFile(const char *path, gvizGraph *out) {
  if (!path || !out)
    return -1;

  memset(out, 0, sizeof(*out));

  size_t len = 0;
  char *buf = gexf_read_file(path, &len);
  if (!buf)
    return -1;

  int node_count = gexf_count_tags(buf, "<node");
  if (node_count <= 0) {
    GVIZ_DEALLOC(buf);
    return -1;
  }

  if (gvizGraphInitAtCapacity(out, 0, (size_t)node_count) < 0) {
    GVIZ_DEALLOC(buf);
    return -1;
  }

  gvizGexfIdTable ids;
  if (gexf_id_table_init(&ids, (size_t)node_count) < 0) {
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    GVIZ_DEALLOC(buf);
    return -1;
  }

  int attr_count = gexf_count_tags(buf, "<attribute");
  gvizGexfIdTable attr_types;
  if (gexf_id_table_init(&attr_types, attr_count > 0 ? (size_t)attr_count : 0) <
      0) {
    gexf_id_table_release(&ids);
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    GVIZ_DEALLOC(buf);
    return -1;
  }

  int err = gexf_load_attribute_types(buf, &attr_types);
  if (err == 0)
    err = gexf_load_nodes(buf, out, &ids, &attr_types);
  if (err == 0)
    err = gexf_load_edges(buf, out, &ids);

  gexf_id_table_release(&attr_types);
  gexf_id_table_release(&ids);
  GVIZ_DEALLOC(buf);

  if (err < 0) {
    gvizGraphFreeVertexDataStrings(out);
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    return -1;
  }

  return 0;
}

void gvizGraphFreeVertexDataStrings(gvizGraph *g) {
  if (!g)
    return;
  size_t n = gvizGraphSize(g);
  for (size_t i = 0; i < n; i++) {
    GVIZ_DEALLOC(gvizGraphGetVertexData(g, i));
    gvizGraphSetVertexData(g, i, NULL);
  }
}

int gvizGraphLoadFromEdgesFile(const char *path,
                               const gvizEdgesFileOptions *opts, gvizGraph *out) {
  if (!path || !opts || !out)
    return -1;

  memset(out, 0, sizeof(*out));

  gvizEdgesScanState scan;
  if (scan_edges_file(path, opts, &scan) < 0)
    return -1;

  size_t map_len = (size_t)(scan.max_id - scan.min_id + 1);
  size_t *id_map = GVIZ_ALLOC(map_len * sizeof(size_t));
  if (!id_map)
    return -1;
  for (size_t i = 0; i < map_len; i++)
    id_map[i] = GVIZ_ID_UNMAPPED;

  if (gvizGraphInitAtCapacity(out, opts->directed, map_len) < 0) {
    GVIZ_DEALLOC(id_map);
    return -1;
  }

  size_t next_internal = 0;
  int err = load_edges_pass(path, opts, scan.min_id, id_map, map_len, out,
                            &next_internal);
  GVIZ_DEALLOC(id_map);

  if (err < 0) {
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    return -1;
  }

  return 0;
}

static int parse_obj_vertex_ref(const char *s, size_t vertex_count,
                                size_t *out) {
  char *end = NULL;
  long idx = strtol(s, &end, 10);
  if (end == s || idx == 0)
    return -1;
  if (idx > 0) {
    if ((size_t)idx > vertex_count)
      return -1;
    *out = (size_t)(idx - 1);
    return 0;
  }
  long abs_idx = -idx;
  if ((size_t)abs_idx > vertex_count)
    return -1;
  *out = vertex_count - (size_t)abs_idx;
  return 0;
}

static int obj_add_edge_undup(gvizGraph *g, size_t a, size_t b) {
  if (a == b)
    return 0;
  if (a > b) {
    size_t tmp = a;
    a = b;
    b = tmp;
  }
  gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, a);
  if (!nbrs)
    return -1;
  for (size_t i = 0; i < nbrs->count; i++) {
    if (*(size_t *)gvizArrayAtIndex(nbrs, i) == b)
      return 0;
  }
  return gvizGraphAddEdge(g, a, b);
}

static int obj_face_push(size_t **verts, size_t *cap, size_t *len, size_t v) {
  if (*len >= *cap) {
    size_t new_cap = *cap ? *cap * 2 : 8;
    size_t *next = GVIZ_REALLOC(*verts, new_cap * sizeof(size_t));
    if (!next)
      return -1;
    *verts = next;
    *cap = new_cap;
  }
  (*verts)[(*len)++] = v;
  return 0;
}

static int obj_add_face_edges(gvizGraph *g, const size_t *verts, size_t len) {
  if (len < 2)
    return 0;
  for (size_t i = 0; i + 1 < len; i++) {
    if (obj_add_edge_undup(g, verts[i], verts[i + 1]) < 0)
      return -1;
  }
  return obj_add_edge_undup(g, verts[len - 1], verts[0]);
}

static int obj_parse_face_line(gvizGraph *g, const char *line) {
  size_t vertex_count = gvizGraphSize(g);
  size_t *face_verts = NULL;
  size_t face_cap = 0;
  size_t face_len = 0;

  while (*line) {
    while (*line == ' ' || *line == '\t')
      line++;
    if (*line == '\0' || *line == '\n' || *line == '\r')
      break;

    size_t idx = 0;
    if (parse_obj_vertex_ref(line, vertex_count, &idx) < 0) {
      GVIZ_DEALLOC(face_verts);
      return -1;
    }
    if (obj_face_push(&face_verts, &face_cap, &face_len, idx) < 0) {
      GVIZ_DEALLOC(face_verts);
      return -1;
    }

    while (*line && *line != ' ' && *line != '\t' && *line != '\n' &&
           *line != '\r')
      line++;
  }

  int err = obj_add_face_edges(g, face_verts, face_len);
  GVIZ_DEALLOC(face_verts);
  return err;
}

int gvizGraphLoadFromObjFile(const char *path, gvizGraph *out) {
  if (!path || !out)
    return -1;

  memset(out, 0, sizeof(*out));
  if (gvizGraphInit(out, 0) < 0)
    return -1;

  FILE *file = fopen(path, "r");
  if (!file) {
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    return -1;
  }

  char *line = NULL;
  size_t cap = 0;
  ssize_t nread;
  int err = 0;

  while ((nread = getline(&line, &cap, file)) != -1) {
    const char *p = line;
    while (*p == ' ' || *p == '\t')
      p++;
    if (*p == '\0' || *p == '\n' || *p == '\r' || *p == '#')
      continue;

    if (p[0] == 'v' && (p[1] == ' ' || p[1] == '\t')) {
      if (gvizGraphAddVertex(out, NULL, NULL, NULL) < 0) {
        err = -1;
        break;
      }
      continue;
    }

    if (p[0] == 'f' && (p[1] == ' ' || p[1] == '\t')) {
      if (obj_parse_face_line(out, p + 2) < 0) {
        err = -1;
        break;
      }
    }
  }

  free(line);
  fclose(file);

  if (err < 0 || gvizGraphSize(out) == 0) {
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    return -1;
  }

  return 0;
}
