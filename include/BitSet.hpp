#ifndef GVIZ_BITSET_HPP
#define GVIZ_BITSET_HPP

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <vector>

namespace gviz {

/**
 * A growable bitset backed by std::vector<uint64_t> word storage --
 * deliberately not std::vector<bool>, so the word-skipping set-bit
 * iterator below can skip all-zero words in O(1) rather than testing bit
 * by bit.
 *
 * Size() is the number of meaningful bits; the backing vector always holds
 * exactly ceil(Size()/64) words, and any bits beyond Size() in the last word
 * are kept zero by every mutator here, so Popcount()/the full-range
 * iterator never need to mask a partial tail word at call time.
 *
 * Test/Set/Clear are unchecked (no bounds validation), same contract as
 * std::vector::operator[].
 */
class BitSet {
public:
  /** Constructs a zeroed bitset of @p nbits bits. */
  explicit BitSet(size_t nbits = 0)
      : words_((nbits + 63) / 64, uint64_t{0}), size_(nbits) {}

  /** Number of meaningful bits. */
  size_t Size() const noexcept { return size_; }

  bool Test(size_t k) const noexcept {
    return (words_[k / 64] >> (k % 64)) & uint64_t{1};
  }

  void Set(size_t k) noexcept { words_[k / 64] |= (uint64_t{1} << (k % 64)); }

  void Clear(size_t k) noexcept {
    words_[k / 64] &= ~(uint64_t{1} << (k % 64));
  }

  /** Clears every bit in [0, Size()). */
  void ClearAll() noexcept { std::fill(words_.begin(), words_.end(), uint64_t{0}); }

  /** Sets every bit in [0, Size()). Needed by Subgraph::MakeFull, which
   *  marks every vertex/edge of the parent graph present in one shot. */
  void SetAll() noexcept {
    std::fill(words_.begin(), words_.end(), ~uint64_t{0});
    size_t rem = size_ % 64;
    if (rem != 0 && !words_.empty())
      words_.back() &= (uint64_t{1} << rem) - 1;
  }

  /**
   * Clears bits in the half-open range [start, end). Per word, the bits to
   * *keep* are those outside the range -- i.e. (bits before start, only
   * relevant in the start word) OR (bits at/after end, only relevant in
   * the end word). A word that is neither the start nor the end word (a
   * true "middle" word of a multi-word range) keeps nothing: every bit in
   * it falls inside [start, end).
   */
  void ClearRange(size_t start, size_t end) noexcept {
    if (start >= end)
      return;
    size_t startWord = start / 64;
    size_t endWord = (end - 1) / 64;
    for (size_t w = startWord; w <= endWord; w++) {
      uint64_t keep = 0;
      size_t base = w * 64;
      if (w == startWord && start > base)
        keep |= (uint64_t{1} << (start - base)) - 1;
      if (w == endWord) {
        size_t limit = end - base;
        if (limit < 64)
          keep |= ~((uint64_t{1} << limit) - 1);
      }
      words_[w] &= keep;
    }
  }

  /** Number of set bits in [0, Size()). */
  size_t Popcount() const noexcept {
    size_t count = 0;
    for (uint64_t w : words_)
      count += static_cast<size_t>(std::popcount(w));
    return count;
  }

  /** Number of set bits in the half-open range [start, end). */
  size_t PopcountRange(size_t start, size_t end) const noexcept {
    if (start >= end)
      return 0;
    size_t startWord = start / 64;
    size_t endWord = (end - 1) / 64;
    size_t count = 0;
    for (size_t w = startWord; w <= endWord; w++) {
      uint64_t word = words_[w];
      size_t base = w * 64;
      if (w == startWord && start > base)
        word &= ~((uint64_t{1} << (start - base)) - 1);
      if (w == endWord) {
        size_t limit = end - base;
        if (limit < 64)
          word &= (uint64_t{1} << limit) - 1;
      }
      count += static_cast<size_t>(std::popcount(word));
    }
    return count;
  }

  /**
   * Grows or shrinks the bitset to @p newSize bits, in place. New bits (on
   * growth) are zeroed; bits beyond @p newSize (on shrink) are dropped.
   */
  void Resize(size_t newSize) {
    size_t newWords = (newSize + 63) / 64;
    words_.resize(newWords, uint64_t{0});
    if (newSize < size_) {
      size_t rem = newSize % 64;
      if (rem != 0 && newWords > 0)
        words_[newWords - 1] &= (uint64_t{1} << rem) - 1;
    }
    size_ = newSize;
  }

  /**
   * Forward iterator over set bit indices in ascending order. Skips
   * all-zero words outright and uses std::countr_zero within nonzero words.
   * Default-constructed (or exhausted) iterators compare equal to each
   * other and to end() -- there is a single canonical "no more bits" state,
   * which is what lets Range() below reuse this same type as both a
   * whole-set and a bounded-range iterator without a separate end type.
   */
  class iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = size_t;
    using difference_type = std::ptrdiff_t;
    using pointer = const size_t *;
    using reference = size_t;

    iterator() = default;

    size_t operator*() const noexcept { return idx_; }

    iterator &operator++() noexcept {
      Advance();
      return *this;
    }

    iterator operator++(int) noexcept {
      iterator tmp = *this;
      Advance();
      return tmp;
    }

    bool operator==(const iterator &other) const noexcept {
      return idx_ == other.idx_;
    }
    bool operator!=(const iterator &other) const noexcept {
      return !(*this == other);
    }

  private:
    friend class BitSet;

    static constexpr size_t kNone = static_cast<size_t>(-1);

    /** Begins searching at bit index @p start (start == 0 searches from
     *  bit 0); @p end is the exclusive upper bound of the search. */
    iterator(const uint64_t *words, size_t end, size_t start) noexcept
        : words_(words), end_(end), idx_(start == 0 ? kNone : start - 1) {
      Advance();
    }

    void Advance() noexcept {
      size_t next = (idx_ == kNone) ? 0 : idx_ + 1;
      if (next >= end_) {
        idx_ = kNone;
        return;
      }

      size_t numWords = (end_ + 63) / 64;
      size_t wordIdx = next / 64;

      while (wordIdx < numWords) {
        uint64_t w = words_[wordIdx];
        size_t base = wordIdx * 64;

        size_t bitOff = next - base;
        if (bitOff > 0)
          w &= ~((uint64_t{1} << bitOff) - 1);

        size_t wordLimit = end_ - base;
        if (wordLimit < 64)
          w &= (uint64_t{1} << wordLimit) - 1;

        if (w != 0) {
          idx_ = base + static_cast<size_t>(std::countr_zero(w));
          return;
        }

        wordIdx++;
        next = wordIdx * 64;
      }

      idx_ = kNone;
    }

    const uint64_t *words_ = nullptr;
    size_t end_ = 0;
    size_t idx_ = kNone;
  };

  /** A begin()/end() pair over a bounded bit-index range; see Range(). */
  class RangeView {
  public:
    iterator begin() const noexcept { return begin_; }
    iterator end() const noexcept { return iterator(); }

  private:
    friend class BitSet;
    explicit RangeView(iterator b) noexcept : begin_(b) {}
    iterator begin_;
  };

  iterator begin() const noexcept { return iterator(words_.data(), size_, size_t{0}); }
  iterator end() const noexcept { return iterator(); }

  /** Iterates set bits within the half-open range [start, end) only. */
  RangeView Range(size_t start, size_t end) const noexcept {
    return RangeView(iterator(words_.data(), end, start));
  }

private:
  std::vector<uint64_t> words_;
  size_t size_;
};

static_assert(std::input_iterator<BitSet::iterator>);

} // namespace gviz

#endif
