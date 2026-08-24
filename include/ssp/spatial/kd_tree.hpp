#pragma once

#include <vector>
#include <memory>
#include <limits>
#include <algorithm>
#include <queue>
#include "ssp/spatial/vector_math.hpp"
#include "ssp/core/types.hpp"

namespace ssp::spatial {

struct KDPoint {
    uint64_t id{0};
    std::vector<double> coordinates;

    KDPoint() = default;
    KDPoint(uint64_t id, std::vector<double> coords)
        : id(id), coordinates(std::move(coords)) {}
};

struct KDResult {
    uint64_t id{0};
    double distance{core::INF_COST};
};

class KDTree {
private:
    struct Node {
        KDPoint point;
        size_t axis{0};
        std::unique_ptr<Node> left;
        std::unique_ptr<Node> right;

        explicit Node(KDPoint pt, size_t ax)
            : point(std::move(pt)), axis(ax), left(nullptr), right(nullptr) {}
    };

    std::unique_ptr<Node> root_{nullptr};
    size_t dimension_{0};
    size_t size_{0};

    std::unique_ptr<Node> buildRecursive(std::vector<KDPoint>& points, size_t start, size_t end, size_t depth) {
        if (start >= end) return nullptr;

        size_t axis = depth % dimension_;
        size_t mid = start + (end - start) / 2;

        std::nth_element(
            points.begin() + start,
            points.begin() + mid,
            points.begin() + end,
            [axis](const KDPoint& a, const KDPoint& b) {
                double valA = (axis < a.coordinates.size()) ? a.coordinates[axis] : 0.0;
                double valB = (axis < b.coordinates.size()) ? b.coordinates[axis] : 0.0;
                return valA < valB;
            }
        );

        auto node = std::make_unique<Node>(points[mid], axis);
        node->left = buildRecursive(points, start, mid, depth + 1);
        node->right = buildRecursive(points, mid + 1, end, depth + 1);
        return node;
    }

    void nearestRecursive(
        const Node* node,
        const std::vector<double>& query,
        uint64_t& bestId,
        double& bestDistSq
    ) const {
        if (!node) return;

        double currentDistSq = euclideanDistanceSquared(node->point.coordinates, query);
        if (currentDistSq < bestDistSq) {
            bestDistSq = currentDistSq;
            bestId = node->point.id;
        }

        size_t axis = node->axis;
        double queryVal = (axis < query.size()) ? query[axis] : 0.0;
        double nodeVal = (axis < node->point.coordinates.size()) ? node->point.coordinates[axis] : 0.0;
        double diff = queryVal - nodeVal;
        double diffSq = diff * diff;

        const Node* first = (diff < 0) ? node->left.get() : node->right.get();
        const Node* second = (diff < 0) ? node->right.get() : node->left.get();

        nearestRecursive(first, query, bestId, bestDistSq);

        if (diffSq < bestDistSq) {
            nearestRecursive(second, query, bestId, bestDistSq);
        }
    }

    void radiusRecursive(
        const Node* node,
        const std::vector<double>& query,
        double radiusSq,
        std::vector<KDResult>& results
    ) const {
        if (!node) return;

        double currentDistSq = euclideanDistanceSquared(node->point.coordinates, query);
        if (currentDistSq <= radiusSq) {
            results.push_back({node->point.id, std::sqrt(currentDistSq)});
        }

        size_t axis = node->axis;
        double queryVal = (axis < query.size()) ? query[axis] : 0.0;
        double nodeVal = (axis < node->point.coordinates.size()) ? node->point.coordinates[axis] : 0.0;
        double diff = queryVal - nodeVal;
        double diffSq = diff * diff;

        const Node* first = (diff < 0) ? node->left.get() : node->right.get();
        const Node* second = (diff < 0) ? node->right.get() : node->left.get();

        radiusRecursive(first, query, radiusSq, results);

        if (diffSq <= radiusSq) {
            radiusRecursive(second, query, radiusSq, results);
        }
    }

public:
    KDTree() = default;

    void build(std::vector<KDPoint> points) {
        size_ = points.size();
        if (points.empty()) {
            root_ = nullptr;
            dimension_ = 0;
            return;
        }
        dimension_ = points[0].coordinates.size();
        root_ = buildRecursive(points, 0, points.size(), 0);
    }

    [[nodiscard]] size_t size() const { return size_; }
    [[nodiscard]] bool empty() const { return size_ == 0; }

    /**
     * @brief Find nearest neighbor to query vector.
     * @return KDResult with closest point ID and Euclidean distance.
     */
    [[nodiscard]] KDResult nearestNeighbor(const std::vector<double>& query) const {
        if (!root_) {
            return {0, core::INF_COST};
        }
        uint64_t bestId = 0;
        double bestDistSq = core::INF_COST;
        nearestRecursive(root_.get(), query, bestId, bestDistSq);
        return {bestId, std::sqrt(bestDistSq)};
    }

    /**
     * @brief Find all points within radius of query vector.
     */
    [[nodiscard]] std::vector<KDResult> radiusSearch(const std::vector<double>& query, double radius) const {
        std::vector<KDResult> results;
        if (!root_ || radius < 0.0) return results;
        radiusRecursive(root_.get(), query, radius * radius, results);
        return results;
    }
};

} // namespace ssp::spatial
