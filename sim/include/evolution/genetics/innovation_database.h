/**
 * @file innovation_database.h
 * @brief Global innovation number tracking for NEAT structural mutations.
 */

#pragma once

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <unordered_map>

namespace evolution::genetics {

/**
 * @brief Hash function for (in_node, out_node) pairs.
 */
struct ConnectionHash {
    std::size_t operator()(const std::pair<std::uint32_t, std::uint32_t>& p) const noexcept {
        return std::hash<std::uint64_t>{}(
            (static_cast<std::uint64_t>(p.first) << 32) | p.second);
    }
};

/**
 * @brief Central registry tracking global innovation numbers for NEAT.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) average for get_or_create operations.
 * @note Maps (InNode, OutNode) pairs to unique innovation IDs, ensuring that
 *       independent evolutions of the same structural change remain compatible
 *       for crossover alignment.
 * @warning Not thread-safe; external synchronization required for concurrent access.
 * @threadsafe @notthreadsafe.
 */
class InnovationDatabase {
public:
    /**
     * @brief Construct empty innovation database.
     * @param initial_node_id Starting ID for new hidden nodes (default 1000 to reserve for I/O).
     * @param initial_innovation_id Starting innovation number.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Node IDs 0-999 are reserved for input/output nodes by convention.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    explicit InnovationDatabase(std::uint32_t initial_node_id = 1000,
                                 std::uint32_t initial_innovation_id = 1) noexcept;

    /**
     * @brief Get or create innovation number for a connection.
     * @param in_node Source node ID.
     * @param out_node Target node ID.
     * @return Innovation number for this connection (existing or newly assigned).
     * @throws None.
     * @complexity O(1) average.
     * @note Creates new innovation number if pair not previously seen.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t get_or_create(std::uint32_t in_node,
                                               std::uint32_t out_node) noexcept;

    /**
     * @brief Check if an innovation exists for the given connection.
     * @param in_node Source node ID.
     * @param out_node Target node ID.
     * @return Innovation number if exists, 0 otherwise.
     * @throws None.
     * @complexity O(1) average.
     * @note Returns 0 for unknown connections.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t get(std::uint32_t in_node,
                                     std::uint32_t out_node) const noexcept;

    /**
     * @brief Allocate next available node ID for AddNode mutations.
     * @param None.
     * @return Unique node ID.
     * @throws None.
     * @complexity O(1).
     * @note Each call returns a new unique ID.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t next_node_id() noexcept;

    /**
     * @brief Get current node ID counter without incrementing.
     * @param None.
     * @return Current next node ID value.
     * @throws None.
     * @complexity O(1).
     * @note Useful for serialization.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t current_node_id() const noexcept { return next_node_id_; }

    /**
     * @brief Get current innovation counter without incrementing.
     * @param None.
     * @return Current next innovation ID value.
     * @throws None.
     * @complexity O(1).
     * @note Useful for serialization.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::uint32_t current_innovation_id() const noexcept { return next_innovation_id_; }

    /**
     * @brief Reset generation-local innovations (optional NEAT variant).
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(N) where N is number of innovations.
     * @note Some NEAT implementations reset innovations each generation.
     *       This clears the connection map but preserves counters.
     * @warning Use with caution; affects crossover compatibility.
     * @threadsafe @notthreadsafe.
     */
    void reset_generation() noexcept;

    /**
     * @brief Clear all data and reset counters.
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(N).
     * @note Full reset for new simulation runs.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    void clear() noexcept;

    /**
     * @brief Get total number of registered innovations.
     * @param None.
     * @return Count of unique connection innovations.
     * @throws None.
     * @complexity O(1).
     * @note None.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::size_t innovation_count() const noexcept {
        return connection_innovations_.size();
    }

    /**
     * @brief Serialize database to file.
     * @param path Output file path.
     * @return True on success, false on failure.
     * @throws None.
     * @complexity O(N).
     * @note Binary format: node_id, innovation_id, then (in, out, innov) triplets.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] bool save(const std::filesystem::path& path) const noexcept;

    /**
     * @brief Deserialize database from file.
     * @param path Input file path.
     * @return True on success, false on failure.
     * @throws None.
     * @complexity O(N).
     * @note Replaces current state with loaded data.
     * @warning None.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] bool load(const std::filesystem::path& path) noexcept;

private:
    using ConnectionKey = std::pair<std::uint32_t, std::uint32_t>;
    std::unordered_map<ConnectionKey, std::uint32_t, ConnectionHash> connection_innovations_;
    std::uint32_t next_innovation_id_;
    std::uint32_t next_node_id_;
};

}  // namespace evolution::genetics

