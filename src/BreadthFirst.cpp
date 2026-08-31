#include "BreadthFirst.hpp"

#include "BitSet.hpp"

#include <deque>
#include <limits>

namespace gviz::search {

namespace {
struct NodeDepth {
  size_t v;
  size_t d;
};
} // namespace

bool BreadthFirstTree(const Subgraph &sg, Subgraph &out, size_t source, size_t maxDepth,
                       std::vector<size_t> *distances) {
  if (!out.IsFull())
    return false;
  if (!sg.HasVertex(source))
    return false;

  size_t n = sg.VertexCapacity();
  if (distances) {
    distances->assign(n, std::numeric_limits<size_t>::max());
    (*distances)[source] = 0;
  }

  BitSet seen(n);
  seen.Set(source);
  out.ShowVertex(source);

  std::deque<NodeDepth> queue;
  queue.push_back({source, 0});

  while (!queue.empty()) {
    NodeDepth nd = queue.front();
    queue.pop_front();

    if (maxDepth && nd.d >= maxDepth)
      continue;

    for (size_t neighbor : sg.Neighbors(nd.v)) {
      if (seen.Test(neighbor))
        continue;
      seen.Set(neighbor);

      size_t nextDepth = nd.d + 1;
      if (distances)
        (*distances)[neighbor] = nextDepth;

      out.ShowVertex(neighbor);
      out.ShowEdge(nd.v, neighbor);

      queue.push_back({neighbor, nextDepth});
    }
  }

  return true;
}

} // namespace gviz::search
