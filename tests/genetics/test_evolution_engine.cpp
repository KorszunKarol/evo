#include <gtest/gtest.h>

#include "evolution/genetics/innovation_database.h"
#include "evolution/genetics/mutation_ops.h"
#include "evolution/genetics/morphology_ops.h"
#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/rng.h"
#include "genome_generated.h"

#include <memory>
#include <unordered_set>
#include <filesystem>

namespace evolution::genetics {
namespace {

class InnovationDatabaseTest : public ::testing::Test {
protected:
    InnovationDatabase db{1000, 1};
};

TEST_F(InnovationDatabaseTest, GetOrCreateReturnsConsistentInnovation) {
    const auto innov1 = db.get_or_create(0, 1);
    const auto innov2 = db.get_or_create(0, 1);
    EXPECT_EQ(innov1, innov2);
}

TEST_F(InnovationDatabaseTest, DifferentConnectionsGetDifferentInnovations) {
    const auto innov1 = db.get_or_create(0, 1);
    const auto innov2 = db.get_or_create(0, 2);
    const auto innov3 = db.get_or_create(1, 2);
    
    EXPECT_NE(innov1, innov2);
    EXPECT_NE(innov2, innov3);
    EXPECT_NE(innov1, innov3);
}

TEST_F(InnovationDatabaseTest, NextNodeIdIncrementsCorrectly) {
    const auto id1 = db.next_node_id();
    const auto id2 = db.next_node_id();
    const auto id3 = db.next_node_id();
    
    EXPECT_EQ(id1, 1000);
    EXPECT_EQ(id2, 1001);
    EXPECT_EQ(id3, 1002);
}

TEST_F(InnovationDatabaseTest, GetReturnsZeroForUnknown) {
    EXPECT_EQ(db.get(99, 100), 0);
    
    db.get_or_create(99, 100);
    EXPECT_NE(db.get(99, 100), 0);
}

TEST_F(InnovationDatabaseTest, ClearResetsState) {
    db.get_or_create(0, 1);
    db.next_node_id();
    
    db.clear();
    
    EXPECT_EQ(db.innovation_count(), 0);
    EXPECT_EQ(db.current_node_id(), 1000);
    EXPECT_EQ(db.current_innovation_id(), 1);
}

TEST_F(InnovationDatabaseTest, SaveAndLoadPreservesState) {
    db.get_or_create(0, 1);
    db.get_or_create(1, 2);
    db.get_or_create(0, 2);
    db.next_node_id();
    db.next_node_id();
    
    const auto temp_path = std::filesystem::temp_directory_path() / "innovation_test.bin";
    ASSERT_TRUE(db.save(temp_path));
    
    InnovationDatabase loaded_db;
    ASSERT_TRUE(loaded_db.load(temp_path));
    
    EXPECT_EQ(loaded_db.get(0, 1), db.get(0, 1));
    EXPECT_EQ(loaded_db.get(1, 2), db.get(1, 2));
    EXPECT_EQ(loaded_db.get(0, 2), db.get(0, 2));
    EXPECT_EQ(loaded_db.current_node_id(), db.current_node_id());
    EXPECT_EQ(loaded_db.current_innovation_id(), db.current_innovation_id());
    
    std::filesystem::remove(temp_path);
}

class StructuralMutationTest : public ::testing::Test {
protected:
    void SetUp() override {
        neat_ = std::make_unique<evolution::genome::NEATT>();
        neat_->input_count = 2;
        neat_->output_count = 1;
        neat_->update_rate_hz = 60.0f;
        
        for (std::uint32_t i = 0; i < 2; ++i) {
            auto node = std::make_unique<evolution::genome::NeatNodeT>();
            node->id = i;
            node->type = evolution::genome::NodeType::Input;
            node->bias = 0.0f;
            node->act = evolution::genome::Activation::Linear;
            neat_->nodes.push_back(std::move(node));
        }
        
        auto output_node = std::make_unique<evolution::genome::NeatNodeT>();
        output_node->id = 2;
        output_node->type = evolution::genome::NodeType::Output;
        output_node->bias = 0.0f;
        output_node->act = evolution::genome::Activation::Tanh;
        neat_->nodes.push_back(std::move(output_node));
        
        for (std::uint32_t i = 0; i < 2; ++i) {
            auto conn = std::make_unique<evolution::genome::NeatConnT>();
            conn->in = i;
            conn->out = 2;
            conn->weight = 1.0f;
            conn->enabled = true;
            conn->innovation = innovations_.get_or_create(i, 2);
            conn->recurrent = false;
            neat_->conns.push_back(std::move(conn));
        }
    }
    
    std::unique_ptr<evolution::genome::NEATT> neat_;
    InnovationDatabase innovations_{1000, 100};
    Pcg32 rng_{42};
};

TEST_F(StructuralMutationTest, MutateAddNodeSplitsConnection) {
    const std::size_t initial_nodes = neat_->nodes.size();
    const std::size_t initial_conns = neat_->conns.size();
    
    const auto result = mutate_add_node(*neat_, innovations_, rng_);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(neat_->nodes.size(), initial_nodes + 1);
    EXPECT_EQ(neat_->conns.size(), initial_conns + 2);
    
    bool found_disabled = false;
    for (const auto& conn : neat_->conns) {
        if (conn && !conn->enabled) {
            found_disabled = true;
            break;
        }
    }
    EXPECT_TRUE(found_disabled);
}

TEST_F(StructuralMutationTest, MutateAddConnectionCreatesNewLink) {
    auto hidden = std::make_unique<evolution::genome::NeatNodeT>();
    hidden->id = 1000;
    hidden->type = evolution::genome::NodeType::Hidden;
    hidden->bias = 0.0f;
    hidden->act = evolution::genome::Activation::Tanh;
    neat_->nodes.push_back(std::move(hidden));
    
    const std::size_t initial_conns = neat_->conns.size();
    
    StructuralMutationConfig config{};
    config.allow_recurrent = false;
    
    const auto result = mutate_add_connection(*neat_, innovations_, rng_, config);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(neat_->conns.size(), initial_conns + 1);
}

TEST_F(StructuralMutationTest, MutateDeleteConnectionRemovesLink) {
    const std::size_t initial_conns = neat_->conns.size();
    ASSERT_GT(initial_conns, 0);
    
    const auto result = mutate_delete_connection(*neat_, rng_);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(neat_->conns.size(), initial_conns - 1);
}

TEST_F(StructuralMutationTest, InnovationNumbersAreConsistent) {
    // Verify that the same connection pair always gets the same innovation number
    const auto innov1 = innovations_.get_or_create(0, 1000);
    const auto innov2 = innovations_.get_or_create(0, 1000);
    EXPECT_EQ(innov1, innov2);
    
    // Verify that AddNode creates consistent innovations
    const std::size_t initial_conns = neat_->conns.size();
    const auto result = mutate_add_node(*neat_, innovations_, rng_);
    EXPECT_TRUE(result.success);
    EXPECT_EQ(neat_->conns.size(), initial_conns + 2);
    
    // The new innovations should now be tracked
    for (const auto& conn : neat_->conns) {
        if (conn && conn->innovation >= 100) {
            EXPECT_NE(innovations_.get(conn->in, conn->out), 0);
        }
    }
}

class MorphologyMutationTest : public ::testing::Test {
protected:
    void SetUp() override {
        body_ = std::make_unique<evolution::genome::BodyNodeT>();
        body_->shape = evolution::genome::ShapeType::Sphere;
        body_->mass_density = 500.0f;
        
        body_->size = std::make_unique<evolution::genome::Vec3FT>();
        body_->size->x = 0.3f;
        body_->size->y = 0.3f;
        body_->size->z = 0.3f;
        
        body_->color = std::make_unique<evolution::genome::Vec3FT>();
        body_->color->x = 0.5f;
        body_->color->y = 0.5f;
        body_->color->z = 0.5f;
    }
    
    std::unique_ptr<evolution::genome::BodyNodeT> body_;
    Pcg32 rng_{12345};
    MorphologyConstraints constraints_{};
};

TEST_F(MorphologyMutationTest, CountBodyNodesReturnsCorrect) {
    EXPECT_EQ(count_body_nodes(*body_), 1);
    
    auto child = std::make_unique<evolution::genome::BodyNodeT>();
    child->shape = evolution::genome::ShapeType::CapsuleY;
    body_->children.push_back(std::move(child));
    
    EXPECT_EQ(count_body_nodes(*body_), 2);
}

TEST_F(MorphologyMutationTest, MutateAddLimbIncreasesNodeCount) {
    const std::uint32_t initial_count = count_body_nodes(*body_);
    
    const auto result = mutate_add_limb(*body_, rng_, constraints_);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(count_body_nodes(*body_), initial_count + 1);
}

TEST_F(MorphologyMutationTest, MutateAddLimbRespectsMaxLimbs) {
    constraints_.max_limbs = 2;
    
    mutate_add_limb(*body_, rng_, constraints_);
    const auto result = mutate_add_limb(*body_, rng_, constraints_);
    
    EXPECT_FALSE(result.success);
    EXPECT_EQ(count_body_nodes(*body_), 2);
}

TEST_F(MorphologyMutationTest, MutateModifyLimbChangesProperties) {
    const float initial_density = body_->mass_density;
    const float initial_size_x = body_->size->x;
    
    bool changed = false;
    for (int i = 0; i < 20; ++i) {
        mutate_modify_limb(*body_, rng_, constraints_);
        if (body_->mass_density != initial_density || 
            body_->size->x != initial_size_x) {
            changed = true;
            break;
        }
    }
    
    EXPECT_TRUE(changed);
}

TEST_F(MorphologyMutationTest, MutateRemoveLimbDecreasesNodeCount) {
    mutate_add_limb(*body_, rng_, constraints_);
    ASSERT_EQ(count_body_nodes(*body_), 2);
    
    const auto result = mutate_remove_limb(*body_, rng_);
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(count_body_nodes(*body_), 1);
}

TEST_F(MorphologyMutationTest, MutateRemoveLimbFailsOnRootOnly) {
    const auto result = mutate_remove_limb(*body_, rng_);
    
    EXPECT_FALSE(result.success);
}

class CrossoverTest : public ::testing::Test {
protected:
    void SetUp() override {
        parent_a_.version = 1;
        parent_a_.id = 1;
        parent_a_.generation = 5;
        parent_a_.brain_kind = evolution::genome::BrainKind::NEAT;
        
        parent_a_.body = std::make_unique<evolution::genome::BodyNodeT>();
        parent_a_.body->shape = evolution::genome::ShapeType::Sphere;
        parent_a_.body->mass_density = 500.0f;
        
        parent_a_.neat = std::make_unique<evolution::genome::NEATT>();
        parent_a_.neat->input_count = 2;
        parent_a_.neat->output_count = 1;
        parent_a_.neat->update_rate_hz = 60.0f;
        
        for (std::uint32_t i = 0; i < 2; ++i) {
            auto node = std::make_unique<evolution::genome::NeatNodeT>();
            node->id = i;
            node->type = evolution::genome::NodeType::Input;
            parent_a_.neat->nodes.push_back(std::move(node));
        }
        auto out = std::make_unique<evolution::genome::NeatNodeT>();
        out->id = 2;
        out->type = evolution::genome::NodeType::Output;
        parent_a_.neat->nodes.push_back(std::move(out));
        
        for (std::uint32_t i = 0; i < 2; ++i) {
            auto conn = std::make_unique<evolution::genome::NeatConnT>();
            conn->in = i;
            conn->out = 2;
            conn->weight = 1.0f;
            conn->enabled = true;
            conn->innovation = i + 1;
            parent_a_.neat->conns.push_back(std::move(conn));
        }
        
        parent_b_ = parent_a_;
        parent_b_.id = 2;
        parent_b_.generation = 4;
        
        for (auto& conn : parent_b_.neat->conns) {
            if (conn) {
                conn->weight = -1.0f;
            }
        }
        
        auto extra_conn = std::make_unique<evolution::genome::NeatConnT>();
        extra_conn->in = 0;
        extra_conn->out = 1;
        extra_conn->weight = 0.5f;
        extra_conn->enabled = true;
        extra_conn->innovation = 3;
        parent_b_.neat->conns.push_back(std::move(extra_conn));
    }
    
    evolution::genome::GenomeT parent_a_;
    evolution::genome::GenomeT parent_b_;
    ReproConfig config_{};
};

TEST_F(CrossoverTest, CrossoverProducesValidOffspring) {
    const auto offspring = crossover(parent_a_, parent_b_, config_, 42);
    
    EXPECT_NE(offspring.neat, nullptr);
    EXPECT_GT(offspring.neat->nodes.size(), 0);
    EXPECT_GT(offspring.neat->conns.size(), 0);
}

TEST_F(CrossoverTest, FitterParentDominatesDisjointGenes) {
    const auto offspring = crossover(parent_a_, parent_b_, config_, 42);
    
    std::unordered_set<std::uint32_t> offspring_innovs;
    for (const auto& conn : offspring.neat->conns) {
        if (conn) {
            offspring_innovs.insert(conn->innovation);
        }
    }
    
    EXPECT_TRUE(offspring_innovs.count(1) > 0);
    EXPECT_TRUE(offspring_innovs.count(2) > 0);
}

TEST_F(CrossoverTest, GenerationIsIncremented) {
    const auto offspring = crossover(parent_a_, parent_b_, config_, 42);
    EXPECT_EQ(offspring.generation, 6);
}

}  // namespace
}  // namespace evolution::genetics
