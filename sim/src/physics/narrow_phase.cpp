#include "evolution/sim/physics/narrow_phase.h"

#include <algorithm>
#include <cmath>

namespace evolution::sim {

namespace {

constexpr double kEpsilon = 1.0e-6;

[[nodiscard]] Vec3 clamp_vec3(const Vec3& value, const Vec3& min_v, const Vec3& max_v) noexcept {
    return Vec3{
        std::clamp(value.x, min_v.x, max_v.x),
        std::clamp(value.y, min_v.y, max_v.y),
        std::clamp(value.z, min_v.z, max_v.z)};
}

[[nodiscard]] double length_sq(const Vec3& v) noexcept {
    return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] double clamp01(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

[[nodiscard]] Vec3 closest_point_on_segment(const Vec3& a, const Vec3& b, const Vec3& point) noexcept {
    const Vec3 ab = b - a;
    const double ab_len_sq = length_sq(ab);
    if (ab_len_sq <= kEpsilon) {
        return a;
    }
    const double t = std::clamp(((point - a).x * ab.x + (point - a).y * ab.y + (point - a).z * ab.z) / ab_len_sq, 0.0, 1.0);
    return Vec3{a.x + ab.x * t, a.y + ab.y * t, a.z + ab.z * t};
}

[[nodiscard]] std::pair<Vec3, Vec3> closest_points_between_segments(const Vec3& p1, const Vec3& q1,
                                                                    const Vec3& p2, const Vec3& q2) noexcept {
    const Vec3 d1 = q1 - p1;
    const Vec3 d2 = q2 - p2;
    const Vec3 r = p1 - p2;
    const double a = length_sq(d1);
    const double e = length_sq(d2);
    const double f = (d2.x * r.x) + (d2.y * r.y) + (d2.z * r.z);

    double s = 0.0;
    double t = 0.0;

    if (a <= kEpsilon && e <= kEpsilon) {
        return {p1, p2};
    }

    if (a <= kEpsilon) {
        s = 0.0;
        t = clamp01(f / e);
    } else {
        const double c = (d1.x * r.x) + (d1.y * r.y) + (d1.z * r.z);
        if (e <= kEpsilon) {
            t = 0.0;
            s = clamp01(-c / a);
        } else {
            const double b = (d1.x * d2.x) + (d1.y * d2.y) + (d1.z * d2.z);
            const double denom = a * e - b * b;
            if (denom != 0.0) {
                s = std::clamp((b * f - c * e) / denom, 0.0, 1.0);
            } else {
                s = 0.0;
            }
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = clamp01(-c / a);
            } else if (t > 1.0) {
                t = 1.0;
                s = clamp01((b - c) / a);
            }
        }
    }

    Vec3 closest_a{p1.x + d1.x * s, p1.y + d1.y * s, p1.z + d1.z * s};
    Vec3 closest_b{p2.x + d2.x * t, p2.y + d2.y * t, p2.z + d2.z * t};
    return {closest_a, closest_b};
}

void set_single_point(ContactManifold& manifold,
                      entt::entity entity_a,
                      entt::entity entity_b,
                      const Vec3& point,
                      const Vec3& normal,
                      double penetration) {
    manifold.entity_a = entity_a;
    manifold.entity_b = entity_b;
    manifold.count = 1;
    manifold.points[0].position = point;
    manifold.points[0].normal = normal;
    manifold.points[0].penetration = std::max(0.0, penetration);
    manifold.points[0].normal_impulse = 0.0;
    manifold.points[0].tangent_impulse[0] = 0.0;
    manifold.points[0].tangent_impulse[1] = 0.0;
}

}  // namespace

bool collide_sphere_sphere(entt::entity entity_a,
                           entt::entity entity_b,
                           const Vec3& center_a,
                           double radius_a,
                           const Vec3& center_b,
                           double radius_b,
                           ContactManifold& manifold) {
    const Vec3 delta = center_b - center_a;
    const double distance_sq = length_sq(delta);
    const double radius_sum = radius_a + radius_b;
    if (distance_sq >= radius_sum * radius_sum) {
        return false;
    }
    const double distance = std::sqrt(std::max(distance_sq, kEpsilon));
    const Vec3 normal = (distance > kEpsilon) ? Vec3{delta.x / distance, delta.y / distance, delta.z / distance}
                                              : Vec3{0.0, 1.0, 0.0};
    const Vec3 contact_point = center_a + normal * radius_a;
    const double penetration = radius_sum - distance;
    set_single_point(manifold, entity_a, entity_b, contact_point, normal, penetration);
    return true;
}

bool collide_sphere_aabb(entt::entity entity_a,
                         entt::entity entity_b,
                         const Vec3& center,
                         double radius,
                         const Vec3& box_min,
                         const Vec3& box_max,
                         ContactManifold& manifold) {
    const Vec3 closest = clamp_vec3(center, box_min, box_max);
    const Vec3 delta = center - closest;
    const double distance_sq = length_sq(delta);
    if (distance_sq >= radius * radius) {
        return false;
    }
    const double distance = std::sqrt(std::max(distance_sq, kEpsilon));
    const Vec3 normal = (distance > kEpsilon) ? Vec3{delta.x / distance, delta.y / distance, delta.z / distance}
                                              : Vec3{0.0, 1.0, 0.0};
    const double penetration = radius - distance;
    const Vec3 point = closest;
    set_single_point(manifold, entity_a, entity_b, point, normal, penetration);
    return true;
}

bool collide_aabb_aabb(entt::entity entity_a,
                       entt::entity entity_b,
                       const Vec3& min_a,
                       const Vec3& max_a,
                       const Vec3& min_b,
                       const Vec3& max_b,
                       ContactManifold& manifold) {
    const Vec3 overlap_min{std::max(min_a.x, min_b.x), std::max(min_a.y, min_b.y), std::max(min_a.z, min_b.z)};
    const Vec3 overlap_max{std::min(max_a.x, max_b.x), std::min(max_a.y, max_b.y), std::min(max_a.z, max_b.z)};
    if (overlap_min.x > overlap_max.x || overlap_min.y > overlap_max.y || overlap_min.z > overlap_max.z) {
        return false;
    }

    const Vec3 center_a{(min_a.x + max_a.x) * 0.5, (min_a.y + max_a.y) * 0.5, (min_a.z + max_a.z) * 0.5};
    const Vec3 center_b{(min_b.x + max_b.x) * 0.5, (min_b.y + max_b.y) * 0.5, (min_b.z + max_b.z) * 0.5};
    const Vec3 delta = center_b - center_a;
    const double overlap_x = overlap_max.x - overlap_min.x;
    const double overlap_y = overlap_max.y - overlap_min.y;
    const double overlap_z = overlap_max.z - overlap_min.z;

    Vec3 normal{0.0, 0.0, 0.0};
    double penetration = overlap_x;
    if (overlap_y < penetration) {
        penetration = overlap_y;
        normal = {0.0, (delta.y >= 0.0) ? 1.0 : -1.0, 0.0};
    } else {
        normal = {(delta.x >= 0.0) ? 1.0 : -1.0, 0.0, 0.0};
    }
    if (overlap_z < penetration) {
        penetration = overlap_z;
        normal = {0.0, 0.0, (delta.z >= 0.0) ? 1.0 : -1.0};
    }

    const Vec3 point{std::clamp(center_a.x, overlap_min.x, overlap_max.x),
                     std::clamp(center_a.y, overlap_min.y, overlap_max.y),
                     std::clamp(center_a.z, overlap_min.z, overlap_max.z)};

    set_single_point(manifold, entity_a, entity_b, point, normal, penetration);
    return true;
}

bool collide_sphere_capsule_y(entt::entity entity_a,
                              entt::entity entity_b,
                              const Vec3& sphere_center,
                              double sphere_radius,
                              const Vec3& capsule_center,
                              double capsule_radius,
                              double capsule_half_height,
                              ContactManifold& manifold) {
    const Vec3 segment_a = capsule_center + Vec3{0.0, -capsule_half_height, 0.0};
    const Vec3 segment_b = capsule_center + Vec3{0.0, capsule_half_height, 0.0};
    const Vec3 closest = closest_point_on_segment(segment_a, segment_b, sphere_center);
    const Vec3 delta = sphere_center - closest;
    const double radius_sum = sphere_radius + capsule_radius;
    const double dist_sq = length_sq(delta);
    if (dist_sq >= radius_sum * radius_sum) {
        return false;
    }
    const double dist = std::sqrt(std::max(dist_sq, kEpsilon));
    const Vec3 normal = (dist > kEpsilon) ? Vec3{delta.x / dist, delta.y / dist, delta.z / dist}
                                          : Vec3{0.0, 1.0, 0.0};
    const double penetration = radius_sum - dist;
    const Vec3 point = closest + normal * capsule_radius;
    set_single_point(manifold, entity_a, entity_b, point, normal, penetration);
    return true;
}

bool collide_capsule_y_capsule_y(entt::entity entity_a,
                                 entt::entity entity_b,
                                 const Vec3& center_a,
                                 double radius_a,
                                 double half_height_a,
                                 const Vec3& center_b,
                                 double radius_b,
                                 double half_height_b,
                                 ContactManifold& manifold) {
    const Vec3 a0 = center_a + Vec3{0.0, -half_height_a, 0.0};
    const Vec3 a1 = center_a + Vec3{0.0, half_height_a, 0.0};
    const Vec3 b0 = center_b + Vec3{0.0, -half_height_b, 0.0};
    const Vec3 b1 = center_b + Vec3{0.0, half_height_b, 0.0};

    const auto [closest_a, closest_b] = closest_points_between_segments(a0, a1, b0, b1);
    const Vec3 delta = closest_b - closest_a;
    const double radius_sum = radius_a + radius_b;
    const double dist_sq = length_sq(delta);
    if (dist_sq >= radius_sum * radius_sum) {
        return false;
    }
    const double dist = std::sqrt(std::max(dist_sq, kEpsilon));
    const Vec3 normal = (dist > kEpsilon) ? Vec3{delta.x / dist, delta.y / dist, delta.z / dist}
                                          : Vec3{0.0, 1.0, 0.0};
    const double penetration = radius_sum - dist;
    const Vec3 point = closest_a + normal * radius_a;
    set_single_point(manifold, entity_a, entity_b, point, normal, penetration);
    return true;
}

bool collide_with_ground(entt::entity entity,
                         const ColliderComponent& collider,
                         const Vec3& position,
                         double ground_y,
                         const Terrain* terrain,
                         ContactManifold& manifold) {
    manifold.entity_a = entity;
    manifold.entity_b = entt::null;
    manifold.count = 0;

    const Vec3 center = position + collider.offset;
    const Vec3 surface_point = terrain ? Vec3{center.x, terrain->height(center.x, center.z), center.z}
                                       : Vec3{center.x, ground_y, center.z};
    // Use a vertical support normal for terrain contacts. Slope normals in this
    // simplified solver can inject lateral impulses and cause runaway drift.
    const Vec3 normal{0.0, 1.0, 0.0};

    auto set_contact = [&](const Vec3& point, double penetration) {
        set_single_point(manifold, entity, entt::null, point, normal, penetration);
    };

    switch (collider.type) {
        case ShapeType::Sphere: {
            const double signed_distance = (center - surface_point).x * normal.x
                                           + (center - surface_point).y * normal.y
                                           + (center - surface_point).z * normal.z;
            if (signed_distance >= collider.sphere.radius) {
                return false;
            }
            const double penetration = collider.sphere.radius - signed_distance;
            const Vec3 point = center - normal * collider.sphere.radius;
            set_contact(point, penetration);
            return true;
        }
        case ShapeType::Aabb: {
            Vec3 support{
                (normal.x >= 0.0) ? -collider.aabb.half_extents.x : collider.aabb.half_extents.x,
                (normal.y >= 0.0) ? -collider.aabb.half_extents.y : collider.aabb.half_extents.y,
                (normal.z >= 0.0) ? -collider.aabb.half_extents.z : collider.aabb.half_extents.z};
            const Vec3 support_point = center + support;
            const double signed_distance = (support_point - surface_point).x * normal.x
                                           + (support_point - surface_point).y * normal.y
                                           + (support_point - surface_point).z * normal.z;
            if (signed_distance >= 0.0) {
                return false;
            }
            const double penetration = -signed_distance;
            set_contact(support_point, penetration);
            return true;
        }
        case ShapeType::CapsuleY: {
            const Vec3 bottom_center = center + Vec3{0.0, -collider.capsule.half_height, 0.0};
            const double signed_distance = (bottom_center - surface_point).x * normal.x
                                           + (bottom_center - surface_point).y * normal.y
                                           + (bottom_center - surface_point).z * normal.z;
            if (signed_distance >= collider.capsule.radius) {
                return false;
            }
            const double penetration = collider.capsule.radius - signed_distance;
            const Vec3 point = bottom_center - normal * collider.capsule.radius;
            set_contact(point, penetration);
            return true;
        }
        default:
            return false;
    }
}

}  // namespace evolution::sim

