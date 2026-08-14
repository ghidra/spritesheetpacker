#include "sprite.h"

#include <stb_image.h>

#include <cstring>

bool sprite_load_image(Sprite& sprite, const std::string& abs_path, SDL_Renderer* renderer) {
    int w, h, channels;
    stbi_uc* pixels = stbi_load(abs_path.c_str(), &w, &h, &channels, 4);
    if (!pixels) {
        sprite.load_error = std::string("cannot read PNG: ") + (stbi_failure_reason() ?: "unknown");
        sprite.loaded = false;
        return false;
    }
    sprite.w = w;
    sprite.h = h;
    sprite.pixels.assign(pixels, pixels + (size_t)w * (size_t)h * 4);
    stbi_image_free(pixels);

    sprite_free_texture(sprite);

    if (renderer && !sprite_texture_from_pixels(sprite, renderer)) {
        sprite.loaded = false;
        return false;
    }

    sprite.loaded = true;
    sprite.load_error.clear();
    sprite.ensure_frames();
    return true;
}

bool sprite_texture_from_pixels(Sprite& sprite, SDL_Renderer* renderer) {
    sprite_free_texture(sprite);
    if (!renderer || sprite.pixels.empty() || sprite.w <= 0 || sprite.h <= 0) return false;
    SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STATIC, sprite.w, sprite.h);
    if (!tex) {
        sprite.load_error = std::string("SDL_CreateTexture: ") + SDL_GetError();
        return false;
    }
    SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
    SDL_UpdateTexture(tex, nullptr, sprite.pixels.data(), sprite.w * 4);
    sprite.texture = tex;
    return true;
}

void sprite_free_texture(Sprite& sprite) {
    if (sprite.texture) {
        SDL_DestroyTexture(sprite.texture);
        sprite.texture = nullptr;
    }
}
