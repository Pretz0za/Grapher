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
