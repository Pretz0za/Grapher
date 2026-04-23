#include "utils/gvizTreeIO.h"
#include "core/alloc.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Minimal hand-rolled parser over a mmap-style text buffer. */

typedef struct {
  const char *s;
  size_t i, n;
} cursor;

static void skipWs(cursor *c) {
  while (c->i < c->n && isspace((unsigned char)c->s[c->i]))
    c->i++;
}

static int match(cursor *c, char ch) {
  skipWs(c);
  if (c->i < c->n && c->s[c->i] == ch) {
    c->i++;
    return 1;
  }
  return 0;
}

static int parseUint(cursor *c, size_t *out) {
  skipWs(c);
  if (c->i >= c->n || !isdigit((unsigned char)c->s[c->i]))
    return -1;
  size_t v = 0;
  while (c->i < c->n && isdigit((unsigned char)c->s[c->i])) {
    v = v * 10 + (size_t)(c->s[c->i] - '0');
    c->i++;
  }
  *out = v;
  return 0;
}

/* Matches an unquoted key literally (e.g. "root") inside a quoted string. */
static int matchKey(cursor *c, const char *key) {
  skipWs(c);
  if (!match(c, '"'))
    return 0;
  size_t klen = strlen(key);
  if (c->i + klen > c->n || memcmp(c->s + c->i, key, klen) != 0)
    return 0;
  c->i += klen;
  if (!match(c, '"'))
    return 0;
  return match(c, ':');
}

static char *readFile(const char *path, size_t *outLen) {
  FILE *f = fopen(path, "rb");
  if (!f)
    return NULL;
  fseek(f, 0, SEEK_END);
  long sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz < 0) {
    fclose(f);
    return NULL;
  }
  char *buf = GVIZ_ALLOC((size_t)sz + 1);
  if (!buf) {
    fclose(f);
    return NULL;
  }
  size_t r = fread(buf, 1, (size_t)sz, f);
  fclose(f);
  buf[r] = '\0';
  *outLen = r;
  return buf;
}

int gvizLoadTreeJSON(const char *path, gvizGraph *out, size_t *outRoot) {
  size_t n = 0;
  char *buf = readFile(path, &n);
  if (!buf)
    return -1;
  cursor c = {buf, 0, n};

  size_t root = 0;
  size_t maxId = 0;
  int haveRoot = 0;

  /* First pass: scan for root and maxId. */
  if (!match(&c, '{'))
    goto fail;

  /* The JSON schema is small and fixed — accept keys in any order. We
   * do a simple two-pass: first pass walks to find root and the highest
   * id, second pass re-walks to build edges. */
  cursor snapshot = c;

  while (c.i < c.n) {
    skipWs(&c);
    if (c.i < c.n && c.s[c.i] == '}')
      break;
    if (matchKey(&c, "root")) {
      if (parseUint(&c, &root) != 0)
        goto fail;
      haveRoot = 1;
    } else if (matchKey(&c, "vertices")) {
      if (!match(&c, '['))
        goto fail;
      while (!match(&c, ']')) {
        if (!match(&c, '{'))
          goto fail;
        size_t id = 0;
        int haveId = 0;
        while (!match(&c, '}')) {
          if (matchKey(&c, "id")) {
            if (parseUint(&c, &id) != 0)
              goto fail;
            haveId = 1;
          } else if (matchKey(&c, "children")) {
            if (!match(&c, '['))
              goto fail;
            while (!match(&c, ']')) {
              size_t ch = 0;
              if (parseUint(&c, &ch) != 0)
                goto fail;
              if (ch > maxId)
                maxId = ch;
              match(&c, ',');
            }
          } else {
            goto fail;
          }
          match(&c, ',');
        }
        if (!haveId)
          goto fail;
        if (id > maxId)
          maxId = id;
        match(&c, ',');
      }
    } else {
      goto fail;
    }
    match(&c, ',');
  }

  if (!haveRoot)
    goto fail;

  /* Initialize graph. */
  if (gvizGraphInitAtCapacity(out, 1, maxId + 1) != 0)
    goto fail;
  for (size_t i = 0; i <= maxId; i++)
    gvizGraphAddVertex(out, NULL, NULL, NULL);

  /* Second pass: populate edges. */
  c = snapshot;
  while (c.i < c.n) {
    skipWs(&c);
    if (c.i < c.n && c.s[c.i] == '}')
      break;
    if (matchKey(&c, "root")) {
      size_t tmp;
      parseUint(&c, &tmp);
    } else if (matchKey(&c, "vertices")) {
      if (!match(&c, '['))
        goto fail_loaded;
      while (!match(&c, ']')) {
        if (!match(&c, '{'))
          goto fail_loaded;
        size_t id = 0;
        while (!match(&c, '}')) {
          if (matchKey(&c, "id")) {
            parseUint(&c, &id);
          } else if (matchKey(&c, "children")) {
            match(&c, '[');
            while (!match(&c, ']')) {
              size_t ch = 0;
              parseUint(&c, &ch);
              gvizGraphAddEdge(out, id, ch);
              match(&c, ',');
            }
          }
          match(&c, ',');
        }
        match(&c, ',');
      }
    }
    match(&c, ',');
  }

  GVIZ_DEALLOC(buf);
  if (outRoot)
    *outRoot = root;
  return 0;

fail_loaded:
  gvizGraphRelease(out);
fail:
  GVIZ_DEALLOC(buf);
  return -1;
}

int gvizSaveTreeJSON(const gvizGraph *g, size_t root, const char *path) {
  FILE *f = fopen(path, "w");
  if (!f)
    return -1;
  fprintf(f, "{\n  \"root\": %zu,\n  \"vertices\": [\n", root);
  for (size_t i = 0; i < g->vertices.count; i++) {
    fprintf(f, "    { \"id\": %zu, \"children\": [", i);
    gvizArray *nbrs = gvizGraphGetVertexNeighbors(g, i);
    if (nbrs) {
      for (size_t j = 0; j < nbrs->count; j++) {
        size_t ch = *(size_t *)gvizArrayAtIndex(nbrs, j);
        fprintf(f, "%s%zu", j == 0 ? "" : ", ", ch);
      }
    }
    fprintf(f, "] }%s\n", i + 1 == g->vertices.count ? "" : ",");
  }
  fprintf(f, "  ]\n}\n");
  fclose(f);
  return 0;
}
