#include "renderer/embeddings/gvizEmbeddedTree.h"
#include "core/alloc.h"
#include "dsa/gvizArray.h"
#include "dsa/gvizBitArray.h"
#include "dsa/gvizGraph.h"
#include "dsa/gvizTree.h"
#include "raymath.h"
#include "renderer/embeddings/gvizEmbeddedGraph.h"
#include "utils/serializers.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define XSEPERATION 500.0f
#define YSEPERATION 1000.0f

typedef struct SeparationResult {
  float lOffset;
  float rOffset;
  float totalNewSeperation;
  float rmostSeperation;
} SeparationResult;

typedef struct SubtreePairExtremes {
  size_t ll;
  size_t lr;
  size_t rl;
  size_t rr;
} SubtreePairExtremes;

Vector2 Vector2Swap(const Vector2 vec) { return (Vector2){vec.y, vec.x}; }

float iterateContourRightward(const gvizEmbeddedTree *state, size_t *contour) {
  gvizArray *neighbors = gvizGraphGetVertexNeighbors(
      ((gvizEmbeddedGraph *)state)->graph, *contour);

  float out = state->dec[*contour].offsets[neighbors->count - 1];
  *contour = *(size_t *)gvizArrayTail(neighbors);
  return out;
}

float iterateContourLeftward(const gvizEmbeddedTree *state, size_t *contour) {
  gvizArray *neighbors = gvizGraphGetVertexNeighbors(
      ((gvizEmbeddedGraph *)state)->graph, *contour);
  float out = state->dec[*contour].offsets[0];
  *contour = *(size_t *)neighbors->arr;
  return out;
}

size_t getAncestor(gvizEmbeddedTree *state, size_t root, size_t i) {
  gvizArray *children =
      gvizGraphGetVertexNeighbors(((gvizEmbeddedGraph *)state)->graph, root);
  size_t ancestor = state->dec[i].ancestor;
  int res = gvizArrayFindOne(children, &ancestor);
  return res == -1 ? gvizArrayFindOne(children, &state->defaultAncestor) : res;
}

void seperationsToOffsets(float *seperations, float *offsets,
                          size_t rightSubtreeIndex) {
  memset(offsets, 0, sizeof(float) * rightSubtreeIndex);
  float total = 0.0;
  for (size_t i = 0; i < rightSubtreeIndex; i++) {
    total += seperations[i];
  }
  offsets[0] = -total / 2.0;
  for (size_t i = 1; i <= rightSubtreeIndex; i++) {
    offsets[i] = offsets[i - 1] + seperations[i - 1];
  }
}

void offsetsToSeperations(float *offsets, float *seperations,
                          size_t rightSubtreeIndex) {
  memset(seperations, 0, sizeof(float) * (rightSubtreeIndex - 1));
  for (size_t i = 0; i < rightSubtreeIndex - 1; i++) {
    seperations[i] = fabs(offsets[i + 1] - offsets[i]);
  }
}

void setAncestorAlongRightContour(gvizEmbeddedTree *state, size_t i) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  size_t curr = i;
  state->dec[curr].ancestor = i;
  while (!gvizTreeIsLeaf(embedding->graph, curr)) {
    iterateContourRightward(state, &curr);
    state->dec[curr].ancestor = i;
  }
}

void initializeRTLeaf(gvizEmbeddedTree *state, size_t i, size_t level) {
  state->dec[i].rMost = i;
  state->dec[i].lMost = i;
  state->dec[i].ancestor = i;
  state->dec[i].depth = level;
  if (level > state->height)
    state->height = level;

  // We still need to give an offset array to track lMost and rMost offsets.
  state->dec[i].offsets = state->offsetsOrigin + state->offsetsOffset;
  state->offsetsOffset++;
  return;
}

void initializeRTSubtreeRoot(gvizEmbeddedTree *state, size_t root,
                             size_t level) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, root);

  if (gvizArrayIsEmpty(children)) { // Base case
    initializeRTLeaf(state, root, level);
    return;
  } else if (children->count == 1) // Special case
    setAncestorAlongRightContour(state, root);

  // Conquering initialization
  size_t lMostSubtree = *(size_t *)children->arr;
  state->defaultAncestor = lMostSubtree;
  state->dec[root].lMost = state->dec[lMostSubtree].lMost;
  state->dec[root].rMost = state->dec[lMostSubtree].rMost;
  state->dec[root].depth = level;

  // offsets array distribution
  state->dec[root].offsets = state->offsetsOrigin + state->offsetsOffset;
  state->offsetsOffset += children->count;
}

void createThreads(gvizEmbeddedTree *state, size_t root, size_t i,
                   size_t *lrContour, size_t *rlContour,
                   SubtreePairExtremes *extremes, SeparationResult *res) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;

  // left subtree (blob of subtrees) is deeper. thread rr.
  if (!gvizTreeIsLeaf(embedding->graph, *lrContour) &&
      gvizTreeIsLeaf(embedding->graph, *rlContour)) {
    res->lOffset += iterateContourRightward(state, lrContour);
    gvizGraphAddEdge(embedding->graph, extremes->rr, *lrContour);
    gvizSetBit(state->thread, extremes->rr);
    state->dec[extremes->rr].offsets[0] = state->dec[extremes->rr].offsets[0] -
                                          res->rmostSeperation + res->lOffset;

  }
  // right subtree is deeper. thread ll.
  else if (gvizTreeIsLeaf(embedding->graph, *lrContour) &&
           !gvizTreeIsLeaf(embedding->graph, *rlContour)) {
    res->rOffset += iterateContourLeftward(state, rlContour);
    gvizGraphAddEdge(embedding->graph, extremes->ll, *rlContour);
    gvizSetBit(state->thread, extremes->ll);
    state->dec[extremes->ll].offsets[0] =
        state->dec[extremes->ll].offsets[0] + res->totalNewSeperation / 2.0 +
        state->dec[root].offsets[i] + res->rOffset;
  }
}

void updateExtremes(gvizEmbeddedTree *state, size_t root,
                    SubtreePairExtremes *extremes, SeparationResult *res) {
  // update offsets from extreme vertices
  if (state->dec[extremes->rl].depth > state->dec[extremes->ll].depth) {
    state->dec[root].lMost = extremes->rl;
    state->dec[extremes->rl].offsets[0] -= res->totalNewSeperation / 2.0;
  } else {
    state->dec[root].lMost = extremes->ll;
    state->dec[extremes->ll].offsets[0] += res->totalNewSeperation / 2.0;
  }
  if (state->dec[extremes->lr].depth > state->dec[extremes->rr].depth) {
    state->dec[root].rMost = extremes->lr;
    state->dec[extremes->lr].offsets[0] += res->totalNewSeperation / 2.0;
  } else {
    state->dec[root].rMost = extremes->rr;
    state->dec[extremes->rr].offsets[0] -= res->totalNewSeperation / 2.0;
  }
}

SeparationResult separateAlongContours(gvizEmbeddedTree *state,
                                       size_t *lrContour, size_t *rlContour) {
  assert(state->parents[*lrContour] == state->parents[*rlContour]);

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  float lOffset = 0, rOffset = 0, lstep, rstep, currsep = 1.0;
  size_t ancestor, root = state->parents[*lrContour];
  gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, root);

  // tracks how much seperation needs to be added to merge the right subtree.
  // newSeperation[i] = x, means all seperations with index >= i will gain x
  // units of seperation.
  size_t rightSubtree = *rlContour;
  int rightSubtreeIndex = gvizArrayFindOne(children, rlContour);
  float newSeperations[rightSubtreeIndex];
  memset(newSeperations, 0, sizeof(float) * rightSubtreeIndex);
  newSeperations[rightSubtreeIndex - 1] = 1.0;

  while (!gvizTreeIsLeaf(embedding->graph, *lrContour) &&
         !gvizTreeIsLeaf(embedding->graph, *rlContour)) {

    // take one step along each contour, stores x-displacement
    rstep = iterateContourLeftward(state, rlContour);
    lstep = iterateContourRightward(state, lrContour);

    // update total offset
    rOffset += rstep;
    lOffset += lstep;

    // update seperation
    currsep += rstep;
    currsep -= lstep;

    if (fabs(currsep - 1.0) > 0.1) {
      ancestor = getAncestor(state, root, *lrContour);

      // # of Subtrees between the colliding vertices
      int n = rightSubtreeIndex - ancestor;

      newSeperations[ancestor] += fabs(1.0 - currsep) / (float)n;
      currsep = 1.0;
    }
  }

  // Loop to accumulate final seperation distributions
  float acc = 0.0, totalNewSeperation = 0.0;
  for (size_t i = 0; i < rightSubtreeIndex; i++) {
    // accumulate each element we see to build the seperation of this
    // iteration
    acc += newSeperations[i];

    // newSeperations[i] overwritten to store seperation of this iteration
    newSeperations[i] = acc;

    // accumulate all seperations for total added seperation
    totalNewSeperation += acc;
  }

  // Apply new seperation to state
  if (rightSubtreeIndex > 1) {
    float originalSeperations[rightSubtreeIndex - 1];
    offsetsToSeperations(state->dec[root].offsets, originalSeperations,
                         rightSubtreeIndex);
    for (size_t i = 0; i < rightSubtreeIndex - 1; i++) {
      newSeperations[i] += originalSeperations[i];
    }
  }
  seperationsToOffsets(newSeperations, state->dec[root].offsets,
                       rightSubtreeIndex);

  // return offsets
  return (SeparationResult){lOffset, rOffset, totalNewSeperation,
                            newSeperations[rightSubtreeIndex - 1]};
}

void combineSubtreeLeft(gvizEmbeddedTree *state, size_t root, size_t i) {
  if (i == 0)
    return;

  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, root);
  size_t lrContour, rlContour, rightSubtree;

  lrContour = *(size_t *)gvizArrayAtIndex(children, i - 1);
  rlContour = *(size_t *)gvizArrayAtIndex(children, i);
  rightSubtree = rlContour;
  SubtreePairExtremes extremes = {
      state->dec[root].lMost,
      state->dec[root].rMost,
      state->dec[rlContour].lMost,
      state->dec[rlContour].rMost,
  };

  // Iterates through both contours and separates all children to add right
  // subtree to the blob.
  SeparationResult res = separateAlongContours(state, &lrContour, &rlContour);

  // Maintain ancestor values.
  // If the right subtree was deeper or as deep
  if (!gvizTreeIsLeaf(embedding->graph, rlContour) ||
      gvizTreeIsLeaf(embedding->graph, lrContour)) {
    // New default ancestor
    state->defaultAncestor = rightSubtree;
  } else {
    // Sets rightSubtree to be the ancestor along all rr contour vertices
    setAncestorAlongRightContour(state, rightSubtree);
  }

  updateExtremes(state, root, &extremes, &res);
  createThreads(state, root, i, &lrContour, &rlContour, &extremes, &res);
}

int gvizEmbeddedTreeCalculateOffsets(gvizEmbeddedTree *state, size_t root,
                                     size_t level) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, root);

  // Divide
  for (size_t i = 0; i < children->count; i++) {
    gvizEmbeddedTreeCalculateOffsets(
        state, *(size_t *)gvizArrayAtIndex(children, i), level);
  }

  // Initialization. Includes base case (leaf node)
  initializeRTSubtreeRoot(state, root, level);

  // Conquer
  for (size_t i = 0; i < children->count; i++) {
    combineSubtreeLeft(state, root, i);
  }

  return 0;
}

int gvizEmbeddedTreeRTInit(gvizEmbeddedTree *state, gvizGraph *graph,
                           size_t root) {

  // This memset allows us to simply run gvizEmbeddedTreeRelease if init ever
  // returns -1, without having to worry about double freeing and leaks.
  memset(state, 0, sizeof(gvizEmbeddedTree));
  int res;

  res = gvizEmbeddedGraphInit((gvizEmbeddedGraph *)state, graph);
  if (res < 0)
    return res;
  state->parents = GVIZ_ALLOC(sizeof(int) * graph->vertices.count);
  if (!state->parents)
    return -1;
  if (gvizGraphIsTree(graph, state->parents) < 0) {
    return -1;
  }

  state->dec = GVIZ_ALLOC(sizeof(gvizRTDecorators) * graph->vertices.count);
  if (!state->dec)
    return -1;

  state->thread = GVIZ_ALLOC(sizeof(GVIZ_BIT_UNIT) *
                             GVIZ_ARRAY_UNITS(graph->vertices.count));
  if (!state->thread)
    return -1;
  memset(state->thread, 0,
         sizeof(GVIZ_BIT_UNIT) * GVIZ_ARRAY_UNITS(graph->vertices.count));

  state->offsetsOrigin =
      GVIZ_ALLOC(sizeof(float) * (graph->vertices.count - 1 +
                                  gvizTreeCountLeaves(graph, root)));
  if (!state->offsetsOrigin)
    return -1;
  memset(state->offsetsOrigin, 0,
         sizeof(float) *
             (graph->vertices.count - 1 + gvizTreeCountLeaves(graph, root)));

  state->height = 0;
  state->offsetsOffset = 0;
  return 0;
}

void gvizEmbeddedTreeRTRelease(gvizEmbeddedTree *state) {
  if (state->dec)
    GVIZ_DEALLOC(state->dec);
  if (state->parents)
    GVIZ_DEALLOC(state->parents);
  if (state->thread)
    GVIZ_DEALLOC(state->thread);
  if (state->offsetsOrigin)
    GVIZ_DEALLOC(state->offsetsOrigin);
  if (state->graph.embedding.vertexPositions) {
    GVIZ_DEALLOC(state->graph.embedding.vertexPositions);
  }
}

int gvizEmbeddedTreeEmbed(gvizEmbeddedTree *state, size_t root,
                          Vector2 position) {
  gvizEmbeddedGraph *embedding = (gvizEmbeddedGraph *)state;
  gvizArray *children = gvizGraphGetVertexNeighbors(embedding->graph, root);
  embedding->embedding.vertexPositions[root] = position;
  for (size_t i = 0; i < children->count; i++) {
    gvizEmbeddedTreeEmbed(
        state, *(size_t *)gvizArrayAtIndex(children, i),
        Vector2Add(
            position,
            (Vector2){state->dec[root].offsets[i] * XSEPERATION, YSEPERATION}));
  }
  return 0;
}
