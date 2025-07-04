#pragma once

#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <functional>
#include <type_traits>
#include <optional>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <future>
#include <limits>
#include <queue>
#include <map>
#include <set>

#include <stddef.h>
#include <cstdint>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <tracy/Tracy.hpp>

#if __has_include(<filesystem>)
#include <filesystem>
#define FUUJIN_HAS_FS
#else
#include <experimental/filesystem>
#endif

#define SPDLOG_ACTIVE_LEVEL 0
#include <spdlog/spdlog.h>

#include <yaml-cpp/yaml.h>

namespace YAML {
    template <glm::length_t L, typename _Ty, glm::qualifier Q>
    struct convert<glm::vec<L, _Ty, Q>> {
        using vector = glm::vec<L, _Ty, Q>;

        static Node encode(const vector& value) {
            Node node;
            node.SetStyle(EmitterStyle::Flow);

            for (glm::length_t i = 0; i < L; i++) {
                node.push_back(value[i]);
            }

            return node;
        }

        static bool decode(const Node& node, vector& value) {
            if (!node.IsSequence()) {
                return false;
            }

            for (glm::length_t i = 0; i < L; i++) {
                value[i] = node[i].as<_Ty>();
            }

            return true;
        }
    };
} // namespace YAML

namespace fuujin {
    using Duration = std::chrono::duration<double>;

    struct Version {
        uint32_t Major, Minor, Patch;

        inline operator std::string() {
            return std::to_string(Major) + "." + std::to_string(Minor) + "." +
                   std::to_string(Patch);
        }
    };

    extern spdlog::logger s_Logger;

#ifdef FUUJIN_HAS_FS
    namespace fs = std::filesystem;
#else
    namespace fs = std::experimental::filesystem;
#endif

    inline void* allocate(size_t size) {
        void* block = std::malloc(size);
        TracyAlloc(block, size);
        return block;
    }

    inline void* reallocate(void* block, size_t size) {
        TracyFree(block);

        void* newBlock = std::realloc(block, size);
        TracyAlloc(newBlock, size);
        return newBlock;
    }

    inline void freemem(void* block) {
        TracyFree(block);
        std::free(block);
    }
} // namespace fuujin

inline void* operator new(size_t size) { return fuujin::allocate(size); }
inline void operator delete(void* block) { return fuujin::freemem(block); }

#define FUUJIN_TRACE(...) SPDLOG_LOGGER_TRACE(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_DEBUG(...) SPDLOG_LOGGER_DEBUG(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_INFO(...) SPDLOG_LOGGER_INFO(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_WARN(...) SPDLOG_LOGGER_WARN(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_ERROR(...) SPDLOG_LOGGER_ERROR(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(&::fuujin::s_Logger, __VA_ARGS__)