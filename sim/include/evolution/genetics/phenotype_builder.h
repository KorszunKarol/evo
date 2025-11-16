/**
 * @file phenotype_builder.h
 * @brief Materializes ECS entities from serialized genomes.
 */

#pragma once

#include <entt/entt.hpp>

#include "evolution/genetics/genome_storage.h"
#include "evolution/genetics/genome_types.h"

namespace evolution::genetics {

/**
 * @brief Transform stored genomes into runnable ECS phenotypes.
 * @param None.
 * @return None.
 * @throws None.
 * @complexity O(1) construction.
 * @note Stateless helper; all state provided via parameters.
 * @warning Functions are not thread-safe unless caller serializes registry access.
 * @threadsafe @notthreadsafe.
 */
class PhenotypeBuilder {
public:
    /**
     * @brief Populate an entity with components derived from a genome.
     * @param id Identifier of the genome to instantiate.
     * @param registry Destination registry receiving components.
     * @param entity Target entity handle (must be valid).
     * @param storage Source genome storage providing serialized data.
     * @return PhenotypeBuildResult describing success and derived traits.
     * @throws None.
     * @complexity O(C + W) where C = component writes and W = brain weights.
     * @note Existing components are overridden with deterministic genome-driven values.
     * @warning Caller must ensure registry ownership of entity; undefined behaviour otherwise.
     * @threadsafe @notthreadsafe.
     */
    [[nodiscard]] static PhenotypeBuildResult build(GenomeId id,
                                                    entt::registry& registry,
                                                    entt::entity entity,
                                                    const GenomeStorage& storage) noexcept;
};

}  // namespace evolution::genetics


