#pragma once
// Tiny RGBA -> OpenGL texture helper for previewing decoded graphics.

#include <cstdint>

namespace mm2 {

// Upload an RGBA8 buffer as a GL_NEAREST 2D texture. Returns the texture id
// (cast to ImTextureID for ImGui::Image), or 0 on failure.
unsigned int makeTextureRGBA(const uint8_t* rgba, int w, int h);
// Replace full texture contents (same dimensions as created texture).
void uploadTextureRGBA(unsigned int tex, const uint8_t* rgba, int w, int h);
void freeTexture(unsigned int tex);

}  // namespace mm2
