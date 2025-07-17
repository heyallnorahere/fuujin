#include "fuujinpch.h"
#include "fuujin/core/Buffer.h"

namespace fuujin {
    Buffer Buffer::Wrapper(void* data, size_t size) {
        ZoneScoped;

        Buffer newBuffer;
        newBuffer.m_Buffer = data;
        newBuffer.m_Size = size;
        newBuffer.m_Owned = false;

        return std::move(newBuffer);
    }

    const Buffer Buffer::Wrapper(const void* data, size_t size) {
        ZoneScoped;

        Buffer newBuffer;
        newBuffer.m_Buffer = (void*)data;
        newBuffer.m_Size = size;
        newBuffer.m_Owned = false;

        return std::move(newBuffer);
    }

    Buffer Buffer::CreateCopy(const void* data, size_t size) {
        ZoneScoped;

        Buffer newBuffer(size);
        Copy(Wrapper(data, size), newBuffer);

        return std::move(newBuffer);
    }

    void Buffer::Copy(const Buffer& src, Buffer& dst, size_t size) {
        ZoneScoped;

        size_t copySize = size > 0 ? size : std::min(src.m_Size, dst.m_Size);
        if (copySize > src.m_Size || copySize > dst.m_Size) {
            throw std::runtime_error("Attempted to copy out of bounds!");
        }

        std::memcpy(dst.m_Buffer, src.m_Buffer, copySize);
    }

    void Buffer::Copy(const Buffer& src, Buffer&& dst, size_t size) {
        ZoneScoped;

        size_t copySize = size > 0 ? size : std::min(src.m_Size, dst.m_Size);
        if (copySize > src.m_Size || copySize > dst.m_Size) {
            throw std::runtime_error("Attempted to copy out of bounds!");
        }

        std::memcpy(dst.m_Buffer, src.m_Buffer, copySize);
    }

    Buffer::Buffer() {
        m_Owned = false;
        Release();
    }

    Buffer::Buffer(size_t size) { Allocate(size); }
    Buffer::~Buffer() { Release(); }

    Buffer::Buffer(std::nullptr_t) { Release(); }

    Buffer& Buffer::operator=(std::nullptr_t) {
        Release();
        return *this;
    }

    Buffer::Buffer(const Buffer& src) {
        if (src) {
            Allocate(src.m_Size);
            Copy(src, *this);
        } else {
            Release();
        }
    }

    Buffer& Buffer::operator=(const Buffer& src) {
        Release();

        if (src) {
            Allocate(src.m_Size);
            Copy(src, *this);
        }

        return *this;
    }

    Buffer::Buffer(Buffer&& src) {
        m_Buffer = src.m_Buffer;
        m_Size = src.m_Size;

        m_Owned = src.m_Owned;
        src.m_Owned = false;
    }

    Buffer& Buffer::operator=(Buffer&& src) {
        Release();

        m_Buffer = src.m_Buffer;
        m_Size = src.m_Size;

        m_Owned = src.m_Owned;
        src.m_Owned = false;

        return *this;
    }

    Buffer Buffer::Slice(size_t offset, size_t size) {
        ZoneScoped;
        if (offset + size > m_Size) {
            throw std::runtime_error("Attempted to slice out of bounds!");
        }

        auto slicePtr = (void*)((size_t)m_Buffer + offset);
        auto sliceSize = size > 0 ? size : m_Size - offset;

        return Wrapper(slicePtr, sliceSize);
    }

    const Buffer Buffer::Slice(size_t offset, size_t size) const {
        ZoneScoped;
        if (offset + size > m_Size) {
            throw std::runtime_error("Attempted to slice out of bounds!");
        }

        auto slicePtr = (const void*)((size_t)m_Buffer + offset);
        auto sliceSize = size > 0 ? size : m_Size - offset;

        return Wrapper(slicePtr, sliceSize);
    }

    Buffer Buffer::Copy() const {
        ZoneScoped;

        if (IsEmpty()) {
            return Buffer();
        }

        Buffer copy(m_Size);
        Copy(*this, copy);

        return copy;
    }

    void Buffer::Allocate(size_t size) {
        ZoneScoped;

        if (size == 0) {
            m_Owned = false;
            Release();

            return;
        }

        m_Buffer = allocate(size);
        m_Size = size;
        m_Owned = true;
    }

    void Buffer::Free() {
        ZoneScoped;

        freemem(m_Buffer);
    }

    void Buffer::Release() {
        ZoneScoped;
        if (m_Owned) {
            Free();
        }

        m_Buffer = nullptr;
        m_Size = 0;
        m_Owned = false;
    }
} // namespace fuujin
