#pragma once
#include "fuujin/core/Ref.h"

#include "fuujin/renderer/GraphicsDevice.h"
#include "fuujin/renderer/DeviceImage.h"

#include "fuujin/core/Buffer.h"

namespace fuujin {
    class CommandList;
    class DeviceBuffer : public RefCounted {
    public:
        enum class Usage {
            Vertex,
            Index,
            Uniform,
            Storage,
            Staging,
        };

        struct Spec {
            size_t Size;
            Usage BufferUsage;
            std::unordered_set<QueueType> QueueOwnership;
        };

        virtual const Spec& GetSpec() const = 0;

        virtual Buffer RT_Map() = 0;
        virtual void RT_Unmap() = 0;

        virtual void RT_CopyToBuffer(CommandList& cmdlist, const Ref<DeviceBuffer>& destination,
                                     size_t size = 0, size_t srcOffset = 0,
                                     size_t dstOffset = 0) const = 0;

        virtual void RT_CopyToImage(CommandList& cmdlist, const Ref<DeviceImage>& destination,
                                    size_t offset = 0, const ImageCopy& copy = {}) const = 0;
    };
} // namespace fuujin