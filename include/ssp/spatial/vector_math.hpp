#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>
#include <numeric>
#include <algorithm>

namespace ssp::spatial {

/**
 * @brief Squared Euclidean distance between two d-dimensional vectors.
 * Avoids square root for fast branch-and-bound comparisons.
 */
inline double euclideanDistanceSquared(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.empty() || b.empty()) return 0.0;
    size_t dim = std::min(a.size(), b.size());
    double sum = 0.0;
    for (size_t i = 0; i < dim; ++i) {
        double diff = a[i] - b[i];
        sum += diff * diff;
    }
    // Penalize dimension mismatch if any
    if (a.size() != b.size()) {
        const auto& larger = (a.size() > b.size()) ? a : b;
        for (size_t i = dim; i < larger.size(); ++i) {
            sum += larger[i] * larger[i];
        }
    }
    return sum;
}

/**
 * @brief True Euclidean distance (L2 norm) between two vectors.
 */
inline double euclideanDistance(const std::vector<double>& a, const std::vector<double>& b) {
    return std::sqrt(euclideanDistanceSquared(a, b));
}

/**
 * @brief Manhattan distance (L1 norm) between two vectors.
 */
inline double manhattanDistance(const std::vector<double>& a, const std::vector<double>& b) {
    if (a.empty() || b.empty()) return 0.0;
    size_t dim = std::min(a.size(), b.size());
    double sum = 0.0;
    for (size_t i = 0; i < dim; ++i) {
        sum += std::abs(a[i] - b[i]);
    }
    return sum;
}

/**
 * @brief Vector dot product.
 */
inline double dotProduct(const std::vector<double>& a, const std::vector<double>& b) {
    size_t dim = std::min(a.size(), b.size());
    double sum = 0.0;
    for (size_t i = 0; i < dim; ++i) {
        sum += a[i] * b[i];
    }
    return sum;
}

/**
 * @brief Vector L2 norm (magnitude).
 */
inline double vectorNorm(const std::vector<double>& a) {
    return std::sqrt(dotProduct(a, a));
}

/**
 * @brief Cosine similarity in [-1.0, 1.0].
 */
inline double cosineSimilarity(const std::vector<double>& a, const std::vector<double>& b) {
    double normA = vectorNorm(a);
    double normB = vectorNorm(b);
    if (normA < 1e-12 || normB < 1e-12) return 0.0;
    return dotProduct(a, b) / (normA * normB);
}

/**
 * @brief Cosine distance = 1.0 - cosineSimilarity.
 */
inline double cosineDistance(const std::vector<double>& a, const std::vector<double>& b) {
    return 1.0 - cosineSimilarity(a, b);
}

} // namespace ssp::spatial
