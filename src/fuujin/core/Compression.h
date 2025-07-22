#pragma once

#include "fuujin/core/Buffer.h"

namespace fuujin {
    // uses ZSTD
    class Compression {
    public:
        Compression() = delete;

        static void Init();
        static void Shutdown();

        // returns usable slice of buffer with optimal capacity
        static Buffer Compress(const Buffer& uncompressed, Buffer& storage);

        static Buffer Decompress(const Buffer& compressed);
    };
} // namespace fuujin
