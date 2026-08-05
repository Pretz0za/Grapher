#include "ConnectedComponents.hpp"

#include <deque>
#include <limits>

namespace gviz::search {

Components ConnectedComponents(const Subgraph &sg) {
  constexpr size_t kUnlabeled = std::numeric_limits<size_t>::max();

  size_t n = sg.VertexCapacity();
  Components result;
  result.labels.assign(n, kUnlabeled);

  std::deque<size_t> queue;
  size_t comp = 0;
  for (size_t i = 0; i < n; i++) {
    if (!sg.HasVertex(i) || result.labels[i] != kUnlabeled)
      continue;

    result.labels[i] = comp;
    queue.push_back(i);

    while (!queue.empty()) {
      size_t curr = queue.front();
      queue.pop_front();

      for (size_t neighbor : sg.Neighbors(curr)) {
        if (result.labels[neighbor] != kUnlabeled)
          continue;
        result.labels[neighbor] = comp;
        queue.push_back(neighbor);
      }
    }
    comp++;
  }

  result.count = comp;
  return result;
}

std::vector<size_t> ConnectedComponentSizes(const std::vector<size_t> &labels,
                                             size_t componentCount) {
  constexpr size_t kUnlabeled = std::numeric_limits<size_t>::max();

  std::vector<size_t> sizes(componentCount, 0);
  for (size_t label : labels) {
    if (label == kUnlabeled)
      continue;
    sizes[label]++;
  }
  return sizes;
}

} // namespace gviz::search
