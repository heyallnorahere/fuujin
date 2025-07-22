#include "fuujinpch.h"
#include "fuujin/core/Compression.h"

#include <zstd.h>

namespace fuujin {
    struct CompressionData {
        ZSTD_CCtx* CCtx;
        ZSTD_DCtx* DCtx;
        size_t RefCount;
    };

    static std::unique_ptr<CompressionData> s_Data;

    void Compression::Init() {
        ZoneScoped;

        if (s_Data) {
            FUUJIN_DEBUG("Initialized compression again - increasing ref count");

            s_Data->RefCount++;
            return;
        }

        s_Data = std::make_unique<CompressionData>();
        s_Data->CCtx = nullptr;
        s_Data->DCtx = nullptr;
        s_Data->RefCount = 1;
    }

    void Compression::Shutdown() {
        ZoneScoped;

        if (!s_Data) {
            return;
        }

        s_Data->RefCount--;
        if (s_Data->RefCount > 0) {
            FUUJIN_DEBUG("Not destroying compression contexts yet - ZSTD still in use");
            return;
        }

        if (s_Data->CCtx != nullptr) {
            FUUJIN_DEBUG("Freeing ZSTD compression context");
            ZSTD_freeCCtx(s_Data->CCtx);
        }

        if (s_Data->DCtx != nullptr) {
            FUUJIN_DEBUG("Freeing ZSTD decompression context");
            ZSTD_freeDCtx(s_Data->DCtx);
        }

        s_Data.reset();
    }

    Buffer Compression::Compress(const Buffer& uncompressed, Buffer& storage) {
        ZoneScoped;

        if (uncompressed.IsEmpty()) {
            FUUJIN_WARN("Attempted to compress empty buffer - returning empty buffer");
            return Buffer();
        }

        if (s_Data->CCtx == nullptr) {
            FUUJIN_DEBUG("Creating compression context");

            s_Data->CCtx = ZSTD_createCCtx();
            ZSTD_CCtx_setParameter(s_Data->CCtx, ZSTD_c_strategy, ZSTD_fast);
        }

        size_t uncompressedSize = uncompressed.GetSize();
        size_t capacity = ZSTD_compressBound(uncompressedSize);
        storage = Buffer(capacity);

        size_t compressedSize = ZSTD_compress2(s_Data->CCtx, storage.Get(), capacity,
                                               uncompressed.Get(), uncompressedSize);

        if (compressedSize == 0) {
            return Buffer();
        }

        return storage.Slice(0, compressedSize);
    }

    Buffer Compression::Decompress(const Buffer& compressed) {
        ZoneScoped;

        if (compressed.IsEmpty()) {
            FUUJIN_WARN("Attempted to decompress empty buffer - returning empty buffer");
            return Buffer();
        }

        if (s_Data->DCtx == nullptr) {
            FUUJIN_DEBUG("Creating decompression context");

            s_Data->DCtx = ZSTD_createDCtx();
        }

        const void* compressedData = compressed.Get();
        size_t compressedSize = compressed.GetSize();

        unsigned long long expectedSize = ZSTD_getFrameContentSize(compressedData, compressedSize);
        if (expectedSize == ZSTD_CONTENTSIZE_UNKNOWN || expectedSize == ZSTD_CONTENTSIZE_ERROR) {
            FUUJIN_ERROR("Failed to determine decompressed size of chunk!");
            return Buffer();
        }

        if (expectedSize == 0) {
            return Buffer();
        }

        Buffer decompressed((size_t)expectedSize);
        size_t decompressedSize =
            ZSTD_decompressDCtx(s_Data->DCtx, decompressed.Get(), decompressed.GetSize(),
                                compressedData, compressedSize);

        if (decompressedSize != decompressed.GetSize()) {
            FUUJIN_ERROR("Decompressed size differs from expected size!");
            return Buffer();
        }

        return decompressed;
    }
} // namespace fuujin
