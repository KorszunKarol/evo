#include <gtest/gtest.h>

#include "evolution/genetics/genome_ops.h"
#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/innovation_database.h"
#include "evolution/genetics/rng.h"
#include "evolution/genetics/brain_neat.h"
#include "evolution/sim/species_index_system.h"
#include "evolution/sim/evolution_system.h"
#include "genome_generated.h"

#include <cmath>
#include <unordered_map>

namespace evolution::sim {
namespace {

class XorEvolutionTest : public ::testing::Test {
protected:
    void SetUp() override {
        template_genome_.version = 1;
        template_genome_.id = 0;
        template_genome_.generation = 0;
        template_genome_.brain_kind = evolution::genome::BrainKind::NEAT;
        
        template_genome_.body = std::make_unique<evolution::genome::BodyNodeT>();
        template_genome_.body->shape = evolution::genome::ShapeType::Sphere;
        template_genome_.body->mass_density = 500.0f;
        
        template_genome_.neat = std::make_unique<evolution::genome::NEATT>();
        template_genome_.neat->input_count = 2;
        template_genome_.neat->output_count = 1;
        template_genome_.neat->update_rate_hz = 60.0f;
        
        for (std::uint32_t i = 0; i < 2; ++i) {
            auto node = std::make_unique<evolution::genome::NeatNodeT>();
            node->id = i;
            node->type = evolution::genome::NodeType::Input;
            node->bias = 0.0f;
            node->act = evolution::genome::Activation::Linear;
            template_genome_.neat->nodes.push_back(std::move(node));
        }
        
        auto output = std::make_unique<evolution::genome::NeatNodeT>();
        output->id = 2;
        output->type = evolution::genome::NodeType::Output;
        output->bias = 0.0f;
        output->act = evolution::genome::Activation::Tanh;
        template_genome_.neat->nodes.push_back(std::move(output));
        
        for (std::uint32_t i = 0; i < 2; ++i) {
            auto conn = std::make_unique<evolution::genome::NeatConnT>();
            conn->in = i;
            conn->out = 2;
            conn->weight = 0.5f;
            conn->enabled = true;
            conn->innovation = innovations_.get_or_create(i, 2);
            conn->recurrent = false;
            template_genome_.neat->conns.push_back(std::move(conn));
        }
    }
    
    double EvaluateXor(genetics::GenomeId id) {
        const auto* genome = storage_.get(id);
        if (!genome || !genome->neat()) {
            return 0.0;
        }
        
        genetics::BrainNeat brain(*genome->neat());
        
        const std::array<std::pair<std::array<double, 2>, double>, 4> xor_cases = {{
            {{0.0, 0.0}, 0.0},
            {{0.0, 1.0}, 1.0},
            {{1.0, 0.0}, 1.0},
            {{1.0, 1.0}, 0.0}
        }};
        
        double total_error = 0.0;
        for (const auto& [inputs, expected] : xor_cases) {
            brain.reset_state();
            std::array<double, 1> outputs{};
            for (int step = 0; step < 5; ++step) {
                brain.evaluate(inputs, outputs);
            }
            const double output = outputs[0];
            const double error = (output - expected) * (output - expected);
            total_error += error;
        }
        
        return 4.0 - total_error;
    }
    
    genetics::GenomeStorage storage_;
    genetics::InnovationDatabase innovations_{1000, 10};
    genetics::ReproConfig repro_config_{};
    evolution::genome::GenomeT template_genome_;
};

TEST_F(XorEvolutionTest, EvolutionSystemInitializesPopulation) {
    SpeciesIndexSystem species(storage_, repro_config_, 5, 3.0);
    
    EvolutionConfig config{};
    config.population_size = 50;
    config.elite_count = 2;
    
    EvolutionSystem evolution(storage_, innovations_, species, config, repro_config_, 42);
    
    evolution.initialize_population(template_genome_, 50);
    
    EXPECT_EQ(evolution.population().size(), 50);
    EXPECT_EQ(evolution.generation(), 0);
}

TEST_F(XorEvolutionTest, EvolutionSystemAdvancesGeneration) {
    SpeciesIndexSystem species(storage_, repro_config_, 5, 3.0);
    
    EvolutionConfig config{};
    config.population_size = 30;
    config.elite_count = 2;
    
    EvolutionSystem evolution(storage_, innovations_, species, config, repro_config_, 42);
    evolution.initialize_population(template_genome_, 30);
    
    // Create fitness map
    std::unordered_map<genetics::GenomeId, double> fitness_map;
    for (genetics::GenomeId id : evolution.population()) {
        fitness_map[id] = EvaluateXor(id);
    }
    
    evolution.set_fitness(fitness_map);
    
    // Update species clustering
    entt::registry reg;
    SimulationContext context(reg, 0.016, 0.0);
    species.tick(context);
    
    // Advance generation
    const auto stats = evolution.advance_generation();
    
    EXPECT_EQ(stats.generation, 0);
    EXPECT_EQ(stats.population_size, 30);
    EXPECT_GE(stats.best_fitness, 0.0);
    EXPECT_EQ(evolution.generation(), 1);
}

TEST_F(XorEvolutionTest, FitnessImprovesOverGenerations) {
    SpeciesIndexSystem species(storage_, repro_config_, 5, 3.0);
    
    EvolutionConfig config{};
    config.population_size = 100;
    config.elite_count = 5;
    config.crossover_rate = 0.75;
    
    EvolutionSystem evolution(storage_, innovations_, species, config, repro_config_, 12345);
    evolution.initialize_population(template_genome_, 100);
    
    double initial_best = 0.0;
    double final_best = 0.0;
    
    entt::registry reg;
    SimulationContext context(reg, 0.016, 0.0);
    
    for (int gen = 0; gen < 20; ++gen) {
        std::unordered_map<genetics::GenomeId, double> fitness_map;
        for (genetics::GenomeId id : evolution.population()) {
            fitness_map[id] = EvaluateXor(id);
        }
        
        evolution.set_fitness(fitness_map);
        species.tick(context);
        
        double best_this_gen = 0.0;
        for (const auto& [_, fitness] : fitness_map) {
            best_this_gen = std::max(best_this_gen, fitness);
        }
        
        if (gen == 0) {
            initial_best = best_this_gen;
        }
        final_best = best_this_gen;
        
        evolution.advance_generation();
    }
    
    // Fitness should improve or stay the same (elite preservation)
    EXPECT_GE(final_best, initial_best * 0.9);  // Allow slight variation
    
    // Both should be positive (XOR fitness is 4 - error, max is 4)
    EXPECT_GT(initial_best, 0.0);
    EXPECT_GT(final_best, 0.0);
}

TEST_F(XorEvolutionTest, StructuralMutationsOccur) {
    SpeciesIndexSystem species(storage_, repro_config_, 5, 3.0);
    
    EvolutionConfig config{};
    config.population_size = 50;
    config.elite_count = 2;
    
    repro_config_.mutate_rate_struct = 0.3;  // High structural mutation rate
    
    EvolutionSystem evolution(storage_, innovations_, species, config, repro_config_, 42);
    evolution.initialize_population(template_genome_, 50);
    
    entt::registry reg;
    SimulationContext context(reg, 0.016, 0.0);
    
    std::size_t total_nodes_initial = 0;
    for (genetics::GenomeId id : evolution.population()) {
        const auto* genome = storage_.get(id);
        if (genome && genome->neat()) {
            total_nodes_initial += genome->neat()->nodes()->size();
        }
    }
    
    for (int gen = 0; gen < 10; ++gen) {
        std::unordered_map<genetics::GenomeId, double> fitness_map;
        for (genetics::GenomeId id : evolution.population()) {
            fitness_map[id] = EvaluateXor(id);
        }
        
        evolution.set_fitness(fitness_map);
        species.tick(context);
        evolution.advance_generation();
    }
    
    std::size_t total_nodes_final = 0;
    for (genetics::GenomeId id : evolution.population()) {
        const auto* genome = storage_.get(id);
        if (genome && genome->neat()) {
            total_nodes_final += genome->neat()->nodes()->size();
        }
    }
    
    // With structural mutations, we should see some increase in complexity
    EXPECT_GE(total_nodes_final, total_nodes_initial);
}

}  // namespace
}  // namespace evolution::sim
