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
    solve_joint_constraints(registry, dt);
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
    auto dot = [](const Vec3& a, const Vec3& b) -> double {
        return a.x * b.x + a.y * b.y + a.z * b.z;
    };

    auto normalize = [&dot](const Vec3& v) -> Vec3 {
        const double len = std::sqrt(dot(v, v));
        if (len < 1e-9) {
            return Vec3{0.0, 0.0, 1.0};
        }
        return Vec3{v.x / len, v.y / len, v.z / len};
    };

    const Vec3 dir = normalize(direction);
    double best_t = max_distance + 1.0;
    ContactEvent best_hit{};
    bool found = false;

    for (const auto& body : bodies_) {
        if (!body.collider || !body.transform) {
            continue;
        }

        const Vec3 pos = body.transform->position + body.collider->offset;
        double t = max_distance + 1.0;
        Vec3 hit_normal{0.0, 1.0, 0.0};

        switch (body.collider->type) {
            case ShapeType::Sphere: {
                const double r = body.collider->sphere.radius;
                const Vec3 oc = origin - pos;
                const double a = dot(dir, dir);
                const double b = 2.0 * dot(oc, dir);
                const double c = dot(oc, oc) - r * r;
                const double discriminant = b * b - 4.0 * a * c;
                if (discriminant >= 0.0) {
                    const double sqrt_disc = std::sqrt(discriminant);
                    double t0 = (-b - sqrt_disc) / (2.0 * a);
                    double t1 = (-b + sqrt_disc) / (2.0 * a);
                    if (t0 > 0.0 && t0 < t) {
                        t = t0;
                    } else if (t1 > 0.0 && t1 < t) {
                        t = t1;
                    }
                    if (t <= max_distance) {
                        Vec3 hit_point = origin + dir * t;
                        hit_normal = normalize(hit_point - pos);
                    }
                }
                break;
            }
            case ShapeType::Aabb: {
                const Vec3 half = body.collider->aabb.half_extents;
                const Vec3 box_min = pos - half;
                const Vec3 box_max = pos + half;
                double tmin = -1e18;
                double tmax = 1e18;
                int hit_axis = 0;
                bool valid = true;
                for (int axis = 0; axis < 3; ++axis) {
                    double o = (axis == 0) ? origin.x : ((axis == 1) ? origin.y : origin.z);
                    double d = (axis == 0) ? dir.x : ((axis == 1) ? dir.y : dir.z);
                    double bmin = (axis == 0) ? box_min.x : ((axis == 1) ? box_min.y : box_min.z);
                    double bmax = (axis == 0) ? box_max.x : ((axis == 1) ? box_max.y : box_max.z);
                    if (std::abs(d) < 1e-9) {
                        if (o < bmin || o > bmax) {
                            valid = false;
                            break;
                        }
                    } else {
                        double t1 = (bmin - o) / d;
                        double t2 = (bmax - o) / d;
                        if (t1 > t2) {
                            std::swap(t1, t2);
                        }
                        if (t1 > tmin) {
                            tmin = t1;
                            hit_axis = axis;
                        }
                        tmax = std::min(tmax, t2);
                        if (tmin > tmax || tmax < 0.0) {
                            valid = false;
                            break;
                        }
                    }
                }
                if (valid && tmin > 0.0 && tmin < t) {
                    t = tmin;
                    hit_normal = Vec3{0.0, 0.0, 0.0};
                    double* n = (hit_axis == 0) ? &hit_normal.x : ((hit_axis == 1) ? &hit_normal.y : &hit_normal.z);
                    double d_comp = (hit_axis == 0) ? dir.x : ((hit_axis == 1) ? dir.y : dir.z);
                    *n = (d_comp > 0.0) ? -1.0 : 1.0;
                }
                break;
            }
            case ShapeType::CapsuleY: {
                const double r = body.collider->capsule.radius;
                const double hh = body.collider->capsule.half_height;
                const Vec3 cap_top = Vec3{pos.x, pos.y + hh, pos.z};
                const Vec3 cap_bot = Vec3{pos.x, pos.y - hh, pos.z};
                const Vec3 oc = origin - cap_bot;
                const Vec3 seg_dir = cap_top - cap_bot;
                const double seg_len = 2.0 * hh;
                const double dir_dot_seg = dot(dir, seg_dir);
                const double oc_dot_seg = dot(oc, seg_dir);
                const double seg_sq = seg_len * seg_len;
                const double a = dot(dir, dir) - (dir_dot_seg * dir_dot_seg) / seg_sq;
                const double b = 2.0 * (dot(oc, dir) - (oc_dot_seg * dir_dot_seg) / seg_sq);
                const double c = dot(oc, oc) - (oc_dot_seg * oc_dot_seg) / seg_sq - r * r;
                const double discriminant = b * b - 4.0 * a * c;
                double cyl_t = max_distance + 1.0;
                if (discriminant >= 0.0 && std::abs(a) > 1e-9) {
                    const double sqrt_disc = std::sqrt(discriminant);
                    double t0 = (-b - sqrt_disc) / (2.0 * a);
                    if (t0 > 0.0) {
                        Vec3 hit_pt = origin + dir * t0;
                        double proj = dot(hit_pt - cap_bot, seg_dir) / seg_sq;
                        if (proj >= 0.0 && proj <= 1.0) {
                            cyl_t = t0;
                        }
                    }
                }
                for (const Vec3& sphere_center : {cap_top, cap_bot}) {
                    const Vec3 oc_s = origin - sphere_center;
                    const double a_s = dot(dir, dir);
                    const double b_s = 2.0 * dot(oc_s, dir);
                    const double c_s = dot(oc_s, oc_s) - r * r;
                    const double disc_s = b_s * b_s - 4.0 * a_s * c_s;
                    if (disc_s >= 0.0) {
                        const double sqrt_d = std::sqrt(disc_s);
                        double t_s = (-b_s - sqrt_d) / (2.0 * a_s);
                        if (t_s > 0.0 && t_s < cyl_t) {
                            cyl_t = t_s;
                        }
                    }
                }
                if (cyl_t <= max_distance && cyl_t < t) {
                    t = cyl_t;
                    Vec3 hit_pt = origin + dir * t;
                    double proj = dot(hit_pt - cap_bot, seg_dir) / seg_sq;
                    proj = std::clamp(proj, 0.0, 1.0);
                    Vec3 closest_on_axis = cap_bot + seg_dir * proj;
                    hit_normal = normalize(hit_pt - closest_on_axis);
                }
                break;
            }
        }

        if (t > 0.0 && t <= max_distance && t < best_t) {
            best_t = t;
            best_hit.entity_a = entt::null;
            best_hit.entity_b = body.entity;
            best_hit.point = origin + dir * t;
            best_hit.normal = hit_normal;
            best_hit.penetration = 0.0;
            best_hit.begin = true;
            found = true;
        }
    }

    if (terrain_ != nullptr) {
        constexpr int kMaxSteps = 64;
        const double step_size = max_distance / static_cast<double>(kMaxSteps);
        for (int i = 1; i <= kMaxSteps; ++i) {
            const double t_sample = step_size * static_cast<double>(i);
            const Vec3 sample_pt = origin + dir * t_sample;
            const double terrain_y = terrain_->height(sample_pt.x, sample_pt.z);
            if (sample_pt.y <= terrain_y) {
                if (t_sample < best_t) {
                    best_t = t_sample;
                    best_hit.entity_a = entt::null;
                    best_hit.entity_b = entt::null;
                    best_hit.point = sample_pt;
                    best_hit.normal = terrain_->normal(sample_pt.x, sample_pt.z);
                    best_hit.penetration = terrain_y - sample_pt.y;
                    best_hit.begin = true;
                    found = true;
                }
                break;
            }
        }
    }

    if (found) {
        return best_hit;
    }
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
    constexpr double kMaxVelocity = 50.0;  // 50 m/s max to prevent explosions
    constexpr double kTerrainMin = -10.0;  // Small buffer below terrain
    constexpr double kTerrainMax = 200.0;  // Safety bound
    
    for (auto& body : bodies_) {
        if (!body.transform || !body.kinematics) {
            continue;
        }
        if (body.body_flags && (body.body_flags->is_static || body.body_flags->is_kinematic)) {
            continue;
        }
        
        // Clamp velocities to prevent blowup
        auto& vel = body.kinematics->linear_velocity;
        vel.x = std::clamp(vel.x, -kMaxVelocity, kMaxVelocity);
        vel.y = std::clamp(vel.y, -kMaxVelocity, kMaxVelocity);
        vel.z = std::clamp(vel.z, -kMaxVelocity, kMaxVelocity);
        
        body.transform->position += vel * dt;
        
        // Clamp positions to terrain bounds (safety net)
        body.transform->position.x = std::clamp(body.transform->position.x, kTerrainMin, kTerrainMax);
        body.transform->position.y = std::clamp(body.transform->position.y, kTerrainMin, kTerrainMax);
        body.transform->position.z = std::clamp(body.transform->position.z, kTerrainMin, kTerrainMax);
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


