#pragma once

namespace fuujin {
    /*
     * Spacially-safe representation of a block of memory. Can either allocate a new block or wrap
     * around existing memory.
     */
    class Buffer {
    public:
        static Buffer Wrapper(void* data, size_t size);
        static const Buffer Wrapper(const void* data, size_t size);

        template <typename _Ty>
        static Buffer Wrapper(std::vector<_Ty>& data) {
            return Wrapper(data.data(), data.size() * sizeof(_Ty));
        }

        template <typename _Ty>
        static const Buffer Wrapper(const std::vector<_Ty>& data) {
            return Wrapper(data.data(), data.size() * sizeof(_Ty));
        }

        static Buffer CreateCopy(const void* data, size_t size);

        static void Copy(const Buffer& src, Buffer& dst, size_t size = 0);
        static void Copy(const Buffer& src, Buffer&& dst, size_t size = 0);

        Buffer();
        Buffer(size_t size);
        ~Buffer();

        Buffer(std::nullptr_t);
        Buffer& operator=(std::nullptr_t);

        Buffer(const Buffer& src);
        Buffer& operator=(const Buffer& src);

        Buffer(Buffer&& src);
        Buffer& operator=(Buffer&& src);

        // creates a slice of the buffer
        // note that this returns a wrapper! keep the original buffer unless you copy the slice
        Buffer Slice(size_t offset, size_t size = 0);
        const Buffer Slice(size_t offset, size_t size = 0) const;

        Buffer Copy() const;

        void* Get() { return m_Buffer; }
        const void* Get() const { return m_Buffer; }

        size_t GetSize() const { return m_Size; }
        bool IsMemoryOwned() const { return m_Owned; }

        bool IsPresent() const { return m_Buffer != nullptr; }
        bool IsEmpty() const { return m_Buffer == nullptr; }

        operator bool() const { return IsPresent(); }

        template <typename _Ty>
        _Ty* As() {
            return (_Ty*)m_Buffer;
        }

        template <typename _Ty>
        const _Ty* As() const {
            return (const _Ty*)m_Buffer;
        }

    private:
        void Allocate(size_t size);
        void Free();
        void Release();

        void* m_Buffer;
        size_t m_Size;
        bool m_Owned;
    };
} // namespace fuujin
