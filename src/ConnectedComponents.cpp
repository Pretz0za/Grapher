#include "ConnectedComponents.hpp"

namespace gviz::search {

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
