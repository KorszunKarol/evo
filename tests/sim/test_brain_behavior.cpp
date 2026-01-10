/**
 * @file test_brain_behavior.cpp
 * @brief Verification tests for Neural Network (NEAT) behavior and determinism.
 */

#include <gtest/gtest.h>
#include <vector>
#include <cmath>
#include <flatbuffers/flatbuffers.h>

#include "evolution/genetics/brain_neat.h"
#include "evolution/genetics/genome_types.h"
#include "genome_generated.h" 

using namespace evolution::genetics;
using namespace evolution::genome;

// Helper to create a simple manual NEAT graph
// Input 0 -> Hidden 0 -> Output 0
flatbuffers::DetachedBuffer CreateMockNeatGraph(flatbuffers::FlatBufferBuilder& fbb) {
    // 1. Create Nodes (NeatNode)
    std::vector<flatbuffers::Offset<NeatNode>> nodes_vec;
    
    // Input 0 (ID 0)
    nodes_vec.push_back(CreateNeatNode(fbb, 0, NodeType::Input, 0.0, Activation::Linear));
    // Hidden 0 (ID 2) -- Must come before Output for 1-pass propagation
    nodes_vec.push_back(CreateNeatNode(fbb, 2, NodeType::Hidden, 0.0, Activation::Tanh));
    // Output 0 (ID 1)
    nodes_vec.push_back(CreateNeatNode(fbb, 1, NodeType::Output, 0.0, Activation::Tanh));

    auto nodes = fbb.CreateVector(nodes_vec);

    // 2. Create Connections (NeatConn)
    std::vector<flatbuffers::Offset<NeatConn>> conns_vec;
    
    // Input 0 -> Hidden 0 (Weight 1.0)
    // Args: fbb, in, out, weight, enabled, innovation, recurrent
    conns_vec.push_back(CreateNeatConn(fbb, 0, 2, 1.0, true, 0, false));
    // Hidden 0 -> Output 0 (Weight 1.0)
    conns_vec.push_back(CreateNeatConn(fbb, 2, 1, 1.0, true, 0, false));

    auto connections = fbb.CreateVector(conns_vec);

    // 3. Create NEAT table
    NEATBuilder neat_builder(fbb);
    neat_builder.add_nodes(nodes);
    neat_builder.add_conns(connections);
    neat_builder.add_input_count(1);
    neat_builder.add_output_count(1);
    auto neat = neat_builder.Finish();
    
    fbb.Finish(neat);
    return fbb.Release();
}

TEST(BrainBehavior, ForwardPassDeterminism) {
    flatbuffers::FlatBufferBuilder fbb;
    auto buffer = CreateMockNeatGraph(fbb);
    auto* neat_ptr = flatbuffers::GetRoot<NEAT>(buffer.data());

    BrainNeat brain(*neat_ptr);

    std::vector<double> inputs = {1.0};
    std::vector<double> outputs(1);

    // Run forward pass
    // Input(1.0) -> Hidden(Tanh(1.0*1.0)) -> Output(Tanh(Hidden*1.0))
    // Tanh(1.0) ~= 0.76159
    // Output ~= Tanh(0.76159) ~= 0.642
    brain.evaluate(inputs, outputs);

    EXPECT_NEAR(outputs[0], 0.642, 0.01);
    
    // Run again with same input, should be identical (no recurrence in this graph)
    brain.evaluate(inputs, outputs);
    EXPECT_NEAR(outputs[0], 0.642, 0.01);
}

TEST(BrainBehavior, InputSensitivity) {
    flatbuffers::FlatBufferBuilder fbb;
    auto buffer = CreateMockNeatGraph(fbb);
    auto* neat_ptr = flatbuffers::GetRoot<NEAT>(buffer.data());
    BrainNeat brain(*neat_ptr);

    std::vector<double> inputs_zero = {0.0};
    std::vector<double> outputs(1);
    
    // Input 0.0 -> Hidden 0.0 -> Output 0.0
    brain.evaluate(inputs_zero, outputs);
    EXPECT_NEAR(outputs[0], 0.0, 0.001);

    std::vector<double> inputs_neg = {-1.0};
    brain.evaluate(inputs_neg, outputs);
    // Tanh(-1) is negative, output should be negative
    EXPECT_LT(outputs[0], -0.1);
}
