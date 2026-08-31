#include "DepthFirst.hpp"

#include "BitSet.hpp"

#include <vector>

namespace gviz::search {

bool DepthFirst(const Subgraph &sg, Subgraph &out, size_t source) {
  if (!out.IsFull())
    return false;
  if (!sg.HasVertex(source))
    return false;

  size_t n = sg.VertexCapacity();
  BitSet seen(n);
  seen.Set(source);
  out.ShowVertex(source);

  std::vector<size_t> stack;
  stack.push_back(source);

  while (!stack.empty()) {
    size_t curr = stack.back();
    stack.pop_back();

    for (size_t neighbor : sg.Neighbors(curr)) {
      if (seen.Test(neighbor))
        continue;
      seen.Set(neighbor);

      out.ShowVertex(neighbor);
      out.ShowEdge(curr, neighbor);

      stack.push_back(neighbor);
    }
  }

  return true;
}

} // namespace gviz::search
