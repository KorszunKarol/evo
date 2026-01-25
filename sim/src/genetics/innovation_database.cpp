#include "evolution/genetics/innovation_database.h"

#include <cstring>
#include <fstream>

namespace evolution::genetics {

InnovationDatabase::InnovationDatabase(std::uint32_t initial_node_id,
                                       std::uint32_t initial_innovation_id) noexcept
    : next_innovation_id_(initial_innovation_id)
    , next_node_id_(initial_node_id) {}

std::uint32_t InnovationDatabase::get_or_create(std::uint32_t in_node,
                                                 std::uint32_t out_node) noexcept {
    const ConnectionKey key{in_node, out_node};
    const auto it = connection_innovations_.find(key);
    if (it != connection_innovations_.end()) {
        return it->second;
    }
    
    const std::uint32_t innovation = next_innovation_id_++;
    connection_innovations_[key] = innovation;
    return innovation;
}

std::uint32_t InnovationDatabase::get(std::uint32_t in_node,
                                       std::uint32_t out_node) const noexcept {
    const ConnectionKey key{in_node, out_node};
    const auto it = connection_innovations_.find(key);
    return it != connection_innovations_.end() ? it->second : 0;
}

std::uint32_t InnovationDatabase::next_node_id() noexcept {
    return next_node_id_++;
}

void InnovationDatabase::reset_generation() noexcept {
    connection_innovations_.clear();
}

void InnovationDatabase::clear() noexcept {
    connection_innovations_.clear();
    next_innovation_id_ = 1;
    next_node_id_ = 1000;
}

bool InnovationDatabase::save(const std::filesystem::path& path) const noexcept {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Binary format: [MAGIC][VERSION][COUNT][ENTRIES][DATA...]
    // Magic bytes for validation
    constexpr std::uint32_t kMagic = 0x49564E56UL;
    constexpr std::uint32_t kVersion = 1;
    
    file.write(reinterpret_cast<const char*>(&kMagic), sizeof(kMagic));
    file.write(reinterpret_cast<const char*>(&kVersion), sizeof(kVersion));
    
    const std::uint32_t count = static_cast<std::uint32_t>(connection_innovations_.size());
    file.write(reinterpret_cast<const char*>(&count), sizeof(count));
    
    for (const auto& [key, innovation] : connection_innovations_) {
        file.write(reinterpret_cast<const char*>(&key.first), sizeof(key.first));
        file.write(reinterpret_cast<const char*>(&key.second), sizeof(key.second));
        file.write(reinterpret_cast<const char*>(&innovation), sizeof(innovation));
    }
    
    return file.good();
}

bool InnovationDatabase::load(const std::filesystem::path& path) noexcept {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != 1) {
        return false;
    }

    file.read(reinterpret_cast<char*>(&next_node_id_), sizeof(next_node_id_));
    file.read(reinterpret_cast<char*>(&next_innovation_id_), sizeof(next_innovation_id_));

    std::uint32_t count = 0;
    file.read(reinterpret_cast<char*>(&count), sizeof(count));

    connection_innovations_.clear();
    connection_innovations_.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t in_node = 0;
        std::uint32_t out_node = 0;
        std::uint32_t innovation = 0;
        
        file.read(reinterpret_cast<char*>(&in_node), sizeof(in_node));
        file.read(reinterpret_cast<char*>(&out_node), sizeof(out_node));
        file.read(reinterpret_cast<char*>(&innovation), sizeof(innovation));
        
        connection_innovations_[{in_node, out_node}] = innovation;
    }

    return file.good();
}

}  // namespace evolution::genetics

