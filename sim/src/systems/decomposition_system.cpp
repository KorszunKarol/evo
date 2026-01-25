#include "evolution/sim/decomposition_system.h"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "evolution/sim/environment/environment.h"
#include "evolution/sim/environment/soil_volume.h"

namespace evolution::sim {

void DecompositionSystem::tick(SimulationContext& context) {
    auto& registry = context.registry();
    const double dt = context.fixed_dt();

    auto* stats_ptr = registry.ctx().find<DecompositionStatistics>();
    if (stats_ptr == nullptr) {
        stats_ptr = &registry.ctx().emplace<DecompositionStatistics>();
    }
    auto& stats = *stats_ptr;
    stats.biomass_decayed_last_tick = 0.0;
    stats.nutrients_returned_last_tick = 0.0;

    SoilVolume* volume = registry.ctx().find<SoilVolume>();
    SoilGrid* soil_grid = registry.ctx().find<SoilGrid>();

    if (volume == nullptr && soil_grid == nullptr) {
        return;
    }

    removal_queue_.clear();

    auto view = registry.view<TransformComponent, CorpseComponent>();
    for (auto entity : view) {
        auto& transform = view.get<TransformComponent>(entity);
        auto& corpse = view.get<CorpseComponent>(entity);

        corpse.age += dt;
        
        // Toxicity increases as it rots (simplified model)
        corpse.toxicity = std::min(1.0, corpse.toxicity + 0.01 * dt);

        double loss = corpse.decay_rate * dt;
        if (loss > corpse.biomass) {
            loss = corpse.biomass;
        }

        if (loss > 0.0) {
            corpse.biomass -= loss;
            stats.biomass_decayed_last_tick += loss;
            stats.nutrients_returned_last_tick += loss;
            
            // Inject into soil
            if (volume) {
                int ix = static_cast<int>(transform.position.x / volume->voxel_size());
                int iy = static_cast<int>(transform.position.y / volume->voxel_size());
                int iz = static_cast<int>(transform.position.z / volume->voxel_size());
                
                ix = std::clamp(ix, 0, volume->width() - 1);
                iy = std::clamp(iy, 0, volume->height() - 1);
                iz = std::clamp(iz, 0, volume->depth() - 1);
                
                auto& voxel = volume->at(ix, iy, iz);
                double current_n = voxel.nitrogen.to_double();
                voxel.nitrogen = math::Fixed64(current_n + loss);
            } else if (soil_grid) {
                const double cell_size = soil_grid->cell_size();
                const int ix = std::clamp(static_cast<int>(transform.position.x / cell_size), 0, soil_grid->width() - 1);
                const int iz = std::clamp(static_cast<int>(transform.position.z / cell_size), 0, soil_grid->height() - 1);
                auto& cell = soil_grid->at(ix, iz);
                cell += static_cast<float>(loss);
            }
        }

        if (corpse.biomass <= 0.0) {
            removal_queue_.push_back(entity);
        }
    }

    for (auto entity : removal_queue_) {
        registry.destroy(entity);
    }
}

}  // namespace evolution::sim
