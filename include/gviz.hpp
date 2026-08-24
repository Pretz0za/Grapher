#ifndef GVIZ_HPP
#define GVIZ_HPP

/*
 * gviz — graph layout backend.
 *
 * This umbrella header pulls in the entire public interface. See
 * CLAUDE.md for the full architecture doc (layering, the Graph/Subgraph
 * hot-path design, the EmbeddedGraph base class and its sync/commit
 * machinery, and each embedder's specifics).
 */

#include "BitSet.hpp"
#include "BreadthFirst.hpp"
#include "ConnectedComponents.hpp"
#include "DenseIndex.hpp"
#include "DepthFirst.hpp"
#include "EmbeddedGraph.hpp"
#include "Error.hpp"
#include "ForceAtlas.hpp"
#include "ForceModel.hpp"
#include "GRIP.hpp"
#include "Graph.hpp"
#include "GraphLike.hpp"
#include "GraphLoader.hpp"
#include "Graphs.hpp"
#include "KNearest.hpp"
#include "KamadaKawai.hpp"
#include "Planar.hpp"
#include "QuadTree.hpp"
#include "ReingoldTilford.hpp"
#include "ReingoldTilfordTrace.hpp"
#include "SchnyderWood.hpp"
#include "SpringTutte.hpp"
#include "Subgraph.hpp"
#include "ThreadPool.hpp"
#include "Tree.hpp"
#include "Tutte.hpp"
#include "Vec.hpp"

// core/    — ThreadPool, Vec helpers.
// ds/      — Graph, Subgraph, BitSet, QuadTree, GraphLike (the traversal
//            concept both Graph and Subgraph satisfy) and DenseIndex (the
//            raw-handle <-> dense-local-index adapter built on it).
// search/  — BreadthFirst (+ BreadthFirstTree), DepthFirst,
//            ConnectedComponents, Tree (IsTree/IsLeaf/CountLeaves),
//            KNearest -- all templated over GraphLike.
// layout/  — EmbeddedGraph (GraphLike-agnostic base: dimension, positions,
//            actions, stats, draw mask), ForceModel, and the embedders.
//            ForceAtlas<G>, GRIP<G>, KamadaKawai<G>, Tutte<G>, SpringTutte<G>
//            are generic over GraphLike G (typically Graph or Subgraph).
//            Planar, SchnyderWood, ReingoldTilford, ReingoldTilfordTrace
//            (teaching-only, see its own header) stay concrete/Graph-
//            specific -- see each header's class doc for why.
// io/      — GraphLoader.
// graphs/  — synthetic graph generators (Graphs.hpp).

#endif
