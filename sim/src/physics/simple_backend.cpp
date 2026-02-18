#include "evolution/sim/physics/simple_backend.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_set>

namespace evolution::sim {

namespace {

[[nodiscard]] Vec3 make_gravity_vector(double gravity_y) noexcept {
    return Vec3{0.0, gravity_y, 0.0};
}

[[nodiscard]] double mix_friction(double a, double b) noexcept {
    return std::sqrt(std::max(0.0, a) * std::max(0.0, b));
}

[[nodiscard]] double mix_restitution(double a, double b) noexcept {
    return std::max(a, b);
}

[[nodiscard]] std::uint64_t make_contact_key(entt::entity a, entt::entity b) noexcept {
    const auto id_a = static_cast<std::uint64_t>(entt::to_integral(a));
    const auto id_b = static_cast<std::uint64_t>(entt::to_integral(b));
    const std::uint64_t min_id = std::min(id_a, id_b);
    const std::uint64_t max_id = std::max(id_a, id_b);
    return (min_id << 32ULL) | max_id;
}

[[nodiscard]] PhysicsMaterial resolve_material(const ColliderComponent& collider,
                                               const KinematicsComponent* kinematics) noexcept {
    PhysicsMaterial material = collider.material;
    if (kinematics) {
        material.friction = collider.material.friction;
        material.restitution = collider.material.restitution;
    }
    return material;
}

}  // namespace

SimplePhysicsBackend::SimplePhysicsBackend(const SimplePhysicsConfig& config)
    : config_(config),
      gravity_(make_gravity_vector(config.gravity)),
      spatial_hash_() {
    configure(config.core);
}

void SimplePhysicsBackend::configure(const Config& config) {
    config_.core = config;
    spatial_hash_ = SpatialHash(config_.core.cell_size);
}

void SimplePhysicsBackend::sync_from_registry(entt::registry& registry) {
    terrain_ = nullptr;
    if (config_.enable_heightfield) {
        terrain_ = registry.ctx().find<Terrain>();
    }
    rebuild_body_records(registry);
    generate_broad_phase_pairs();
    stats_.bodies = bodies_.size();
    stats_.pairs = candidate_pairs_.size();
}

void SimplePhysicsBackend::step(entt::registry& registry, double dt) {
    run_narrow_phase();
    stats_.contacts = 0;
    for (const auto& manifold : manifolds_) {
        stats_.contacts += manifold.count;
    }
    stats_.solver_iterations = config_.core.solver_iterations;

    integrate_forces(dt);
    solve_velocity_constraints(std::span<ContactManifold>(manifolds_.data(), manifolds_.size()),
                               registry,
                               config_.core.solver_iterations,
                               dt);
    integrate_velocities(dt);
    solve_position_constraints(std::span<ContactManifold>(manifolds_.data(), manifolds_.size()),
                               registry,
                               config_.core.baumgarte,
                               config_.core.penetration_slop);
    dispatch_contact_events();

    // Refresh lookup positions after integration for next tick.
    for (auto& body : bodies_) {
        if (body.transform) {
            body.position = body.transform->position;
        }
    }
}

std::optional<IPhysicsBackend::ContactEvent> SimplePhysicsBackend::raycast(const Vec3& origin,
                                                                           const Vec3& direction,
                                                                           double max_distance) const {
    (void)origin;
    (void)direction;
    (void)max_distance;
    return std::nullopt;
}

std::span<const IPhysicsBackend::ContactEvent> SimplePhysicsBackend::contact_events() const {
    return contact_events_;
}

IPhysicsBackend::Stats SimplePhysicsBackend::stats() const {
    return stats_;
}

void SimplePhysicsBackend::set_gravity(const Vec3& gravity) noexcept {
    gravity_ = gravity;
}

void SimplePhysicsBackend::set_ground_height(double ground_y) noexcept {
    config_.ground_height = ground_y;
}

void SimplePhysicsBackend::rebuild_body_records(entt::registry& registry) {
    bodies_.clear();
    body_lookup_.clear();
    manifolds_.clear();
    candidate_pairs_.clear();
    spatial_hash_.clear();

    auto view = registry.view<TransformComponent, ColliderComponent>();
    bodies_.reserve(static_cast<std::size_t>(view.size_hint()));

    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& collider = view.get<ColliderComponent>(entity);
        auto* kinematics = registry.try_get<KinematicsComponent>(entity);
        auto* body_flags = registry.try_get<RigidbodyComponent>(entity);

        BodyRecord record{};
        record.entity = entity;
        record.position = transform.position;
        record.inverse_mass = (kinematics) ? kinematics->inverse_mass : 0.0;
        record.collider = &collider;
        record.kinematics = kinematics;
        record.body_flags = body_flags;
        record.manifold = ContactManifold{};
        record.transform = &transform;

        if (record.body_flags) {
            if (record.body_flags->is_static) {
                record.inverse_mass = 0.0;
            }
        }

        bodies_.push_back(record);
        body_lookup_[entity] = bodies_.size() - 1;
        const WorldAabb bounds = compute_world_aabb(collider, transform.position);
        spatial_hash_.insert(entity, bounds);
    }

    spatial_hash_.finalize();
}

void SimplePhysicsBackend::generate_broad_phase_pairs() {
    candidate_pairs_.clear();
    spatial_hash_.build_candidate_pairs(candidate_pairs_);
}

void SimplePhysicsBackend::run_narrow_phase() {
    manifolds_.clear();
    manifolds_.reserve(candidate_pairs_.size());

    auto try_get_body = [&](entt::entity entity) -> BodyRecord* {
        auto it = body_lookup_.find(entity);
        if (it == body_lookup_.end()) {
            return nullptr;
        }
        return &bodies_[it->second];
    };

    for (const auto& [entity_a, entity_b] : candidate_pairs_) {
        BodyRecord* body_a = try_get_body(entity_a);
        BodyRecord* body_b = try_get_body(entity_b);
        if (!body_a || !body_b || !body_a->collider || !body_b->collider) {
            continue;
        }

        const CollisionFilter& filter_a = body_a->collider->filter;
        const CollisionFilter& filter_b = body_b->collider->filter;
        const bool should_collide =
            (filter_a.category & filter_b.mask) != 0u && (filter_b.category & filter_a.mask) != 0u;
        if (!should_collide) {
            continue;
        }

        ContactManifold manifold{};
        const Vec3 pos_a = body_a->transform ? body_a->transform->position : Vec3{0.0, 0.0, 0.0};
        const Vec3 pos_b = body_b->transform ? body_b->transform->position : Vec3{0.0, 0.0, 0.0};
        const ShapeType type_a = body_a->collider->type;
        const ShapeType type_b = body_b->collider->type;

        bool collided = false;
        if (type_a == ShapeType::Sphere && type_b == ShapeType::Sphere) {
            collided = collide_sphere_sphere(entity_a,
                                             entity_b,
                                             pos_a + body_a->collider->offset,
                                             body_a->collider->sphere.radius,
                                             pos_b + body_b->collider->offset,
                                             body_b->collider->sphere.radius,
                                             manifold);
        } else if (type_a == ShapeType::Sphere && type_b == ShapeType::Aabb) {
            const Vec3 min_b = pos_b + body_b->collider->offset - body_b->collider->aabb.half_extents;
            const Vec3 max_b = pos_b + body_b->collider->offset + body_b->collider->aabb.half_extents;
            collided = collide_sphere_aabb(entity_a,
                                           entity_b,
                                           pos_a + body_a->collider->offset,
                                           body_a->collider->sphere.radius,
                                           min_b,
                                           max_b,
                                           manifold);
        } else if (type_a == ShapeType::Aabb && type_b == ShapeType::Sphere) {
            const Vec3 min_a = pos_a + body_a->collider->offset - body_a->collider->aabb.half_extents;
            const Vec3 max_a = pos_a + body_a->collider->offset + body_a->collider->aabb.half_extents;
            collided = collide_sphere_aabb(entity_b,
                                           entity_a,
                                           pos_b + body_b->collider->offset,
                                           body_b->collider->sphere.radius,
                                           min_a,
                                           max_a,
                                           manifold);
            if (collided) {
                std::swap(manifold.entity_a, manifold.entity_b);
                manifold.points[0].normal = manifold.points[0].normal * -1.0;
            }
        } else if (type_a == ShapeType::Aabb && type_b == ShapeType::Aabb) {
            const Vec3 min_a = pos_a + body_a->collider->offset - body_a->collider->aabb.half_extents;
            const Vec3 max_a = pos_a + body_a->collider->offset + body_a->collider->aabb.half_extents;
            const Vec3 min_b = pos_b + body_b->collider->offset - body_b->collider->aabb.half_extents;
            const Vec3 max_b = pos_b + body_b->collider->offset + body_b->collider->aabb.half_extents;
            collided = collide_aabb_aabb(entity_a, entity_b, min_a, max_a, min_b, max_b, manifold);
        } else if (type_a == ShapeType::Sphere && type_b == ShapeType::CapsuleY) {
            collided = collide_sphere_capsule_y(entity_a,
                                                entity_b,
                                                pos_a + body_a->collider->offset,
                                                body_a->collider->sphere.radius,
                                                pos_b + body_b->collider->offset,
                                                body_b->collider->capsule.radius,
                                                body_b->collider->capsule.half_height,
                                                manifold);
        } else if (type_a == ShapeType::CapsuleY && type_b == ShapeType::Sphere) {
            collided = collide_sphere_capsule_y(entity_b,
                                                entity_a,
                                                pos_b + body_b->collider->offset,
                                                body_b->collider->sphere.radius,
                                                pos_a + body_a->collider->offset,
                                                body_a->collider->capsule.radius,
                                                body_a->collider->capsule.half_height,
                                                manifold);
            if (collided) {
                std::swap(manifold.entity_a, manifold.entity_b);
                manifold.points[0].normal = manifold.points[0].normal * -1.0;
            }
        } else if (type_a == ShapeType::CapsuleY && type_b == ShapeType::CapsuleY) {
            collided = collide_capsule_y_capsule_y(entity_a,
                                                   entity_b,
                                                   pos_a + body_a->collider->offset,
                                                   body_a->collider->capsule.radius,
                                                   body_a->collider->capsule.half_height,
                                                   pos_b + body_b->collider->offset,
                                                   body_b->collider->capsule.radius,
                                                   body_b->collider->capsule.half_height,
                                                   manifold);
        }

        if (!collided) {
            continue;
        }

        const PhysicsMaterial material_a = resolve_material(*body_a->collider, body_a->kinematics);
        const PhysicsMaterial material_b = resolve_material(*body_b->collider, body_b->kinematics);
        manifold.mixed_friction = mix_friction(material_a.friction, material_b.friction);
        manifold.mixed_restitution = mix_restitution(material_a.restitution, material_b.restitution);
        manifold.is_trigger = filter_a.is_trigger || filter_b.is_trigger;
        manifolds_.push_back(manifold);
    }

    // Ground plane contacts
    for (auto& body : bodies_) {
        if (!body.collider || !body.transform) {
            continue;
        }
        ContactManifold ground_manifold{};
        if (collide_with_ground(body.entity,
                                *body.collider,
                                body.transform->position,
                                config_.ground_height,
                                terrain_,
                                ground_manifold)) {
            const PhysicsMaterial surface = resolve_material(*body.collider, body.kinematics);
            ground_manifold.mixed_friction = surface.friction;
            ground_manifold.mixed_restitution = surface.restitution;
            ground_manifold.is_trigger = body.collider->filter.is_trigger;
            manifolds_.push_back(ground_manifold);
        }
    }
}

void SimplePhysicsBackend::integrate_forces(double dt) {
    for (auto& body : bodies_) {
        if (!body.kinematics || body.inverse_mass <= 0.0) {
            continue;
        }
        if (body.body_flags && (body.body_flags->is_static || body.body_flags->is_kinematic)) {
            body.kinematics->accumulated_force = {0.0, 0.0, 0.0};
            continue;
        }
        const Vec3 acceleration = gravity_ + body.kinematics->accumulated_force * body.inverse_mass;
        body.kinematics->linear_velocity += acceleration * dt;
        const double damping = std::clamp(body.kinematics->linear_damping, 0.0, 1.0);
        const double damping_multiplier = std::clamp(1.0 - damping * dt, 0.0, 1.0);
        body.kinematics->linear_velocity *= damping_multiplier;
        body.kinematics->accumulated_force = {0.0, 0.0, 0.0};
    }
}

void SimplePhysicsBackend::integrate_velocities(double dt) {
    for (auto& body : bodies_) {
        if (!body.transform || !body.kinematics) {
            continue;
        }
        if (body.body_flags && (body.body_flags->is_static || body.body_flags->is_kinematic)) {
            continue;
        }
        body.transform->position += body.kinematics->linear_velocity * dt;
    }
}

void SimplePhysicsBackend::dispatch_contact_events() {
    std::unordered_map<std::uint64_t, IPhysicsBackend::ContactEvent> previous_map;
    previous_map.reserve(previous_contacts_.size());
    for (const auto& event : previous_contacts_) {
        previous_map.emplace(make_contact_key(event.entity_a, event.entity_b), event);
    }

    contact_events_.clear();
    contact_events_.reserve(manifolds_.size());
    std::unordered_set<std::uint64_t> processed_keys;
    processed_keys.reserve(manifolds_.size());

    for (const auto& manifold : manifolds_) {
        if (manifold.count == 0) {
            continue;
        }
        const auto key = make_contact_key(manifold.entity_a, manifold.entity_b);
        processed_keys.insert(key);

        IPhysicsBackend::ContactEvent event{};
        event.entity_a = manifold.entity_a;
        event.entity_b = manifold.entity_b;
        event.normal = manifold.points[0].normal;
        event.point = manifold.points[0].position;
        event.penetration = manifold.points[0].penetration;
        double impulse_sum = 0.0;
        for (std::size_t i = 0; i < manifold.count; ++i) {
            const auto& cp = manifold.points[i];
            const double tangent_mag =
                std::sqrt(cp.tangent_impulse[0] * cp.tangent_impulse[0] +
                          cp.tangent_impulse[1] * cp.tangent_impulse[1]);
            impulse_sum += std::sqrt(cp.normal_impulse * cp.normal_impulse + tangent_mag * tangent_mag);
        }
        event.impulse_magnitude = impulse_sum;

        const auto prev = previous_map.find(key);
        if (prev == previous_map.end()) {
            event.begin = true;
        } else {
            event.stay = true;
        }
        contact_events_.push_back(event);
    }

    for (const auto& [key, event] : previous_map) {
        if (processed_keys.find(key) != processed_keys.end()) {
            continue;
        }
        IPhysicsBackend::ContactEvent end_event = event;
        end_event.begin = false;
        end_event.stay = false;
        end_event.end = true;
        contact_events_.push_back(end_event);
    }

    previous_contacts_ = contact_events_;
}

}  // namespace evolution::sim

