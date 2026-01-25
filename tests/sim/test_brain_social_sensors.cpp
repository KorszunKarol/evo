#include <gtest/gtest.h>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/sim/brain_io_layout.h"
#include "evolution/sim/brain_inference_system.h"
#include "evolution/sim/components.h"
#include "evolution/sim/simulation_context.h"

namespace evolution::sim {

TEST(BrainInferenceSystem, AppendsSocialSensorsAfterVision) {
    genetics::GenomeStorage storage;

    evolution::genome::GenomeT genome_obj;
    genome_obj.brain_kind = evolution::genome::BrainKind::MLP;
    genome_obj.mlp = std::make_unique<evolution::genome::MLPT>();
    genome_obj.mlp->input_count = static_cast<std::uint32_t>(kTotalInputCount);
    genome_obj.mlp->output_count = 5;
    genome_obj.mlp->hidden_layers.clear();
    genome_obj.mlp->weights.assign(
        static_cast<std::size_t>(genome_obj.mlp->input_count) *
            static_cast<std::size_t>(genome_obj.mlp->output_count),
        0.0F);
    genome_obj.mlp->biases.assign(static_cast<std::size_t>(genome_obj.mlp->output_count), 0.0F);

    const genetics::GenomeId genome_id = storage.insert(std::move(genome_obj));

    entt::registry registry;
    const entt::entity entity = registry.create();

    BrainComponent brain{};
    brain.kind = BrainComponent::Kind::MLP;
    brain.input_count = static_cast<std::uint32_t>(kTotalInputCount);
    brain.output_count = 5;
    brain.update_interval = 0.1;
    registry.emplace<BrainComponent>(entity, brain);
    registry.emplace<ActuationComponent>(entity);
    registry.emplace<GenomeHandleComponent>(entity, GenomeHandleComponent{genome_id});
    registry.emplace<MetabolismComponent>(entity, MetabolismComponent{.energy = 10.0, .max_energy = 10.0});
    registry.emplace<KinematicsComponent>(entity, KinematicsComponent{});
    registry.emplace<TransformComponent>(entity, TransformComponent{.position = Vec3{0.0, 0.0, 0.0}});
    registry.emplace<LifecycleComponent>(entity, LifecycleComponent{});

    VisionComponent vision{};
    vision.ray_count = static_cast<std::uint8_t>(kVisionRayCapacity);
    vision.enabled = true;
    vision.ray_distances = {0.1F, 0.2F, 0.3F, 0.4F, 0.5F};
    vision.ray_hit_types = {VisionHitType::Plant,
                            VisionHitType::Herbivore,
                            VisionHitType::Carnivore,
                            VisionHitType::Terrain,
                            VisionHitType::None};
    vision.ray_hit_entities.assign(vision.ray_distances.size(), entt::null);
    registry.emplace<VisionComponent>(entity, vision);

    SocialSignalsComponent social{};
    social.cohesion_dir = Vec3{0.2, 0.0, 0.4};
    social.alignment_dir = Vec3{0.6, 0.0, 0.8};
    social.separation_dir = Vec3{-0.5, 0.0, -0.25};
    social.neighbor_density = 0.2;
    social.territory_dist_norm = 0.7;
    social.intruder_density = 0.1;
    social.prey_dir = Vec3{1.0, 0.0, 0.0};
    social.pack_density_near_prey = 0.3;
    registry.emplace<SocialSignalsComponent>(entity, social);

    registry.emplace<BrainInspectComponent>(entity);

    BrainInferenceSystem system(storage);
    SimulationContext context(registry, 0.1, 0.0);
    system.tick(context);

    const auto& inspect = registry.get<BrainInspectComponent>(entity);
    const auto& inputs = inspect.input_snapshot;

    const std::size_t vision_start = kBaseSensorCount;
    const std::size_t hit_type_start = vision_start + kVisionRayCapacity;
    const std::size_t social_start = hit_type_start + kVisionRayCapacity;

    ASSERT_GE(inputs.size(), social_start + kSocialSensorCount);

    EXPECT_NEAR(inputs[vision_start + 0], 0.1, 1e-6);
    EXPECT_NEAR(inputs[vision_start + 1], 0.2, 1e-6);
    EXPECT_NEAR(inputs[vision_start + 2], 0.3, 1e-6);
    EXPECT_NEAR(inputs[vision_start + 3], 0.4, 1e-6);
    EXPECT_NEAR(inputs[vision_start + 4], 0.5, 1e-6);

    EXPECT_NEAR(inputs[hit_type_start + 0], 0.25, 1e-6);
    EXPECT_NEAR(inputs[hit_type_start + 1], 0.5, 1e-6);
    EXPECT_NEAR(inputs[hit_type_start + 2], 0.75, 1e-6);
    EXPECT_NEAR(inputs[hit_type_start + 3], 1.0, 1e-6);
    EXPECT_NEAR(inputs[hit_type_start + 4], 0.0, 1e-6);

    EXPECT_NEAR(inputs[social_start + 0], 0.2, 1e-6);
    EXPECT_NEAR(inputs[social_start + 1], 0.4, 1e-6);
    EXPECT_NEAR(inputs[social_start + 2], 0.6, 1e-6);
    EXPECT_NEAR(inputs[social_start + 3], 0.8, 1e-6);
    EXPECT_NEAR(inputs[social_start + 4], -0.5, 1e-6);
    EXPECT_NEAR(inputs[social_start + 5], -0.25, 1e-6);
    EXPECT_NEAR(inputs[social_start + 6], 0.2, 1e-6);
    EXPECT_NEAR(inputs[social_start + 7], 0.7, 1e-6);
    EXPECT_NEAR(inputs[social_start + 8], 0.1, 1e-6);
    EXPECT_NEAR(inputs[social_start + 9], 1.0, 1e-6);
    EXPECT_NEAR(inputs[social_start + 10], 0.0, 1e-6);
    EXPECT_NEAR(inputs[social_start + 11], 0.3, 1e-6);
}

}  // namespace evolution::sim
