#include "evolution/genetics/mutation_ops.h"

#include <algorithm>
#include <unordered_set>
#include <vector>

namespace evolution::genetics {

namespace {

struct ConnectionKey {
    std::uint32_t in;
    std::uint32_t out;
    
    bool operator==(const ConnectionKey& other) const noexcept {
        return in == other.in && out == other.out;
    }
};

struct ConnectionKeyHash {
    std::size_t operator()(const ConnectionKey& k) const noexcept {
        return std::hash<std::uint64_t>{}(
            (static_cast<std::uint64_t>(k.in) << 32) | k.out);
    }
};

std::unordered_set<ConnectionKey, ConnectionKeyHash> BuildConnectionSet(
    const evolution::genome::NEATT& neat) {
    std::unordered_set<ConnectionKey, ConnectionKeyHash> existing;
    existing.reserve(neat.conns.size());
    for (const auto& conn : neat.conns) {
        if (conn) {
            existing.insert({conn->in, conn->out});
        }
    }
    return existing;
}

bool WouldCreateCycle(const evolution::genome::NEATT& neat,
                      std::uint32_t src,
                      std::uint32_t dst) {
    std::unordered_set<std::uint32_t> visited;
    std::vector<std::uint32_t> stack;
    stack.push_back(dst);
    
    while (!stack.empty()) {
        const std::uint32_t current = stack.back();
        stack.pop_back();
        
        if (current == src) {
            return true;
        }
        
        if (visited.count(current) > 0) {
            continue;
        }
        visited.insert(current);
        
        for (const auto& conn : neat.conns) {
            if (conn && conn->enabled && conn->in == current && !conn->recurrent) {
                stack.push_back(conn->out);
            }
        }
    }
    
    return false;
}

evolution::genome::NodeType GetNodeType(const evolution::genome::NEATT& neat,
                                         std::uint32_t node_id) {
    for (const auto& node : neat.nodes) {
        if (node && node->id == node_id) {
            return node->type;
        }
    }
    return evolution::genome::NodeType::Hidden;
}

}  // namespace

MutationResult mutate_add_node(evolution::genome::NEATT& neat,
                                InnovationDatabase& innovations,
                                Pcg32& rng) noexcept {
    std::vector<std::size_t> enabled_indices;
    for (std::size_t i = 0; i < neat.conns.size(); ++i) {
        if (neat.conns[i] && neat.conns[i]->enabled) {
            enabled_indices.push_back(i);
        }
    }
    
    if (enabled_indices.empty()) {
        return {false, "No enabled connections to split"};
    }
    
    const std::size_t target_idx = enabled_indices[rng.next_u32() % enabled_indices.size()];
    auto& target_conn = neat.conns[target_idx];
    
    target_conn->enabled = false;
    
    const std::uint32_t new_node_id = innovations.next_node_id();
    const std::uint32_t in_node = target_conn->in;
    const std::uint32_t out_node = target_conn->out;
    const float original_weight = target_conn->weight;
    const bool was_recurrent = target_conn->recurrent;
    
    auto new_node = std::make_unique<evolution::genome::NeatNodeT>();
    new_node->id = new_node_id;
    new_node->type = evolution::genome::NodeType::Hidden;
    new_node->bias = 0.0f;
    new_node->act = evolution::genome::Activation::Tanh;
    neat.nodes.push_back(std::move(new_node));
    
    auto conn_in = std::make_unique<evolution::genome::NeatConnT>();
    conn_in->in = in_node;
    conn_in->out = new_node_id;
    conn_in->weight = 1.0f;
    conn_in->enabled = true;
    conn_in->innovation = innovations.get_or_create(in_node, new_node_id);
    conn_in->recurrent = false;
    neat.conns.push_back(std::move(conn_in));
    
    auto conn_out = std::make_unique<evolution::genome::NeatConnT>();
    conn_out->in = new_node_id;
    conn_out->out = out_node;
    conn_out->weight = original_weight;
    conn_out->enabled = true;
    conn_out->innovation = innovations.get_or_create(new_node_id, out_node);
    conn_out->recurrent = was_recurrent;
    neat.conns.push_back(std::move(conn_out));
    
    return {true, ""};
}

MutationResult mutate_add_connection(evolution::genome::NEATT& neat,
                                      InnovationDatabase& innovations,
                                      Pcg32& rng,
                                      const StructuralMutationConfig& config) noexcept {
    if (neat.nodes.empty()) {
        return {false, "No nodes in genome"};
    }
    
    if (neat.conns.size() >= config.max_connections) {
        return {false, "Maximum connection limit reached"};
    }
    
    const auto existing = BuildConnectionSet(neat);
    
    std::vector<std::uint32_t> node_ids;
    node_ids.reserve(neat.nodes.size());
    for (const auto& node : neat.nodes) {
        if (node) {
            node_ids.push_back(node->id);
        }
    }
    
    constexpr std::size_t kMaxAttempts = 50;
    for (std::size_t attempt = 0; attempt < kMaxAttempts; ++attempt) {
        const std::uint32_t src = node_ids[rng.next_u32() % node_ids.size()];
        const std::uint32_t dst = node_ids[rng.next_u32() % node_ids.size()];
        
        if (src == dst) {
            continue;
        }
        
        const auto src_type = GetNodeType(neat, src);
        const auto dst_type = GetNodeType(neat, dst);
        
        if (dst_type == evolution::genome::NodeType::Input) {
            continue;
        }
        
        if (existing.count({src, dst}) > 0) {
            continue;
        }
        
        bool is_recurrent = false;
        if (!config.allow_recurrent) {
            if (WouldCreateCycle(neat, src, dst)) {
                continue;
            }
        } else {
            is_recurrent = WouldCreateCycle(neat, src, dst);
        }
        
        auto new_conn = std::make_unique<evolution::genome::NeatConnT>();
        new_conn->in = src;
        new_conn->out = dst;
        new_conn->weight = static_cast<float>(rng.normal(0.0, config.weight_init_sigma));
        new_conn->enabled = true;
        new_conn->innovation = innovations.get_or_create(src, dst);
        new_conn->recurrent = is_recurrent;
        neat.conns.push_back(std::move(new_conn));
        
        return {true, ""};
    }
    
    return {false, "Could not find valid connection after max attempts"};
}

MutationResult mutate_delete_connection(evolution::genome::NEATT& neat,
                                         Pcg32& rng) noexcept {
    if (neat.conns.empty()) {
        return {false, "No connections to delete"};
    }
    
    std::vector<std::size_t> enabled_indices;
    for (std::size_t i = 0; i < neat.conns.size(); ++i) {
        if (neat.conns[i] && neat.conns[i]->enabled) {
            enabled_indices.push_back(i);
        }
    }
    
    if (enabled_indices.empty()) {
        return {false, "No enabled connections to delete"};
    }
    
    const std::size_t target_idx = enabled_indices[rng.next_u32() % enabled_indices.size()];
    neat.conns.erase(neat.conns.begin() + static_cast<std::ptrdiff_t>(target_idx));
    
    return {true, ""};
}

MutationResult mutate_toggle_connection(evolution::genome::NEATT& neat,
                                         Pcg32& rng) noexcept {
    if (neat.conns.empty()) {
        return {false, "No connections to toggle"};
    }
    
    const std::size_t idx = rng.next_u32() % neat.conns.size();
    if (neat.conns[idx]) {
        neat.conns[idx]->enabled = !neat.conns[idx]->enabled;
        return {true, ""};
    }
    
    return {false, "Selected connection is null"};
}

std::uint32_t apply_structural_mutations(evolution::genome::NEATT& neat,
                                          InnovationDatabase& innovations,
                                          Pcg32& rng,
                                          const StructuralMutationConfig& config) noexcept {
    std::uint32_t mutations_applied = 0;
    
    std::size_t hidden_count = 0;
    for (const auto& node : neat.nodes) {
        if (node && node->type == evolution::genome::NodeType::Hidden) {
            ++hidden_count;
        }
    }
    
    if (hidden_count < config.max_hidden_nodes && rng.next_unit() < config.add_node_prob) {
        if (mutate_add_node(neat, innovations, rng).success) {
            ++mutations_applied;
        }
    }
    
    if (rng.next_unit() < config.add_conn_prob) {
        if (mutate_add_connection(neat, innovations, rng, config).success) {
            ++mutations_applied;
        }
    }
    
    if (rng.next_unit() < config.delete_conn_prob) {
        if (mutate_delete_connection(neat, rng).success) {
            ++mutations_applied;
        }
    }
    
    return mutations_applied;
}

}  // namespace evolution::genetics

