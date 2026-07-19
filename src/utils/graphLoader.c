#include "utils/graphLoader.h"
#include "core/alloc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GVIZ_ID_UNMAPPED ((size_t)-1)

void gvizEdgesFileOptionsInit(gvizEdgesFileOptions *opts) {
  opts->directed = 0;
  opts->zero_based = 0;
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
      GVIZ_DEALLOC(line);
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
      GVIZ_DEALLOC(line);
      fclose(file);
      return -1;
    }

    update_extremes(u, &state->min_id, &state->max_id, &state->have_id);
    update_extremes(v, &state->min_id, &state->max_id, &state->have_id);
    state->edge_count++;
  }

  GVIZ_DEALLOC(line);
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
      GVIZ_DEALLOC(line);
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
      GVIZ_DEALLOC(line);
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
      GVIZ_DEALLOC(line);
      fclose(file);
      return -1;
    }

    if (gvizGraphAddEdge(g, from, to) < 0) {
      GVIZ_DEALLOC(line);
      fclose(file);
      return -1;
    }
  }

  GVIZ_DEALLOC(line);
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

static int gexf_load_nodes(const char *buf, gvizGraph *out,
                           gvizGexfIdTable *ids) {
  const char *p = buf;
  const char *tag_start;
  while ((tag_start = gexf_next_tag(p, "<node", 5)) != NULL) {
    p = tag_start + 5;
    const char *tag_end = strchr(tag_start, '>');
    if (!tag_end)
      return -1;

    char id[256];
    if (gexf_extract_attr(tag_start, tag_end, "id", id, sizeof(id)) < 0)
      return -1;

    if (gvizGraphAddVertex(out, NULL, NULL, NULL) < 0)
      return -1;

    if (gexf_id_table_insert(ids, id, gvizGraphSize(out) - 1) < 0)
      return -1;

    p = tag_end + 1;
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

  int err = gexf_load_nodes(buf, out, &ids);
  if (err == 0)
    err = gexf_load_edges(buf, out, &ids);

  gexf_id_table_release(&ids);
  GVIZ_DEALLOC(buf);

  if (err < 0) {
    gvizGraphRelease(out);
    memset(out, 0, sizeof(*out));
    return -1;
  }

  return 0;
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
