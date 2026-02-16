#pragma once

#include <algorithm>
#include <vector>

#include <entt/entity/registry.hpp>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/soil_volume.h"
#include "evolution/sim/runtime_contracts.h"

namespace evolution::sim {

class SoilGridField final : public ISoilField {
public:
    explicit SoilGridField(SoilGrid* grid) : grid_(grid) {}

    [[nodiscard]] double sample_nutrient(double x, double z) const noexcept override {
        if (grid_ == nullptr) {
            return 0.0;
        }
        return static_cast<double>(grid_->sample(x, z));
    }

    void consume_nutrient(double x, double z, double amount) noexcept override {
        if (grid_ == nullptr || amount <= 0.0) {
            return;
        }
        const int ix =
            std::clamp(static_cast<int>(x / grid_->cell_size()), 0, grid_->width() - 1);
        const int iz =
            std::clamp(static_cast<int>(z / grid_->cell_size()), 0, grid_->height() - 1);
        float& cell = grid_->at(ix, iz);
        cell = std::max(0.0F, cell - static_cast<float>(amount));
    }

private:
    SoilGrid* grid_{nullptr};
};

class SoilVolumeField final : public ISoilField {
public:
    explicit SoilVolumeField(SoilVolume* volume) : volume_(volume) {}

    [[nodiscard]] double sample_nutrient(double x, double z) const noexcept override {
        if (volume_ == nullptr) {
            return 0.0;
        }
        const SoilVoxel voxel = volume_->sample(Vec3{x, 0.0, z});
        return static_cast<double>(voxel.nitrogen);
    }

    void consume_nutrient(double x, double z, double amount) noexcept override {
        if (volume_ == nullptr || amount <= 0.0) {
            return;
        }
        int ix =
            std::clamp(static_cast<int>(x / volume_->voxel_size()), 0, volume_->width() - 1);
        int iz =
            std::clamp(static_cast<int>(z / volume_->voxel_size()), 0, volume_->depth() - 1);
        SoilVoxel& voxel = volume_->at(ix, 0, iz);
        voxel.nitrogen = static_cast<float>(
            std::max(0.0, static_cast<double>(voxel.nitrogen) - amount));
    }

private:
    SoilVolume* volume_{nullptr};
};

class PlantSpatialQueryAdapter final : public IPlantIndex {
public:
    PlantSpatialQueryAdapter(const PlantSpatialIndex* index, entt::registry* registry)
        : index_(index), registry_(registry) {}

    [[nodiscard]] std::vector<entt::entity> query_radius(double x,
                                                          double z,
                                                          double radius) const override {
        std::vector<entt::entity> hits;
        if (index_ == nullptr || registry_ == nullptr || radius <= 0.0) {
            return hits;
        }

        index_->for_each_in_radius(
            *registry_,
            Vec3{x, 0.0, z},
            radius,
            [&](entt::entity entity, double) { hits.push_back(entity); });
        return hits;
    }

private:
    const PlantSpatialIndex* index_{nullptr};
    entt::registry* registry_{nullptr};
};

}  // namespace evolution::sim
