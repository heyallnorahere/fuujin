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

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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
            if (!node.IsSequence() || node.size() != (size_t)L) {
                return false;
            }

            for (glm::length_t i = 0; i < L; i++) {
                value[i] = node[i].as<_Ty>();
            }

            return true;
        }
    };

    // note that this serializes matrices to appear transposed
    // glm matrices are column-major
    template <glm::length_t C, glm::length_t R, typename _Ty, glm::qualifier Q>
    struct convert<glm::mat<C, R, _Ty, Q>> {
        using matrix = glm::mat<C, R, _Ty, Q>;

        static Node encode(const matrix& value) {
            Node node;
            node.SetStyle(EmitterStyle::Block);

            for (glm::length_t i = 0; i < C; i++) {
                node.push_back(value[i]);
            }

            return node;
        }

        static bool decode(const Node& node, matrix& value) {
            if (!node.IsSequence() || node.size() != (size_t)C) {
                return false;
            }

            for (glm::length_t i = 0; i < C; i++) {
                value[i] = node[i].as<class matrix::col_type>();
            }

            return true;
        }
    };

    template <typename _Ty, glm::qualifier Q>
    struct convert<glm::qua<_Ty, Q>> {
        using quat = glm::qua<_Ty, Q>;

        static Node encode(const quat& value) {
            Node node;
            node.SetStyle(EmitterStyle::Flow);

            for (glm::length_t i = 0; i < value.length(); i++) {
                node.push_back(value[i]);
            }

            return node;
        }

        static bool decode(const Node& node, quat& value) {
            if (!node.IsSequence() || node.size() != (size_t)value.length()) {
                return false;
            }

            for (glm::length_t i = 0; i < value.length(); i++) {
                value[i] = node[i].as<_Ty>();
            }

            return true;
        }
    };

    template <typename _Rep, typename _Period>
    struct convert<std::chrono::duration<_Rep, _Period>> {
        using duration = std::chrono::duration<_Rep, _Period>;

        static Node encode(const duration& value) { return Node(value.count()); }

        static bool decode(const Node& node, duration& value) {
            if (!node.IsScalar()) {
                return false;
            }

            value = duration(node.as<_Rep>());
            return true;
        }
    };
} // namespace YAML

inline void* operator new(size_t size) { return fuujin::allocate(size); }
inline void operator delete(void* block) { return fuujin::freemem(block); }

#define FUUJIN_TRACE(...) SPDLOG_LOGGER_TRACE(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_DEBUG(...) SPDLOG_LOGGER_DEBUG(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_INFO(...) SPDLOG_LOGGER_INFO(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_WARN(...) SPDLOG_LOGGER_WARN(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_ERROR(...) SPDLOG_LOGGER_ERROR(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(&::fuujin::s_Logger, __VA_ARGS__)
