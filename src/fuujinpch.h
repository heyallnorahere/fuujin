#pragma once

#include <string>
#include <memory>
#include <vector>
#include <stdexcept>

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
#ifdef FUUJIN_HAS_FS
    namespace fs = std::filesystem;
#else
    namespace fs = std::experimental::filesystem;
#endif
} // namespace fuujin