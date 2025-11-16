/**
 * @file genome_storage.h
 * @brief In-memory storage and creation utilities for serialized genomes.
 */

#pragma once

#include <span>
#include <unordered_map>
#include <vector>

#include <flatbuffers/flatbuffers.h>

#include "evolution/genetics/genome_types.h"
#include "evolution/genetics/rng.h"
#include "genome_generated.h"

namespace evolution::genetics {

/**
 * @brief Hash serialized genome buffers using 64-bit FNV-1a.
 * @param buffer Byte span representing a serialized genome (id field zeroed).
 * @return Deterministic 64-bit hash value.
 * @throws None.
 * @complexity O(N) where N = buffer.size().
 * @note Used internally by GenomeStorage to derive GenomeId values.
 * @warning Buffer must correspond to a valid genome schema; no validation performed.
 * @threadsafe @notthreadsafe Caller must serialize/halt concurrently for deterministic behaviour.
 */
[[nodiscard]] GenomeId HashGenomeBuffer(std::span<const std::uint8_t> buffer) noexcept;

/**
 * @brief Manage genome serialization buffers and deterministic creation routines.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity Construction is O(1); storage operations vary as documented per method.
 * @note Currently stores genomes solely in-memory; persistence hooks arrive in later milestones.
 * @warning Not thread-safe; synchronize externally if accessed from multiple threads.
 * @threadsafe @notthreadsafe.
 */
class GenomeStorage {
public:
    /**
     * @brief Default constructor for empty storage.
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(1).
     * @note Initializes deterministic RNG with zero seed; reseed via create_random inputs.
     * @warning Storage remains empty until insert/create_random invoked.
     * @threadsafe @notthreadsafe.
     */
    GenomeStorage() = default;

    /**
     * @brief Create and insert a random seed genome using the provided seed.
     * @param seed Deterministic seed applied to genome synthesis.
     * @return Stable GenomeId assigned to the inserted genome.
     * @throws None.
     * @complexity O(M) where M equals serialized genome size.
     * @note Generates both body and brain content with bounded parameter ranges.
     * @warning Seed collisions still yield distinct genomes due to hashing; do not assume id == seed.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] GenomeId create_random(std::uint64_t seed);

    /**
     * @brief Insert an already-constructed genome object and materialize storage buffers.
     * @param genome_obj Object-based genome representation (id optional; recomputed when zero).
     * @return Stable GenomeId stored within the serialized buffer.
     * @throws None.
     * @complexity O(M) serialization plus O(log N) average hash map insertion.
     * @note Reuses existing genomes when an identical hash is already present.
     * @warning Passing conflicting ids triggers replacement of the stored id with recomputed hash.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] GenomeId insert(evolution::genome::GenomeT genome_obj);

    /**
     * @brief Access a stored genome by identifier.
     * @param id Genome identifier previously returned by create_random or insert.
     * @return Pointer to immutable Genome table; nullptr when id missing.
     * @throws None.
     * @complexity O(1) average hash lookup.
     * @note Returned pointer remains valid until the storage mutates (insert/erase).
     * @warning Do not retain pointer beyond storage lifetime.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] const evolution::genome::Genome* get(GenomeId id) const noexcept;

    /**
     * @brief Check whether the storage contains a genome identifier.
     * @param id Genome identifier to query.
     * @return true when present, false otherwise.
     * @throws None.
     * @complexity O(1) average.
     * @note Utility for deterministic reproduction pipelines.
     * @warning Unchecked ids may correspond to stale handles after manual erasure (future feature).
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] bool contains(GenomeId id) const noexcept;

    /**
     * @brief Retrieve ordered view of all genome identifiers.
     * @param None.
     * @return Vector copy of insertion-ordered identifiers.
     * @throws None.
     * @complexity O(N) to copy where N equals genome count.
     * @note Preserves deterministic ordering for telemetry dumps.
     * @warning Copying can be expensive for large populations; prefer iterating via callback (future API).
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] std::vector<GenomeId> ids() const;

    /**
     * @brief Persist all genomes to storage (stub for future disk snapshots).
     * @param None.
     * @return None.
     * @throws None.
     * @complexity O(1) currently (no-op).
     * @note Placeholder enabling milestone parity with design doc.
     * @warning Implementation intentionally empty; expect future behaviour.
     * @threadsafe @notthreadsafe.
     */
    void save_all() const noexcept;

private:
    using Buffer = flatbuffers::DetachedBuffer;

    [[nodiscard]] GenomeId insert_buffer(Buffer buffer);

    std::unordered_map<GenomeId, Buffer> genomes_{};
    std::vector<GenomeId> insertion_order_{};
};

}  // namespace evolution::genetics

