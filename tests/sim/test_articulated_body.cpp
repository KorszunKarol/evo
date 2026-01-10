#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/phenotype_builder.h"
#include "evolution/sim/components.h"
#include "evolution/sim/physics_system.h"
#include "evolution/sim/physics/simple_backend.h"
#include "evolution/sim/simulation_context.h"
#include "test_fixtures.h"

namespace evolution::sim::test {

class ArticulatedBodyTest : public testing::Test, public SimulationFixture {
protected:
    void SetUp() override {
        SimulationFixture::SetUp();
        // Register physics
        SimplePhysicsConfig phys_config;
        auto backend = std::make_unique<SimplePhysicsBackend>(phys_config);
        app().scheduler().add_system(std::make_unique<PhysicsSystem>(std::move(backend)));
    }
    void TearDown() override {
        SimulationFixture::TearDown();
    }
};

TEST_F(ArticulatedBodyTest, BuildAndSimulateArticulatedCreature) {
    auto& registry = app().registry();
    auto& storage = this->storage();

    // 1. Construct Genome with Articulated Body
    evolution::genome::GenomeT genome_obj;
    genome_obj.id = 123;
    genome_obj.brain_kind = evolution::genome::BrainKind::MLP;
    genome_obj.mlp = std::make_unique<evolution::genome::MLPT>();
    genome_obj.mlp->input_count = 1;
    genome_obj.mlp->output_count = 1;
    
    // Root Body
    genome_obj.body = std::make_unique<evolution::genome::BodyNodeT>();
    genome_obj.body->shape = evolution::genome::ShapeType::Box;
    auto root_size = std::make_unique<evolution::genome::Vec3FT>();
    root_size->x = 0.5f; root_size->y = 0.5f; root_size->z = 0.5f;
    genome_obj.body->size = std::move(root_size);
    genome_obj.body->mass_density = 100.0f;
    
    // Child Body
    auto child_node = std::make_unique<evolution::genome::BodyNodeT>();
    child_node->shape = evolution::genome::ShapeType::Sphere;
    auto child_size = std::make_unique<evolution::genome::Vec3FT>();
    child_size->x = 0.3f; child_size->y = 0.3f; child_size->z = 0.3f;
    child_node->size = std::move(child_size);
    child_node->mass_density = 100.0f;
    
    auto child_transform = std::make_unique<evolution::genome::Vec3FT>();
    child_transform->x = 1.0f; child_transform->y = 0.0f; child_transform->z = 0.0f;
    child_node->transform = std::move(child_transform);
    
    // Joint
    child_node->joint_to_parent = std::make_unique<evolution::genome::JointT>();
    child_node->joint_to_parent->type = evolution::genome::JointType::Hinge;
    
    auto anchor = std::make_unique<evolution::genome::Vec3FT>();
    anchor->x = 0.0f; anchor->y = 0.0f; anchor->z = 0.0f;
    child_node->joint_to_parent->anchor = std::move(anchor);
    
    auto axis = std::make_unique<evolution::genome::Vec3FT>();
    axis->x = 0.0f; axis->y = 0.0f; axis->z = 1.0f;
    child_node->joint_to_parent->axis = std::move(axis);
    
    genome_obj.body->children.push_back(std::move(child_node));

    // 2. Store Genome
    const genetics::GenomeId genome_id = storage.insert(std::move(genome_obj));

    // 3. Build Phenotype
    const entt::entity root_entity = registry.create();
    const auto result = genetics::PhenotypeBuilder::build(genome_id, registry, root_entity, storage);
    ASSERT_TRUE(result.ok) << result.msg;

    // 4. Verify Entities
    ASSERT_TRUE(registry.valid(root_entity));
    ASSERT_TRUE((registry.all_of<TransformComponent, RigidbodyComponent>(root_entity)));
    
    // Find child entity
    entt::entity child_entity = entt::null;
    auto view = registry.view<JointComponent>();
    int joint_count = 0;
    for (auto entity : view) {
        const auto& joint = view.get<JointComponent>(entity);
        if (joint.parent == root_entity) {
            child_entity = entity;
            joint_count++;
            
            EXPECT_EQ(joint.child, entity);
            EXPECT_EQ(joint.type, JointType::Hinge);
            // Check anchor positions (approx)
            EXPECT_NEAR(joint.local_anchor_parent.x, 1.0, 1e-5);
        }
    }
    EXPECT_EQ(joint_count, 1);
    EXPECT_FALSE(child_entity == entt::null);
    EXPECT_NE(child_entity, root_entity);

    // 5. Simulate Physics
    // Root at 0,0,0. Child at 1,0,0.
    // Gravity is -9.81 Y.
    // Child should swing down.
    
    // Run a few steps
    run_steps(10);
    
    // Verify they are still connected (distance constraint)
    const auto& root_trans = registry.get<TransformComponent>(root_entity);
    const auto& child_trans = registry.get<TransformComponent>(child_entity);
    
    // Distance should be roughly 1.0 (length of arm)
    double dist = std::sqrt(std::pow(root_trans.position.x - child_trans.position.x, 2) +
                            std::pow(root_trans.position.y - child_trans.position.y, 2) +
                            std::pow(root_trans.position.z - child_trans.position.z, 2));
                            
    EXPECT_NEAR(dist, 1.0, 0.1) << "Joint constraint failed to maintain distance";
    
    // Verify child moved (fell due to gravity)
    EXPECT_LT(child_trans.position.y, -0.01) << "Child did not fall under gravity";
}

}  // namespace evolution::sim::test
