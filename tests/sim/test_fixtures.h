#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <entt/entt.hpp>
#include <gtest/gtest.h>

#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/components.h"
#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"
#include "evolution/sim/simulation_app.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim::test {

/**
 * @brief Result of snapshot comparison.
 */
enum class SnapshotDiffKind {
    None,
    EntityCountMismatch,
    PlantCountMismatch,
    HerbivoreCountMismatch,
    BiomassMismatch,
    SoilMismatch,
    SpeciesCountMismatch,
    BiomomeBiomassMismatch,
    StateHashMismatch
};

/**
 * @brief Snapshot comparison result with details.
 */
struct SnapshotDiff {
    SnapshotDiffKind kind{SnapshotDiffKind::None};
    std::string message{};
    double expected_value{0.0};
    double actual_value{0.0};
};

/**
 * @brief Test fixture providing deterministic simulation setup.
 */
class SimulationFixture {
public:
    void SetUp();
    void TearDown();

    /**
     * @brief Create a deterministic snapshot of current state.
     */
    struct Snapshot {
        std::size_t entity_count{0};
        std::size_t plant_count{0};
        std::size_t herbivore_count{0};
        double total_biomass{0.0};
        double mean_soil{0.0};
        std::unordered_map<std::uint8_t, std::size_t> species_counts{};
        std::unordered_map<evolution::sim::BiomeId, double> biome_biomass{};
        std::string state_hash{};
    };

public:
    /**
     * @brief Run simulation for specified number of steps.
     */
    void run_steps(std::size_t steps);

    /**
     * @brief Spawn a plant entity at position.
     */
    [[nodiscard]] entt::entity spawn_plant(const Vec3& position,
                                           std::uint8_t species_id = 0,
                                           double energy = 10.0);

    /**
     * @brief Spawn a herbivore entity with genome.
     */
    [[nodiscard]] entt::entity spawn_herbivore(const Vec3& position,
                                                evolution::genetics::GenomeId genome_id);

    /**
     * @brief Create a simple genome and return its ID.
     */
    [[nodiscard]] evolution::genetics::GenomeId create_test_genome(std::uint64_t seed = 12345);

    /**
     * @brief Create a deterministic snapshot of current state.
     */
    [[nodiscard]] Snapshot take_snapshot() const;

    /**
     * @brief Access simulation app.
     */
    [[nodiscard]] SimulationApp& app() { return app_; }
    [[nodiscard]] const SimulationApp& app() const { return app_; }

    /**
     * @brief Access genome storage.
     */
    [[nodiscard]] genetics::GenomeStorage& storage() { return storage_; }
    [[nodiscard]] const genetics::GenomeStorage& storage() const { return storage_; }

private:
    SimulationApp app_;
    genetics::GenomeStorage storage_;
    std::uint64_t global_seed_{98765};
};

/**
 * @brief Helper to create deterministic environment config.
 */
[[nodiscard]] EnvironmentConfig create_test_env_config(std::uint32_t seed = 2025);

/**
 * @brief Compute hash of entity state for determinism checks.
 */
[[nodiscard]] std::string hash_entity_state(entt::registry& registry);

// Snapshot comparison utilities (compare_snapshots, assert_snapshot_matches)
// TODO: Implement after resolving macro compilation issue

}  // namespace evolution::sim::test
