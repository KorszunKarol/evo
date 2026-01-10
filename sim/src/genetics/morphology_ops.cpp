#include "evolution/genetics/morphology_ops.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace evolution::genetics {

namespace {

void CollectNodes(evolution::genome::BodyNodeT& body,
                  std::vector<evolution::genome::BodyNodeT*>& nodes) {
    nodes.push_back(&body);
    for (auto& child : body.children) {
        if (child) {
            CollectNodes(*child, nodes);
        }
    }
}

void CollectNodesWithParent(
    evolution::genome::BodyNodeT& body,
    evolution::genome::BodyNodeT* parent,
    std::vector<std::pair<evolution::genome::BodyNodeT*, evolution::genome::BodyNodeT*>>& nodes) {
    nodes.emplace_back(&body, parent);
    for (auto& child : body.children) {
        if (child) {
            CollectNodesWithParent(*child, &body, nodes);
        }
    }
}

std::uint32_t CountNodesRecursive(const evolution::genome::BodyNodeT& body) {
    std::uint32_t count = 1;
    for (const auto& child : body.children) {
        if (child) {
            count += CountNodesRecursive(*child);
        }
    }
    return count;
}

std::uint32_t GetDepthRecursive(const evolution::genome::BodyNodeT& body, std::uint32_t current) {
    std::uint32_t max_depth = current;
    for (const auto& child : body.children) {
        if (child) {
            max_depth = std::max(max_depth, GetDepthRecursive(*child, current + 1));
        }
    }
    return max_depth;
}

std::unique_ptr<evolution::genome::Vec3FT> CreateVec3(float x, float y, float z) {
    auto vec = std::make_unique<evolution::genome::Vec3FT>();
    vec->x = x;
    vec->y = y;
    vec->z = z;
    return vec;
}

std::unique_ptr<evolution::genome::JointT> CreateRandomJoint(Pcg32& rng) {
    auto joint = std::make_unique<evolution::genome::JointT>();
    
    const double type_roll = rng.next_unit();
    if (type_roll < 0.3) {
        joint->type = evolution::genome::JointType::Fixed;
    } else if (type_roll < 0.7) {
        joint->type = evolution::genome::JointType::Hinge;
    } else {
        joint->type = evolution::genome::JointType::Spherical;
    }
    
    joint->axis = CreateVec3(
        static_cast<float>(rng.normal(0.0, 1.0)),
        static_cast<float>(rng.normal(0.0, 1.0)),
        static_cast<float>(rng.normal(0.0, 1.0)));
    
    const float axis_len = std::sqrt(
        joint->axis->x * joint->axis->x +
        joint->axis->y * joint->axis->y +
        joint->axis->z * joint->axis->z);
    if (axis_len > 0.001f) {
        joint->axis->x /= axis_len;
        joint->axis->y /= axis_len;
        joint->axis->z /= axis_len;
    } else {
        joint->axis->y = 1.0f;
    }
    
    const float limit_range = static_cast<float>(rng.next_unit() * 1.5 + 0.2);
    joint->limits = CreateVec3(-limit_range, limit_range, 0.0f);
    
    joint->anchor = CreateVec3(0.0f, 0.0f, 0.0f);
    
    return joint;
}

}  // namespace

std::uint32_t count_body_nodes(const evolution::genome::BodyNodeT& body) noexcept {
    return CountNodesRecursive(body);
}

std::uint32_t get_body_depth(const evolution::genome::BodyNodeT& body) noexcept {
    return GetDepthRecursive(body, 1);
}

MorphologyResult mutate_add_limb(evolution::genome::BodyNodeT& body,
                                  Pcg32& rng,
                                  const MorphologyConstraints& constraints) noexcept {
    const std::uint32_t current_count = count_body_nodes(body);
    if (current_count >= constraints.max_limbs) {
        return {false, "Maximum limb count reached"};
    }

    std::vector<evolution::genome::BodyNodeT*> nodes;
    CollectNodes(body, nodes);

    std::vector<evolution::genome::BodyNodeT*> eligible;
    for (auto* node : nodes) {
        std::uint32_t node_depth = 1;
        evolution::genome::BodyNodeT* current = &body;
        if (current != node) {
            node_depth = get_body_depth(*node);
        }
        if (node_depth < constraints.max_depth) {
            eligible.push_back(node);
        }
    }

    if (eligible.empty()) {
        return {false, "No eligible parent nodes"};
    }

    evolution::genome::BodyNodeT* parent = eligible[rng.next_u32() % eligible.size()];

    auto new_limb = std::make_unique<evolution::genome::BodyNodeT>();
    
    const double shape_roll = rng.next_unit();
    if (shape_roll < 0.4) {
        new_limb->shape = evolution::genome::ShapeType::CapsuleY;
    } else if (shape_roll < 0.7) {
        new_limb->shape = evolution::genome::ShapeType::Sphere;
    } else {
        new_limb->shape = evolution::genome::ShapeType::Box;
    }

    const double base_size = constraints.min_segment_size +
        rng.next_unit() * (constraints.max_segment_size - constraints.min_segment_size) * 0.5;
    new_limb->size = CreateVec3(
        static_cast<float>(base_size * (0.5 + rng.next_unit())),
        static_cast<float>(base_size * (0.5 + rng.next_unit())),
        static_cast<float>(base_size * (0.5 + rng.next_unit())));

    new_limb->mass_density = static_cast<float>(
        constraints.min_density + 
        rng.next_unit() * (constraints.max_density - constraints.min_density) * 0.5);

    if (parent->color) {
        new_limb->color = CreateVec3(
            parent->color->x + static_cast<float>(rng.normal(0.0, 0.1)),
            parent->color->y + static_cast<float>(rng.normal(0.0, 0.1)),
            parent->color->z + static_cast<float>(rng.normal(0.0, 0.1)));
        new_limb->color->x = std::clamp(new_limb->color->x, 0.0f, 1.0f);
        new_limb->color->y = std::clamp(new_limb->color->y, 0.0f, 1.0f);
        new_limb->color->z = std::clamp(new_limb->color->z, 0.0f, 1.0f);
    } else {
        new_limb->color = CreateVec3(
            static_cast<float>(rng.next_unit()),
            static_cast<float>(rng.next_unit()),
            static_cast<float>(rng.next_unit()));
    }

    new_limb->joint_to_parent = CreateRandomJoint(rng);

    const float offset = parent->size ? parent->size->y * 0.5f : 0.3f;
    new_limb->transform = CreateVec3(
        static_cast<float>(rng.normal(0.0, 0.1)),
        offset,
        static_cast<float>(rng.normal(0.0, 0.1)));

    parent->children.push_back(std::move(new_limb));

    return {true, ""};
}

MorphologyResult mutate_modify_limb(evolution::genome::BodyNodeT& body,
                                     Pcg32& rng,
                                     const MorphologyConstraints& constraints) noexcept {
    std::vector<evolution::genome::BodyNodeT*> nodes;
    CollectNodes(body, nodes);

    if (nodes.empty()) {
        return {false, "No nodes to modify"};
    }

    evolution::genome::BodyNodeT* target = nodes[rng.next_u32() % nodes.size()];

    const double mutation_type = rng.next_unit();

    if (mutation_type < 0.4 && target->size) {
        target->size->x = static_cast<float>(std::clamp(
            static_cast<double>(target->size->x) * (1.0 + rng.normal(0.0, constraints.size_mutation_sigma)),
            constraints.min_segment_size, constraints.max_segment_size));
        target->size->y = static_cast<float>(std::clamp(
            static_cast<double>(target->size->y) * (1.0 + rng.normal(0.0, constraints.size_mutation_sigma)),
            constraints.min_segment_size, constraints.max_segment_size));
        target->size->z = static_cast<float>(std::clamp(
            static_cast<double>(target->size->z) * (1.0 + rng.normal(0.0, constraints.size_mutation_sigma)),
            constraints.min_segment_size, constraints.max_segment_size));
    } else if (mutation_type < 0.7) {
        target->mass_density = static_cast<float>(std::clamp(
            static_cast<double>(target->mass_density) * (1.0 + rng.normal(0.0, constraints.density_mutation_sigma)),
            constraints.min_density, constraints.max_density));
    } else if (target->joint_to_parent && target->joint_to_parent->limits) {
        auto& limits = target->joint_to_parent->limits;
        limits->x = static_cast<float>(
            limits->x + rng.normal(0.0, constraints.joint_limit_mutation_sigma));
        limits->y = static_cast<float>(
            limits->y + rng.normal(0.0, constraints.joint_limit_mutation_sigma));
        
        if (limits->x > limits->y) {
            std::swap(limits->x, limits->y);
        }
        limits->x = std::max(-3.14f, limits->x);
        limits->y = std::min(3.14f, limits->y);
    }

    return {true, ""};
}

MorphologyResult mutate_remove_limb(evolution::genome::BodyNodeT& body,
                                     Pcg32& rng) noexcept {
    std::vector<std::pair<evolution::genome::BodyNodeT*, evolution::genome::BodyNodeT*>> nodes;
    CollectNodesWithParent(body, nullptr, nodes);

    std::vector<std::pair<evolution::genome::BodyNodeT*, evolution::genome::BodyNodeT*>> removable;
    for (const auto& [node, parent] : nodes) {
        if (parent != nullptr) {
            removable.push_back({node, parent});
        }
    }

    if (removable.empty()) {
        return {false, "No removable limbs (only root exists)"};
    }

    const auto& [target, parent] = removable[rng.next_u32() % removable.size()];

    auto it = std::find_if(parent->children.begin(), parent->children.end(),
                           [target](const auto& child) {
                               return child.get() == target;
                           });

    if (it != parent->children.end()) {
        parent->children.erase(it);
        return {true, ""};
    }

    return {false, "Failed to find target in parent's children"};
}

std::uint32_t apply_morphology_mutations(evolution::genome::BodyNodeT& body,
                                          Pcg32& rng,
                                          double add_prob,
                                          double modify_prob,
                                          double remove_prob,
                                          const MorphologyConstraints& constraints) noexcept {
    std::uint32_t mutations_applied = 0;

    if (rng.next_unit() < add_prob) {
        if (mutate_add_limb(body, rng, constraints).success) {
            ++mutations_applied;
        }
    }

    if (rng.next_unit() < modify_prob) {
        if (mutate_modify_limb(body, rng, constraints).success) {
            ++mutations_applied;
        }
    }

    if (rng.next_unit() < remove_prob) {
        if (mutate_remove_limb(body, rng).success) {
            ++mutations_applied;
        }
    }

    return mutations_applied;
}

}  // namespace evolution::genetics

