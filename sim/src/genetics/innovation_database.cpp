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

namespace {
    std::uint32_t host_to_le(std::uint32_t value) noexcept {
        constexpr int is_little_endian = 1;
        if (*reinterpret_cast<const char*>(&is_little_endian) == 1) {
            return value;
        }
        return ((value & 0xFF) << 24) |
               ((value & 0xFF00) << 8) |
               ((value & 0xFF0000) >> 8) |
               ((value & 0xFF000000) >> 24);
    }

    std::uint32_t le_to_host(std::uint32_t value) noexcept {
        return host_to_le(value);
    }
}

bool InnovationDatabase::save(const std::filesystem::path& path) const noexcept {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    // Binary format: [MAGIC][VERSION][NODE_ID][INNOVATION_ID][COUNT][ENTRIES...]
    // All multi-byte integers are stored in little-endian byte order for cross-platform compatibility.
    // Magic bytes for validation: 0x49564E56 ("IENV" in ASCII)
    constexpr std::uint32_t kMagic = 0x49564E56UL;
    constexpr std::uint32_t kVersion = 1;

    const std::uint32_t magic_le = host_to_le(kMagic);
    const std::uint32_t version_le = host_to_le(kVersion);
    file.write(reinterpret_cast<const char*>(&magic_le), sizeof(magic_le));
    file.write(reinterpret_cast<const char*>(&version_le), sizeof(version_le));

    const std::uint32_t count_le = host_to_le(static_cast<std::uint32_t>(connection_innovations_.size()));
    const std::uint32_t next_node_id_le = host_to_le(next_node_id_);
    const std::uint32_t next_innovation_id_le = host_to_le(next_innovation_id_);

    file.write(reinterpret_cast<const char*>(&next_node_id_le), sizeof(next_node_id_le));
    file.write(reinterpret_cast<const char*>(&next_innovation_id_le), sizeof(next_innovation_id_le));
    file.write(reinterpret_cast<const char*>(&count_le), sizeof(count_le));

    for (const auto& [key, innovation] : connection_innovations_) {
        const std::uint32_t in_node_le = host_to_le(key.first);
        const std::uint32_t out_node_le = host_to_le(key.second);
        const std::uint32_t innovation_le = host_to_le(innovation);

        file.write(reinterpret_cast<const char*>(&in_node_le), sizeof(in_node_le));
        file.write(reinterpret_cast<const char*>(&out_node_le), sizeof(out_node_le));
        file.write(reinterpret_cast<const char*>(&innovation_le), sizeof(innovation_le));
    }

    return file.good();
}

bool InnovationDatabase::load(const std::filesystem::path& path) noexcept {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    constexpr std::uint32_t kMagic = 0x49564E56UL;
    constexpr std::uint32_t kVersion = 1;

    std::uint32_t magic = 0;
    std::uint32_t version = 0;

    if (!file.read(reinterpret_cast<char*>(&magic), sizeof(magic)) ||
        !file.read(reinterpret_cast<char*>(&version), sizeof(version))) {
        return false;
    }

    magic = le_to_host(magic);
    version = le_to_host(version);

    if (magic != kMagic || version != kVersion) {
        return false;
    }

    std::uint32_t next_node_id_le = 0;
    std::uint32_t next_innovation_id_le = 0;
    std::uint32_t count_le = 0;

    if (!file.read(reinterpret_cast<char*>(&next_node_id_le), sizeof(next_node_id_le)) ||
        !file.read(reinterpret_cast<char*>(&next_innovation_id_le), sizeof(next_innovation_id_le)) ||
        !file.read(reinterpret_cast<char*>(&count_le), sizeof(count_le))) {
        return false;
    }

    next_node_id_ = le_to_host(next_node_id_le);
    next_innovation_id_ = le_to_host(next_innovation_id_le);
    const std::uint32_t count = le_to_host(count_le);

    connection_innovations_.clear();
    connection_innovations_.reserve(count);

    for (std::uint32_t i = 0; i < count; ++i) {
        std::uint32_t in_node_le = 0;
        std::uint32_t out_node_le = 0;
        std::uint32_t innovation_le = 0;

        if (!file.read(reinterpret_cast<char*>(&in_node_le), sizeof(in_node_le)) ||
            !file.read(reinterpret_cast<char*>(&out_node_le), sizeof(out_node_le)) ||
            !file.read(reinterpret_cast<char*>(&innovation_le), sizeof(innovation_le))) {
            return false;
        }

        const std::uint32_t in_node = le_to_host(in_node_le);
        const std::uint32_t out_node = le_to_host(out_node_le);
        const std::uint32_t innovation = le_to_host(innovation_le);

        connection_innovations_[{in_node, out_node}] = innovation;
    }

    return file.good();
}

}  // namespace evolution::genetics
