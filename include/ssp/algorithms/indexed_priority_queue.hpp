#pragma once

#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <utility>
#include <functional>
#include <iostream>
#include <cmath>
#include "ssp/core/types.hpp"

namespace ssp::algorithms {

/**
 * @brief Lexicographical priority key used in D* Lite algorithm: [k1, k2].
 * Ordered such that (k1, k2) < (k1', k2') iff k1 < k1' or (k1 == k1' and k2 < k2').
 */
struct DStarKey {
    double k1{core::INF_COST};
    double k2{core::INF_COST};

    constexpr DStarKey() = default;
    constexpr DStarKey(double k1, double k2) : k1(k1), k2(k2) {}

    bool operator<(const DStarKey& other) const {
        if (k1 + core::EPSILON < other.k1) return true;
        if (other.k1 + core::EPSILON < k1) return false;
        return k2 + core::EPSILON < other.k2;
    }

    bool operator<=(const DStarKey& other) const {
        return !(other < *this);
    }

    bool operator>(const DStarKey& other) const {
        return other < *this;
    }

    bool operator>=(const DStarKey& other) const {
        return !(*this < other);
    }

    bool operator==(const DStarKey& other) const {
        return std::abs(k1 - other.k1) <= core::EPSILON && std::abs(k2 - other.k2) <= core::EPSILON;
    }

    bool operator!=(const DStarKey& other) const {
        return !(*this == other);
    }

    static constexpr DStarKey infinity() {
        return {core::INF_COST, core::INF_COST};
    }

    friend std::ostream& operator<<(std::ostream& os, const DStarKey& k) {
        os << "[" << k.k1 << ", " << k.k2 << "]";
        return os;
    }
};

/**
 * @brief High-performance binary min-heap with an auxiliary hash map for O(1) lookups
 * and O(log N) key updates / arbitrary removals.
 * 
 * @tparam KeyType Identifier type for elements (e.g. StateId = uint64_t).
 * @tparam PriorityType Priority key type (defaults to DStarKey).
 * @tparam Compare Comparator functor (defaults to std::less<PriorityType> for min-heap).
 */
template <
    typename KeyType = core::StateId,
    typename PriorityType = DStarKey,
    typename Compare = std::less<PriorityType>
>
class IndexedPriorityQueue {
public:
    struct Entry {
        KeyType key;
        PriorityType priority;
    };

private:
    std::vector<Entry> heap_;
    std::unordered_map<KeyType, size_t> position_;
    Compare comp_{};

    void swapNodes(size_t i, size_t j) {
        std::swap(heap_[i], heap_[j]);
        position_[heap_[i].key] = i;
        position_[heap_[j].key] = j;
    }

    void siftUp(size_t index) {
        while (index > 0) {
            size_t parent = (index - 1) / 2;
            if (comp_(heap_[index].priority, heap_[parent].priority)) {
                swapNodes(index, parent);
                index = parent;
            } else {
                break;
            }
        }
    }

    void siftDown(size_t index) {
        size_t size = heap_.size();
        while (true) {
            size_t smallest = index;
            size_t left = 2 * index + 1;
            size_t right = 2 * index + 2;

            if (left < size && comp_(heap_[left].priority, heap_[smallest].priority)) {
                smallest = left;
            }
            if (right < size && comp_(heap_[right].priority, heap_[smallest].priority)) {
                smallest = right;
            }

            if (smallest != index) {
                swapNodes(index, smallest);
                index = smallest;
            } else {
                break;
            }
        }
    }

public:
    IndexedPriorityQueue() = default;

    [[nodiscard]] bool empty() const noexcept {
        return heap_.empty();
    }

    [[nodiscard]] size_t size() const noexcept {
        return heap_.size();
    }

    [[nodiscard]] bool contains(const KeyType& key) const {
        return position_.find(key) != position_.end();
    }

    void clear() {
        heap_.clear();
        position_.clear();
    }

    /**
     * @brief Inserts a new key with associated priority or updates if already present.
     */
    void insert(const KeyType& key, const PriorityType& priority) {
        auto it = position_.find(key);
        if (it != position_.end()) {
            update(key, priority);
            return;
        }

        size_t index = heap_.size();
        heap_.push_back({key, priority});
        position_[key] = index;
        siftUp(index);
    }

    /**
     * @brief Updates priority of an existing key in O(log N).
     */
    void update(const KeyType& key, const PriorityType& newPriority) {
        auto it = position_.find(key);
        if (it == position_.end()) {
            insert(key, newPriority);
            return;
        }

        size_t index = it->second;
        PriorityType oldPriority = heap_[index].priority;
        heap_[index].priority = newPriority;

        if (comp_(newPriority, oldPriority)) {
            siftUp(index);
        } else {
            siftDown(index);
        }
    }

    /**
     * @brief Removes a specific key from the queue in O(log N).
     */
    bool remove(const KeyType& key) {
        auto it = position_.find(key);
        if (it == position_.end()) {
            return false;
        }

        size_t index = it->second;
        size_t lastIndex = heap_.size() - 1;

        if (index == lastIndex) {
            position_.erase(key);
            heap_.pop_back();
            return true;
        }

        swapNodes(index, lastIndex);
        position_.erase(key);
        heap_.pop_back();

        // Restore heap property at index
        siftUp(index);
        siftDown(index);
        return true;
    }

    /**
     * @brief Retrieves the top (minimum) key and priority without removing it.
     */
    [[nodiscard]] const Entry& top() const {
        if (heap_.empty()) {
            throw std::runtime_error("[IndexedPriorityQueue] top() called on empty queue");
        }
        return heap_.front();
    }

    /**
     * @brief Returns the priority of top element, or infinity if empty.
     */
    [[nodiscard]] PriorityType topKey() const {
        if (heap_.empty()) {
            return DStarKey::infinity();
        }
        return heap_.front().priority;
    }

    /**
     * @brief Removes and returns the top (minimum) element.
     */
    Entry pop() {
        if (heap_.empty()) {
            throw std::runtime_error("[IndexedPriorityQueue] pop() called on empty queue");
        }

        Entry root = heap_.front();
        size_t lastIndex = heap_.size() - 1;

        if (lastIndex == 0) {
            position_.erase(root.key);
            heap_.pop_back();
            return root;
        }

        swapNodes(0, lastIndex);
        position_.erase(root.key);
        heap_.pop_back();

        siftDown(0);
        return root;
    }

    /**
     * @brief Looks up the priority of an element by key.
     */
    [[nodiscard]] PriorityType getPriority(const KeyType& key) const {
        auto it = position_.find(key);
        if (it == position_.end()) {
            return DStarKey::infinity();
        }
        return heap_[it->second].priority;
    }
};

} // namespace ssp::algorithms
