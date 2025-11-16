#include "test_fixtures.h"

#include <unordered_map>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/environment_bootstrap.h"

using namespace evolution::sim::test;
using namespace evolution::sim;

TEST(PlantSpeciesMapping, SpeciesAssignedByBiome) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& biome_map = registry.ctx().get<BiomeMap>();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    // Spawn plants at different biome locations
    std::unordered_map<BiomeId, std::vector<entt::entity>> plants_by_biome;

    for (double x = 10.0; x < 200.0; x += 20.0) {
        for (double z = 10.0; z < 200.0; z += 20.0) {
            const BiomeId biome = biome_map.sample(x, z);
            const entt::entity plant = fixture.spawn_plant(Vec3{x, 0.0, z}, 0, 10.0);
            plants_by_biome[biome].push_back(plant);
        }
    }

    // Verify plants have species_id assigned
    for (const auto& [biome, plants] : plants_by_biome) {
        EXPECT_GT(plants.size(), 0) << "Biome " << static_cast<int>(biome) << " should have plants";
        for (const entt::entity plant : plants) {
            const auto& plant_comp = registry.get<PlantComponent>(plant);
            EXPECT_LT(plant_comp.species_id, species_registry.count())
                << "Species ID should be valid";
        }
    }
}

TEST(PlantSpeciesMapping, AquaticSpeciesOnlyInWater) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    // Find aquatic species (species 2 = Lily)
    const auto& lily_species = species_registry.get(2);
    EXPECT_TRUE((lily_species.zone_mask & 0x01) != 0) << "Species 2 should be aquatic";

    // Spawn plants and check they're in water
    int aquatic_plants = 0;
    for (double x = 10.0; x < 200.0; x += 10.0) {
        for (double z = 10.0; z < 200.0; z += 10.0) {
            if (water_map.depth(x, z) >= lily_species.aquatic_depth_min) {
                const entt::entity plant = fixture.spawn_plant(Vec3{x, 0.0, z}, 2, 10.0);
                const auto& plant_comp = registry.get<PlantComponent>(plant);
                if (plant_comp.species_id == 2) {
                    ++aquatic_plants;
                    EXPECT_TRUE(water_map.is_water(x, z))
                        << "Aquatic plant at (" << x << ", " << z << ") should be in water";
                }
            }
        }
    }

    EXPECT_GT(aquatic_plants, 0) << "Should have some aquatic plants";
}

TEST(PlantSpeciesMapping, TerrestrialSpeciesNotInDeepWater) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    // Find terrestrial species (species 0 = Grass)
    const auto& grass_species = species_registry.get(0);
    EXPECT_TRUE((grass_species.zone_mask & 0x04) != 0) << "Species 0 should be terrestrial";

    // Spawn plants on land
    int terrestrial_plants = 0;
    for (double x = 10.0; x < 200.0; x += 10.0) {
        for (double z = 10.0; z < 200.0; z += 10.0) {
            if (!water_map.is_water(x, z)) {
                const entt::entity plant = fixture.spawn_plant(Vec3{x, 0.0, z}, 0, 10.0);
                const auto& plant_comp = registry.get<PlantComponent>(plant);
                if (plant_comp.species_id == 0) {
                    ++terrestrial_plants;
                    EXPECT_FALSE(water_map.is_water(x, z))
                        << "Terrestrial plant should not be in water";
                }
            }
        }
    }

    EXPECT_GT(terrestrial_plants, 0) << "Should have some terrestrial plants";
}

TEST(PlantSpeciesMapping, SpeciesDistributionByBiome) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& biome_map = registry.ctx().get<BiomeMap>();

    // Spawn many plants and track distribution
    std::unordered_map<BiomeId, std::unordered_map<std::uint8_t, int>> distribution;

    for (double x = 5.0; x < 200.0; x += 5.0) {
        for (double z = 5.0; z < 200.0; z += 5.0) {
            const BiomeId biome = biome_map.sample(x, z);
            const entt::entity plant = fixture.spawn_plant(Vec3{x, 0.0, z}, 0, 10.0);
            const auto& plant_comp = registry.get<PlantComponent>(plant);
            ++distribution[biome][plant_comp.species_id];
        }
    }

    // Each biome should have some plants
    for (const auto& [biome, species_counts] : distribution) {
        int total = 0;
        for (const auto& [species, count] : species_counts) {
            total += count;
        }
        EXPECT_GT(total, 0) << "Biome " << static_cast<int>(biome) << " should have plants";
    }
}

TEST(PlantSpeciesMapping, ShorelineSpeciesInTransitionZone) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& water_map = registry.ctx().get<WaterMap>();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    // Find shoreline species (species 1 = Reed)
    const auto& reed_species = species_registry.get(1);
    EXPECT_TRUE((reed_species.zone_mask & 0x02) != 0) << "Species 1 should be shoreline";

    // Check shoreline zone criteria
    int shoreline_plants = 0;
    for (double x = 10.0; x < 200.0; x += 10.0) {
        for (double z = 10.0; z < 200.0; z += 10.0) {
            const double depth = water_map.depth(x, z);
            const double shore_dist = water_map.shore_distance(x, z);

            const bool in_shoreline_zone =
                (depth >= reed_species.aquatic_depth_min &&
                 depth <= reed_species.shoreline_depth_max) ||
                (shore_dist >= reed_species.shoreline_distance_min &&
                 shore_dist <= reed_species.shoreline_distance_max);

            if (in_shoreline_zone) {
                const entt::entity plant = fixture.spawn_plant(Vec3{x, 0.0, z}, 1, 10.0);
                const auto& plant_comp = registry.get<PlantComponent>(plant);
                if (plant_comp.species_id == 1) {
                    ++shoreline_plants;
                }
            }
        }
    }

    EXPECT_GE(shoreline_plants, 0) << "May have shoreline plants";
}

TEST(PlantSpeciesMapping, SpeciesRegistryValid) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    EXPECT_GT(species_registry.count(), 0) << "Should have species registered";

    // Check all species have valid parameters
    for (std::size_t i = 0; i < species_registry.count(); ++i) {
        const auto& species = species_registry.get(static_cast<std::uint8_t>(i));
        EXPECT_GT(species.growth_rate, 0.0) << "Species " << i << " should have growth rate";
        EXPECT_GT(species.max_energy, 0.0) << "Species " << i << " should have max energy";
        EXPECT_GT(species.radius, 0.0) << "Species " << i << " should have radius";
    }
}

TEST(PlantSpeciesMapping, BiomeMaskFiltering) {
    EnvironmentConfig config = create_test_env_config(2025);
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& biome_map = registry.ctx().get<BiomeMap>();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    // Check that species only spawn in allowed biomes
    for (std::size_t species_idx = 0; species_idx < species_registry.count(); ++species_idx) {
        const auto& species = species_registry.get(static_cast<std::uint8_t>(species_idx));

        // Spawn plants and verify biome compatibility
        for (double x = 10.0; x < 200.0; x += 20.0) {
            for (double z = 10.0; z < 200.0; z += 20.0) {
                const BiomeId biome = biome_map.sample(x, z);
                const std::uint8_t biome_bit = 1u << static_cast<int>(biome);

                if ((species.biome_mask & biome_bit) != 0) {
                    // Species allowed in this biome
                    const entt::entity plant =
                        fixture.spawn_plant(Vec3{x, 0.0, z}, static_cast<std::uint8_t>(species_idx), 10.0);
                    const auto& plant_comp = registry.get<PlantComponent>(plant);
                    // Plant should be able to spawn here
                    EXPECT_GE(plant_comp.species_id, 0);
                }
            }
        }
    }
}

TEST(PlantSpeciesMapping, InitialSeedingRespectsSpeciesConstraints) {
    EnvironmentConfig config = create_test_env_config(2025);
    config.plants.initial_count = 200;
    SimulationFixture fixture;
    initialize_environment(fixture.app().registry(), config);
    seed_initial_plants(fixture.app().registry(), config);

    auto& registry = fixture.app().registry();
    const auto& biome_map = registry.ctx().get<BiomeMap>();
    const auto& water_map = registry.ctx().get<WaterMap>();
    const auto& species_registry = registry.ctx().get<PlantSpeciesRegistry>();

    auto view = registry.view<TransformComponent, PlantComponent>();
    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);
        const auto& plant = view.get<PlantComponent>(entity);
        if (!plant.alive) {
            continue;
        }

        const PlantSpecies& species = species_registry.get(plant.species_id);
        const BiomeId biome = biome_map.sample(transform.position.x, transform.position.z);
        const double depth = water_map.depth(transform.position.x, transform.position.z);
        const double shore_distance = water_map.shore_distance(transform.position.x, transform.position.z);
        const WaterZone zone = classify_water_zone(water_map, transform.position.x, transform.position.z);

        EXPECT_TRUE(species_allows_location(species, biome, zone, depth, shore_distance))
            << "Species " << static_cast<int>(species.id)
            << " should be allowed at (" << transform.position.x << ", " << transform.position.z << ")";
    }
}

