#ifndef GVIZ_DENSEINDEX_HPP
#define GVIZ_DENSEINDEX_HPP

#include "GraphLike.hpp"

#include <cstddef>
#include <unordered_map>
#include <vector>

namespace gviz {

namespace detail {

/** True if @p G declares `static constexpr bool kDenseVertexHandles = true;`
 *  (gviz::Graph does), meaning its handles are already a dense [0, Size())
 *  range. Defaults to false for any type (e.g. Subgraph) that doesn't
 *  declare it. */
template <typename G>
constexpr bool GraphLikeHandlesDenseFromZero() {
  if constexpr (requires { G::kDenseVertexHandles; })
    return G::kDenseVertexHandles;
  else
    return false;
}

/** Whether @p G exposes an O(1) upper bound on its own handle range
 *  (Subgraph::VertexCapacity()) that DenseIndex can size a raw->local array
 *  against, instead of falling back to a hash map. */
template <typename G>
constexpr bool GraphLikeHasVertexCapacity() {
  return requires(const G &g) {
    { g.VertexCapacity() } -> std::convertible_to<size_t>;
  };
}

} // namespace detail

/**
 * Build-once raw-handle <-> dense-local-index bijection over an arbitrary
 * GraphLike view, computed by walking the view's vertex range exactly once
 * at construction. Lets per-vertex storage be sized to the view's actual
 * vertex count rather than the parent graph's capacity.
 *
 * Built once, at construction; not rebuilt on later mutation.
 *
 * When @p G's handles are already dense from 0 (gviz::Graph), ToLocal/ToRaw
 * are the identity function and no array is allocated. Otherwise ToLocal is
 * an O(1) array lookup when @p G exposes VertexCapacity() (as Subgraph
 * does), else a hash-map lookup.
 *
 * ToLocal/ToRaw are unchecked: @p raw must be a handle @p g actually
 * yielded at construction time; @p local must be < Size().
 */
template <GraphLike G>
class DenseIndex {
  static constexpr bool kDense = detail::GraphLikeHandlesDenseFromZero<G>();
  static constexpr bool kArrayBacked = !kDense && detail::GraphLikeHasVertexCapacity<G>();

public:
  /** Walks @p g's vertex range once, building the bijection (or, for the
   *  dense-handle special case, just recording the count).
   *  @throws std::bad_alloc on allocation failure. */
  explicit DenseIndex(const G &g) {
    if constexpr (kDense) {
      size_ = GraphLikeVertexCount(g);
    } else if constexpr (kArrayBacked) {
      localOfArray_.assign(g.VertexCapacity(), kInvalidLocal);
      rawOf_.reserve(GraphLikeVertexCount(g));
      for (size_t raw : g) {
        localOfArray_[raw] = rawOf_.size();
        rawOf_.push_back(raw);
      }
    } else {
      rawOf_.reserve(GraphLikeVertexCount(g));
      for (size_t raw : g) {
        localOfMap_.emplace(raw, rawOf_.size());
        rawOf_.push_back(raw);
      }
    }
  }

  DenseIndex(const DenseIndex &) = delete;
  DenseIndex &operator=(const DenseIndex &) = delete;
  DenseIndex(DenseIndex &&) noexcept = default;
  DenseIndex &operator=(DenseIndex &&) noexcept = default;
  ~DenseIndex() = default;

  /** The view's vertex count -- also the size of every local-indexed array
   *  built on top of this DenseIndex. */
  size_t Size() const noexcept {
    if constexpr (kDense)
      return size_;
    else
      return rawOf_.size();
  }

  /** Native handle -> dense local index in [0, Size()). Unchecked. */
  size_t ToLocal(size_t raw) const noexcept {
    if constexpr (kDense)
      return raw;
    else if constexpr (kArrayBacked)
      return localOfArray_[raw];
    else
      return localOfMap_.find(raw)->second; // unchecked: raw must be a known handle
  }

  /** Dense local index -> native handle. Unchecked. */
  size_t ToRaw(size_t local) const noexcept {
    if constexpr (kDense)
      return local;
    else
      return rawOf_[local];
  }

private:
  static constexpr size_t kInvalidLocal = static_cast<size_t>(-1);

  size_t size_ = 0;                         // dense-handle case only
  std::vector<size_t> rawOf_;               // local -> raw (general case)
  std::vector<size_t> localOfArray_;        // raw -> local, array-backed case
  std::unordered_map<size_t, size_t> localOfMap_; // raw -> local, hash-backed fallback
};

} // namespace gviz

#endif
