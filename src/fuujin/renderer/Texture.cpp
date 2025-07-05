#include "fuujinpch.h"
#include "fuujin/renderer/Texture.h"

#include "fuujin/renderer/Renderer.h"

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_SIMD
#define STBI_MALLOC(size) ::fuujin::allocate(size)
#define STBI_REALLOC(block, size) ::fuujin::reallocate(block, size)
#define STBI_FREE(block) ::fuujin::freemem(block)
#include <stb_image.h>

namespace fuujin {
    // as supported by the currently-used version of stb_image.h
    static const std::vector<std::string> s_ImageExtensions = {
        "jpg", "jpeg", "png", "tga", "bmp", "psd", "gif", // only static images
        "hdr", "pic",  "pnm"
    };

    Ref<Asset> TextureSerializer::Deserialize(const fs::path& path) const {
        ZoneScoped;
        stbi_set_flip_vertically_on_load(true);

        auto pathString = path.lexically_normal().string();
        FUUJIN_INFO("Loading 2D texture from path: {}", pathString.c_str());

        int width, height, channels;
        auto dataRaw = stbi_load(pathString.c_str(), &width, &height, &channels, 4);

        if (dataRaw == nullptr) {
            FUUJIN_ERROR("Failed to open path {} as an image", pathString.c_str());
            return nullptr;
        }

        Texture::Format format;
        switch (channels) {
        case 3:
            format = Texture::Format::RGB8;
            break;
        case 4:
            format = Texture::Format::RGBA8;
            break;
        default:
            throw std::runtime_error("Invalid channel count!");
        }

        size_t dataSize = (size_t)width * height * channels;
        auto texture = Renderer::CreateTexture((uint32_t)width, (uint32_t)height, format,
                                               Buffer::Wrapper(dataRaw, dataSize), {}, path);

        FUUJIN_INFO("Loaded image at path {} to 2D texture", pathString.c_str());
        stbi_image_free(dataRaw);

        return texture;
    }

    bool TextureSerializer::Serialize(const Ref<Asset>& asset) const {
        ZoneScoped;

        FUUJIN_ERROR("Image writing is currently not supported!");
        return false;
    }

    const std::vector<std::string>& TextureSerializer::GetExtensions() const {
        return s_ImageExtensions;
    }

    AssetType TextureSerializer::GetType() const { return AssetType::Texture; }
} // namespace fuujin