#include "core/Gfx.h"

namespace mm2 {

namespace {

// Decode `outBytes` bytes of the nibble-RLE plane stream starting at `off`.
// Returns the next offset, or SIZE_MAX on EOF.
size_t decodePlaneStream(const Bytes& data, size_t off, size_t outBytes, std::vector<uint8_t>& out) {
    out.clear();
    out.reserve(outBytes);
    bool havePending = false;
    uint8_t pending = 0;
    auto emit = [&](uint8_t nib) {
        if (out.size() >= outBytes) return;
        if (!havePending) {
            pending = nib & 0x0F;
            havePending = true;
        } else {
            out.push_back(static_cast<uint8_t>((pending << 4) | (nib & 0x0F)));
            havePending = false;
        }
    };

    size_t cur = off;
    while (out.size() < outBytes) {
        if (cur >= data.size()) return SIZE_MAX;
        uint8_t p = data[cur++];
        uint8_t cmd = p & 0xF0;
        if (cmd == 0x00 || cmd == 0xF0) {
            uint8_t nib = (p >> 4) & 0x0F;
            int times = (p & 0x0F) + 1;
            for (int i = 0; i < times && out.size() < outBytes; ++i) emit(nib);
        } else {
            emit((p >> 4) & 0x0F);
            emit(p & 0x0F);
        }
    }
    return cur;
}

// Locate the FF 00 image-chunk marker; returns offset of the header word
// (the byte after FF), or SIZE_MAX.
size_t findImageChunk(const Bytes& d, size_t from) {
    if (d.size() < 11) return SIZE_MAX;
    for (size_t i = from; i + 10 < d.size(); ++i) {
        if (d[i] != 0xFF || d[i + 1] != 0x00) continue;
        size_t hdr = i + 1;
        int frames = readU16BE(d.data() + hdr);
        int depth = readU16BE(d.data() + hdr + 2);
        int w = readU16BE(d.data() + hdr + 4);
        int h = readU16BE(d.data() + hdr + 6);
        if (frames > 0 && frames < 256 && depth > 0 && depth < 64 && w > 0 && w <= 1024 &&
            h > 0 && h <= 1024)
            return hdr;
    }
    return SIZE_MAX;
}

// Does a plausible image-chunk header begin at `off`? (depth may be 0 for .32)
bool plausibleHeader(const Bytes& d, size_t off) {
    if (off + 12 > d.size()) return false;
    int frames = readU16BE(d.data() + off);
    if (frames < 1 || frames > 1024) return false;
    size_t infoOff = off + 4;
    if (infoOff + static_cast<size_t>(frames) * 6 + 64 > d.size()) return false;
    int w = readU16BE(d.data() + infoOff);
    int h = readU16BE(d.data() + infoOff + 2);
    return w > 0 && w <= 1024 && h > 0 && h <= 1024;
}

}  // namespace

GfxImage gfxDecode(const Bytes& bytes, bool isAnm) {
    GfxImage img;
    if (bytes.size() < 12) {
        img.error = "file too small";
        return img;
    }

    size_t off = 0;
    if (isAnm) {
        // Skip prelude (0x04..0x2F) + sequence; just scan for the marker.
        off = findImageChunk(bytes, 0x33);
        if (off == SIZE_MAX) {
            img.error = "could not find FF 00 image chunk";
            return img;
        }
    } else if (!plausibleHeader(bytes, 0)) {
        // Some .32 files carry a prelude; fall back to the FF 00 marker scan.
        size_t scan = findImageChunk(bytes, 0);
        if (scan != SIZE_MAX) off = scan;
    }
    img.chunkOffset = off;

    img.frameCount = readU16BE(bytes.data() + off);
    img.depth = readU16BE(bytes.data() + off + 2);
    if (img.frameCount <= 0 || img.frameCount > 1024) {
        img.error = "implausible frame count";
        return img;
    }

    size_t infoOff = off + 4;
    if (infoOff + static_cast<size_t>(img.frameCount) * 6 + 64 > bytes.size()) {
        img.error = "truncated header/palette";
        return img;
    }

    struct FI { int w, h, flags; };
    std::vector<FI> fi(img.frameCount);
    for (int i = 0; i < img.frameCount; ++i) {
        const uint8_t* p = bytes.data() + infoOff + i * 6;
        fi[i] = {readU16BE(p), readU16BE(p + 2), readU16BE(p + 4)};
    }

    size_t palOff = infoOff + static_cast<size_t>(img.frameCount) * 6;
    for (int i = 0; i < kGfxPaletteColors; ++i) {
        uint16_t w = readU16BE(bytes.data() + palOff + i * 2);
        uint8_t r = static_cast<uint8_t>(((w >> 8) & 0x0F) * 17);
        uint8_t g = static_cast<uint8_t>(((w >> 4) & 0x0F) * 17);
        uint8_t b = static_cast<uint8_t>((w & 0x0F) * 17);
        img.palette[i][0] = r;
        img.palette[i][1] = g;
        img.palette[i][2] = b;
        // Pen 0 is the blit mask colour (JSR -$7C20 skips index 0).
        img.palette[i][3] = (i == 0) ? 0 : 255;
    }

    size_t cur = palOff + 64;
    std::vector<uint8_t> planes;
    img.frames.resize(img.frameCount);
    for (int f = 0; f < img.frameCount; ++f) {
        int w = fi[f].w, h = fi[f].h;
        GfxFrame& gf = img.frames[f];
        gf.width = w;
        gf.height = h;
        gf.flags = fi[f].flags;
        if (w <= 0 || h <= 0 || w > 1024 || h > 1024) {
            img.error = "implausible frame size";
            img.frameCount = f;  // keep frames decoded so far
            img.frames.resize(f);
            img.ok = (f > 0);
            return img;
        }
        int rs = gfxRassize(w, h);
        size_t frameBytes = static_cast<size_t>(kGfxPlanes) * rs;
        size_t next = decodePlaneStream(bytes, cur, frameBytes, planes);
        if (next == SIZE_MAX) {
            img.error = "EOF in pixel stream";
            img.frameCount = f;
            img.frames.resize(f);
            img.ok = (f > 0);
            return img;
        }
        cur = next;

        int bpr = ((w + 15) >> 3) & 0xFFFE;  // bytes per row, word aligned
        gf.rgba.assign(static_cast<size_t>(w) * h * 4, 0);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                int idx = 0;
                for (int pl = 0; pl < kGfxPlanes; ++pl) {
                    int bytePos = pl * rs + y * bpr + (x >> 3);
                    int bit = (planes[bytePos] >> (7 - (x & 7))) & 1;
                    idx |= bit << pl;
                }
                uint8_t* o = &gf.rgba[(static_cast<size_t>(y) * w + x) * 4];
                o[0] = img.palette[idx][0];
                o[1] = img.palette[idx][1];
                o[2] = img.palette[idx][2];
                o[3] = (idx == 0) ? 0 : img.palette[idx][3];
            }
        }
    }

    img.ok = true;
    return img;
}

bool gfxLoad(const std::string& path, bool isAnm, GfxImage& out) {
    Bytes b;
    if (!readFile(path, b)) {
        out.clear();
        out.error = "read failed";
        return false;
    }
    out = gfxDecode(b, isAnm);
    return out.ok;
}

}  // namespace mm2
