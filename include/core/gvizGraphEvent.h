#ifndef _GVIZ_CORE_GRAPH_EVENT_H_
#define _GVIZ_CORE_GRAPH_EVENT_H_

#include <stddef.h>

/*
 * Graph mutation event kinds. A scene-managed graph fans out one of these
 * to all subscribers (excluding the originator) whenever a layer mutates it.
 */
typedef enum gvizGraphEventKind {
  GVIZ_GRAPH_VERTEX_ADDED,
  GVIZ_GRAPH_VERTEX_REMOVED,
  GVIZ_GRAPH_EDGE_ADDED,
  GVIZ_GRAPH_EDGE_REMOVED,
  GVIZ_GRAPH_POSITIONS_CHANGED,
  GVIZ_GRAPH_TOPOLOGY_REBUILT,
} gvizGraphEventKind;

typedef struct gvizGraphVertexEvent { size_t v; } gvizGraphVertexEvent;
typedef struct gvizGraphEdgeEvent   { size_t u, v; } gvizGraphEdgeEvent;

typedef void (*gvizGraphCallback)(void *self, gvizGraphEventKind kind,
                                  const void *payload);

#endif
