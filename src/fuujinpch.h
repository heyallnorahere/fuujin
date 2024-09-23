#pragma once

#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <chrono>
#include <mutex>
#include <functional>
#include <type_traits>

#include <stddef.h>
#include <cstdint>

#include <tracy/Tracy.hpp>

#if __has_include(<filesystem>)
#include <filesystem>
#define FUUJIN_HAS_FS
#else
#include <experimental/filesystem>
#endif

#define SPDLOG_ACTIVE_LEVEL 0
#include <spdlog/spdlog.h>

namespace fuujin {
    using Duration = std::chrono::duration<double>;

    extern spdlog::logger s_Logger;

#ifdef FUUJIN_HAS_FS
    namespace fs = std::filesystem;
#else
    namespace fs = std::experimental::filesystem;
#endif
} // namespace fuujin

#define FUUJIN_TRACE(...) SPDLOG_LOGGER_TRACE(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_DEBUG(...) SPDLOG_LOGGER_DEBUG(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_INFO(...) SPDLOG_LOGGER_INFO(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_WARN(...) SPDLOG_LOGGER_WARN(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_ERROR(...) SPDLOG_LOGGER_ERROR(&::fuujin::s_Logger, __VA_ARGS__)
#define FUUJIN_CRITICAL(...) SPDLOG_LOGGER_CRITICAL(&::fuujin::s_Logger, __VA_ARGS__)