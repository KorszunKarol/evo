#include <gtest/gtest.h>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/brain_io_layout.h"
#include "evolution/sim/components.h"

namespace evolution::sim {

TEST(PhenotypeBuilder, ClampsVisionRayCountToCapacity) {
    genetics::GenomeStorage storage;

    evolution::genome::GenomeT genome_obj;
    genome_obj.brain_kind = evolution::genome::BrainKind::MLP;
    genome_obj.mlp = std::make_unique<evolution::genome::MLPT>();
    genome_obj.mlp->input_count = 1;
    genome_obj.mlp->output_count = 1;
    genome_obj.mlp->weights.assign(1U, 0.0F);
    genome_obj.mlp->biases.assign(1U, 0.0F);

    genome_obj.body = std::make_unique<evolution::genome::BodyNodeT>();
    genome_obj.body->shape = evolution::genome::ShapeType::Box;
    auto size = std::make_unique<evolution::genome::Vec3FT>();
    size->x = 0.5f;
    size->y = 0.5f;
    size->z = 0.5f;
    genome_obj.body->size = std::move(size);
    genome_obj.body->mass_density = 100.0f;

    genome_obj.sensory = std::make_unique<evolution::genome::SensoryTraitsT>();
    genome_obj.sensory->vision_rays = 8;

    const auto genome_id = storage.insert(std::move(genome_obj));

    entt::registry registry;
    const entt::entity entity = registry.create();
    const auto build_result = genetics::PhenotypeBuilder::build(genome_id, registry, entity, storage);
    ASSERT_TRUE(build_result.ok) << build_result.msg;

    const auto& vision = registry.get<VisionComponent>(entity);
    EXPECT_EQ(vision.ray_count, static_cast<std::uint8_t>(kVisionRayCapacity));
}

}  // namespace evolution::sim
