#ifndef GVIZ_H
#define GVIZ_H

/*
 * gviz — graph layout backend.
 *
 * This umbrella header pulls in the entire public interface. The library is
 * layered; each layer may only depend on the layers above it:
 *
 *   core/        allocation macros, vector math, thread pool.
 *                Knows nothing about graphs.
 *   ds/          hand-rolled data structures: gvizArray, gvizBitArray,
 *                gvizDeque, gvizGraph, gvizSubgraph, gvizTree, gvizQuadtree.
 *                Knows core only.
 *   algorithms/  BFS, DFS, connected components, k-nearest search over
 *                gvizSubgraph. Knows ds + core.
 *   embedders/   The layout algorithms. Every embedder state embeds a
 *                gvizEmbeddedGraph as its first member, so a pointer to any
 *                embedder state is a valid gvizEmbeddedGraph pointer.
 *                Front-ends drive embedders through the shared
 *                action/stat-series interface on gvizEmbeddedGraph without
 *                knowing which algorithm is underneath.
 *   utils/       graph file loaders (.edges, .gexf, .obj), synthetic test
 *                graph builders, element serializers. Knows ds; never
 *                embedders.
 *
 * Anything not reachable from this header (internal headers under src/,
 * e.g. the GRIP filtration internals) is not public interface and may change
 * without notice.
 */

#include "core/alloc.h"
#include "core/gvizThreadPool.h"
#include "core/gvizVec.h"

#include "ds/gvizArray.h"
#include "ds/gvizBitArray.h"
#include "ds/gvizDeque.h"
#include "ds/gvizGraph.h"
#include "ds/gvizQuadtree.h"
#include "ds/gvizSubgraph.h"
#include "ds/gvizTree.h"

#include "algorithms/search/gvizBreadthFirst.h"
#include "algorithms/search/gvizConnectedComponents.h"
#include "algorithms/search/gvizDepthFirst.h"
#include "algorithms/search/gvizKNearest.h"

#include "embedders/gvizEmbeddedGraph.h"
#include "embedders/gvizEmbeddedTree.h"
#include "embedders/gvizForceDirected.h"
#include "embedders/gvizForceEmbedder.h"
#include "embedders/gvizForceModel.h"
#include "embedders/gvizGRIPEmbedder.h"
#include "embedders/gvizPlanarEmbedder.h"
#include "embedders/gvizSpringTutteEmbedder.h"
#include "embedders/gvizTutteEmbedder.h"

#include "utils/graphLoader.h"
#include "utils/graphs.h"
#include "utils/serializers.h"

#endif
