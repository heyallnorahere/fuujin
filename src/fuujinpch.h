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

namespace fuujin {
    using Duration = std::chrono::duration<double>;

#ifdef FUUJIN_HAS_FS
    namespace fs = std::filesystem;
#else
    namespace fs = std::experimental::filesystem;
#endif
} // namespace fuujin