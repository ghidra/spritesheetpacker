#include "exporter.h"

#include <stb_image_write.h>

#include <filesystem>
#include <stdexcept>
#include <vector>

namespace fs = std::filesystem;

namespace exporter {

// Standard "source-over" alpha compositing.
static inline void blend_pixel(unsigned char* dst, const unsigned char* src) {
    unsigned int sa = src[3];
    if (sa == 0) return;
    if (sa == 255) {
        dst[0] = src[0]; dst[1] = src[1]; dst[2] = src[2]; dst[3] = 255;
        return;
    }
    unsigned int da = dst[3];
    unsigned int out_a = sa + (da * (255 - sa) + 127) / 255;
    if (out_a == 0) { dst[0] = dst[1] = dst[2] = dst[3] = 0; return; }
    for (int c = 0; c < 3; ++c) {
        unsigned int sc = src[c] * sa;
        unsigned int dc = dst[c] * da * (255 - sa) / 255;
        dst[c] = (unsigned char)((sc + dc) / out_a);
    }
    dst[3] = (unsigned char)out_a;
}

void export_png(const Project& project, const std::string& abs_png_path,
                const std::vector<const std::vector<unsigned char>*>* pixel_override) {
    int W = project.sheet_w;
    int H = project.sheet_h > 0 ? project.sheet_h : 1;
    if (W <= 0) throw std::runtime_error("sheet width is 0 — nothing to export");

    std::vector<unsigned char> buf((size_t)W * (size_t)H * 4, 0);

    for (size_t si = 0; si < project.sprites.size(); ++si) {
        const Sprite& s = project.sprites[si];
        const std::vector<unsigned char>* pxp = &s.pixels;
        if (pixel_override)
            pxp = si < pixel_override->size() ? (*pixel_override)[si] : nullptr;
        if (!pxp || pxp->empty()) continue;
        const std::vector<unsigned char>& pixels = *pxp;
        // The cell is what was packed; the art is blitted at its anchor inside
        // it, leaving the margin transparent.
        int fw = s.frame_w(), fh = s.frame_h();
        int aw = s.art_w(), ah = s.art_h();
        int ox = s.art_off_x(), oy = s.art_off_y();
        for (int i = 0; i < s.frame_count(); ++i) {
            const FramePlacement& f = s.frames[i];
            if (f.x < 0 || f.y < 0) continue;
            if (f.x + fw > W || f.y + fh > H)
                throw std::runtime_error(s.frame_id(i) +
                                         ": rectangle exceeds sheet bounds during export");
            int sx = s.frame_src_x(i), sy = s.frame_src_y(i);
            for (int row = 0; row < ah; ++row) {
                const unsigned char* src_row =
                    &pixels[(((size_t)(sy + row) * (size_t)s.w) + sx) * 4];
                unsigned char* dst_row =
                    &buf[((size_t)(f.y + oy + row) * (size_t)W + f.x + ox) * 4];
                for (int col = 0; col < aw; ++col)
                    blend_pixel(dst_row + col * 4, src_row + col * 4);
            }
        }
    }

    fs::create_directories(fs::path(abs_png_path).parent_path());
    stbi_write_png_compression_level = 9;
    if (!stbi_write_png(abs_png_path.c_str(), W, H, 4, buf.data(), W * 4))
        throw std::runtime_error("stbi_write_png failed for " + abs_png_path);
}

}  // namespace exporter
