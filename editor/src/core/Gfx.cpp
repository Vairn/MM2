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

// globe.32 / disk.32 ship XOR-obfuscated on disk (runtime decrypt @ asm 0x2613E
// uses a 5-byte rolling key; globe round-trips with a 10-byte repeating key).
// Recover the key by assuming a 1-frame, depth-0 chunk whose pixel payload
// exactly fills the file, then decode normally.
bool tryDecryptXor32(Bytes& bytes) {
    if (bytes.size() < 74 || plausibleHeader(bytes, 0)) return false;
    const size_t rs = (bytes.size() - 74) / kGfxPlanes;
    if (74 + kGfxPlanes * rs != bytes.size()) return false;

    int bestW = 0;
    int bestMinDim = 0;
    Bytes bestDec;

    for (int bpr = 2; bpr <= 512; bpr += 2) {
        if (rs % static_cast<size_t>(bpr)) continue;
        const int h = static_cast<int>(rs / bpr);
        if (h < 16 || h > 1024) continue;
        for (int w = 1; w <= 1024; ++w) {
            if (w < 16 || gfxRassize(w, h) != static_cast<int>(rs)) continue;

            uint8_t plain[10];
            writeU16BE(plain + 0, 1);
            writeU16BE(plain + 2, 0);
            writeU16BE(plain + 4, static_cast<uint16_t>(w));
            writeU16BE(plain + 6, static_cast<uint16_t>(h));
            writeU16BE(plain + 8, 0);

            uint8_t key[10];
            for (int i = 0; i < 10; ++i) key[i] = static_cast<uint8_t>(bytes[i] ^ plain[i]);

            Bytes dec(bytes.size());
            for (size_t i = 0; i < bytes.size(); ++i)
                dec[i] = static_cast<uint8_t>(bytes[i] ^ key[i % 10]);
            if (!plausibleHeader(dec, 0) || readU16BE(dec.data()) != 1) continue;
            if (readU16BE(dec.data() + 4) != static_cast<uint16_t>(w) ||
                readU16BE(dec.data() + 6) != static_cast<uint16_t>(h))
                continue;

            const int minDim = (w < h) ? w : h;
            if (minDim > bestMinDim || (minDim == bestMinDim && w > bestW)) {
                bestMinDim = minDim;
                bestW = w;
                bestDec = std::move(dec);
            }
        }
    }

    if (bestMinDim <= 0) return false;
    bytes = std::move(bestDec);
    return true;
}

}  // namespace

GfxImage gfxDecode(const Bytes& bytesIn, bool isAnm) {
    GfxImage img;
    Bytes bytes = bytesIn;
    if (bytes.size() < 12) {
        img.error = "file too small";
        return img;
    }

    if (!isAnm && !plausibleHeader(bytes, 0)) tryDecryptXor32(bytes);

    size_t off = 0;
    if (isAnm) {
        // Parse fixed TV prelude table (11 x 4-byte descriptors at 0x04..0x2F).
        img.preludeEntries.resize(11);
        if (bytes.size() >= 0x30) {
            for (int i = 0; i < 11; ++i) {
                size_t po = 0x04 + static_cast<size_t>(i) * 4;
                GfxAnimPreludeEntry& e = img.preludeEntries[i];
                e.xOffset = bytes[po + 0];
                e.yOffset = bytes[po + 1];
                e.width = bytes[po + 2];
                e.height = bytes[po + 3];
                e.used = !(bytes[po + 0] == 0xFF && bytes[po + 1] == 0xFF && bytes[po + 2] == 0xFF &&
                           bytes[po + 3] == 0xFF);
            }
        }
        if (bytes.size() >= 0x33) {
            img.seqHeaderA = bytes[0x30];
            img.seqHeaderB = bytes[0x31];
            img.seqHeaderC = bytes[0x32];
        }

        // Skip prelude (0x04..0x2F) + sequence; just scan for the marker.
        off = findImageChunk(bytes, 0x33);
        if (off == SIZE_MAX) {
            img.error = "could not find FF 00 image chunk";
            return img;
        }

        // Parse sequence stream bytes between header (0x33) and image marker.
        // Blocks are delimited by 0xFF and usually hold (frame,delay) pairs.
        size_t seqStart = 0x33;
        size_t seqEnd = (off > 0) ? (off - 1) : 0;
        if (seqStart < bytes.size() && seqStart < seqEnd) {
            // Some files (e.g. 61.anm) contain more FF-delimited sequence blocks
            // than (seqHeaderB & 0x7F). Treat header count as a hint only and
            // keep scanning until the image-chunk marker.
            size_t cur = seqStart;
            while (cur < seqEnd) {
                while (cur < seqEnd && bytes[cur] != 0xFF) ++cur;
                if (cur >= seqEnd) break;
                ++cur;  // consume block delimiter
                size_t begin = cur;
                while (cur < seqEnd && bytes[cur] != 0xFF) ++cur;
                if (begin < cur) {
                    img.sequences.emplace_back(bytes.begin() + static_cast<std::ptrdiff_t>(begin),
                                               bytes.begin() + static_cast<std::ptrdiff_t>(cur));
                }
            }
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

namespace {

void blitOpaqueRgba(const GfxFrame& fr, int dstX, int dstY, int copyW, int copyH, int canvasW, int canvasH,
                    std::vector<uint8_t>& rgba) {
    if (copyW <= 0 || copyH <= 0) return;
    if (copyW > fr.width) copyW = fr.width;
    if (copyH > fr.height) copyH = fr.height;
    for (int y = 0; y < copyH; ++y) {
        const int oy = dstY + y;
        if (oy < 0 || oy >= canvasH) continue;
        for (int x = 0; x < copyW; ++x) {
            const int ox = dstX + x;
            if (ox < 0 || ox >= canvasW) continue;
            const uint8_t* src = &fr.rgba[(static_cast<size_t>(y) * fr.width + x) * 4];
            if (src[3] == 0) continue;
            uint8_t* dst = &rgba[(static_cast<size_t>(oy) * canvasW + ox) * 4];
            dst[0] = src[0];
            dst[1] = src[1];
            dst[2] = src[2];
            dst[3] = src[3];
        }
    }
}

void clearRectRgba(int dstX, int dstY, int rectW, int rectH, int canvasW, int canvasH, std::vector<uint8_t>& rgba) {
    if (rectW <= 0 || rectH <= 0) return;
    for (int y = 0; y < rectH; ++y) {
        const int oy = dstY + y;
        if (oy < 0 || oy >= canvasH) continue;
        for (int x = 0; x < rectW; ++x) {
            const int ox = dstX + x;
            if (ox < 0 || ox >= canvasW) continue;
            uint8_t* dst = &rgba[(static_cast<size_t>(oy) * canvasW + ox) * 4];
            dst[0] = dst[1] = dst[2] = dst[3] = 0;
        }
    }
}

}  // namespace

GfxAnmCanvas gfxAnmCompositeCanvas(const GfxImage& img) {
    GfxAnmCanvas c;
    const int n = static_cast<int>(img.frames.size());
    if (n <= 0) return c;

    int minX = 0;
    int minY = 0;
    int maxX = img.frames[0].width;
    int maxY = img.frames[0].height;
    for (int i = 1; i < n; ++i) {
        const int preIdx = i - 1;
        if (preIdx < 0 || preIdx >= static_cast<int>(img.preludeEntries.size())) continue;
        const GfxAnimPreludeEntry& pe = img.preludeEntries[preIdx];
        if (!pe.used) continue;
        const GfxFrame& fr = img.frames[i];
        int w = (pe.width > 0 && pe.width < fr.width) ? pe.width : fr.width;
        int h = (pe.height > 0 && pe.height < fr.height) ? pe.height : fr.height;
        if (w <= 0 || h <= 0) continue;
        const int x = pe.xOffset;
        const int y = pe.yOffset;
        if (x < minX) minX = x;
        if (y < minY) minY = y;
        if (x + w > maxX) maxX = x + w;
        if (y + h > maxY) maxY = y + h;
    }
    c.minX = minX;
    c.minY = minY;
    c.width = maxX - minX;
    c.height = maxY - minY;
    c.valid = c.width > 0 && c.height > 0;
    return c;
}

bool gfxAnmCompositeFrame(const GfxImage& img, int frameIdx, std::vector<uint8_t>& rgba, const GfxAnmCanvas* canvas) {
    const int n = static_cast<int>(img.frames.size());
    if (n <= 0 || frameIdx < 0 || frameIdx >= n) return false;

    GfxAnmCanvas local;
    const GfxAnmCanvas& c = canvas ? *canvas : (local = gfxAnmCompositeCanvas(img));
    if (!c.valid) return false;

    rgba.assign(static_cast<size_t>(c.width) * c.height * 4, 0);
    const GfxFrame& base = img.frames[0];
    blitOpaqueRgba(base, -c.minX, -c.minY, base.width, base.height, c.width, c.height, rgba);

    if (frameIdx > 0) {
        const GfxFrame& fr = img.frames[frameIdx];
        int x = -c.minX;
        int y = -c.minY;
        int copyW = fr.width;
        int copyH = fr.height;
        const int preIdx = frameIdx - 1;
        if (preIdx >= 0 && preIdx < static_cast<int>(img.preludeEntries.size()) &&
            img.preludeEntries[preIdx].used) {
            const GfxAnimPreludeEntry& pe = img.preludeEntries[preIdx];
            x = pe.xOffset - c.minX;
            y = pe.yOffset - c.minY;
            if (pe.width > 0 && pe.width < copyW) copyW = pe.width;
            if (pe.height > 0 && pe.height < copyH) copyH = pe.height;
        }
        clearRectRgba(x, y, copyW, copyH, c.width, c.height, rgba);
        blitOpaqueRgba(fr, x, y, copyW, copyH, c.width, c.height, rgba);
    }
    return true;
}

bool gfxAnmHasSequencePlayback(const GfxImage& img) {
    for (const auto& seq : img.sequences) {
        if (seq.size() >= 2) return true;
    }
    return false;
}

int gfxAnmSequenceFrameAt(const GfxImage& img, int seqIndex, int step) {
    if (seqIndex < 0 || seqIndex >= static_cast<int>(img.sequences.size())) return -1;
    const auto& seq = img.sequences[seqIndex];
    if (seq.size() < 2) return -1;
    const int pairCount = static_cast<int>(seq.size() / 2);
    if (pairCount <= 0) return -1;
    if (step < 0) step = 0;
    if (step >= pairCount) step = pairCount - 1;
    return static_cast<int>(seq[static_cast<size_t>(step) * 2]);
}

float gfxAnmSequenceStepDurationSec(const GfxImage& img, int seqIndex, int step, float speed) {
    if (seqIndex < 0 || seqIndex >= static_cast<int>(img.sequences.size()) || speed <= 0.0f) return 0.10f;
    const auto& seq = img.sequences[seqIndex];
    if (seq.size() < 2) return 0.10f;
    const int pairCount = static_cast<int>(seq.size() / 2);
    if (pairCount <= 0) return 0.10f;
    if (step < 0) step = 0;
    if (step >= pairCount) step = pairCount - 1;
    const int delayTicks = static_cast<int>(seq[static_cast<size_t>(step) * 2 + 1]);
    const float base = static_cast<float>(delayTicks > 0 ? delayTicks : 1) / 60.0f;
    return base / speed;
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

namespace {

// Pack one frame's indexed pixels into 5 concatenated Amiga bitplanes.
std::vector<uint8_t> packFramePlanes(const GfxEncodeFrame& fr) {
    const int bpr = (((fr.width + 15) >> 3) & 0xFFFE);
    const int rs = fr.height * bpr;
    std::vector<uint8_t> planes(static_cast<size_t>(kGfxPlanes) * rs, 0);
    for (int y = 0; y < fr.height; ++y) {
        for (int x = 0; x < fr.width; ++x) {
            const uint8_t v = fr.indices[static_cast<size_t>(y) * fr.width + x] & 0x1F;
            if (v == 0) continue;
            const int byteOff = y * bpr + (x >> 3);
            const int bit = 7 - (x & 7);
            for (int pl = 0; pl < kGfxPlanes; ++pl) {
                if ((v >> pl) & 1) planes[static_cast<size_t>(pl) * rs + byteOff] |= (1u << bit);
            }
        }
    }
    return planes;
}

// Encode a byte stream to the .32 nibble-RLE format (runs only for 0x0/0xF).
void rleEncodeNibbles(const std::vector<uint8_t>& stream, Bytes& out) {
    std::vector<uint8_t> nibs(stream.size() * 2);
    for (size_t i = 0; i < stream.size(); ++i) {
        nibs[2 * i] = (stream[i] >> 4) & 0x0F;
        nibs[2 * i + 1] = stream[i] & 0x0F;
    }
    const size_t n = nibs.size();
    size_t i = 0;
    while (i < n) {
        const uint8_t v = nibs[i];
        if (v == 0x0 || v == 0xF) {
            size_t j = i + 1;
            while (j < n && nibs[j] == v && (j - i) < 16) ++j;
            out.push_back(static_cast<uint8_t>((v << 4) | (j - i - 1)));
            i = j;
        } else {
            const uint8_t lo = (i + 1 < n) ? nibs[i + 1] : 0;
            out.push_back(static_cast<uint8_t>((v << 4) | lo));
            i += 2;
        }
    }
}

}  // namespace

Bytes gfxEncode32(const std::vector<GfxEncodeFrame>& frames,
                  const uint8_t palette[kGfxPaletteColors][4], int depth) {
    Bytes out;
    out.resize(4);
    writeU16BE(out.data(), static_cast<uint16_t>(frames.size()));
    writeU16BE(out.data() + 2, static_cast<uint16_t>(depth));
    for (const auto& fr : frames) {
        uint8_t hdr[6];
        writeU16BE(hdr, static_cast<uint16_t>(fr.width));
        writeU16BE(hdr + 2, static_cast<uint16_t>(fr.height));
        writeU16BE(hdr + 4, static_cast<uint16_t>(fr.flags));
        out.insert(out.end(), hdr, hdr + 6);
    }
    for (int i = 0; i < kGfxPaletteColors; ++i) {
        const uint16_t packed = static_cast<uint16_t>(
            ((palette[i][0] / 17) << 8) | ((palette[i][1] / 17) << 4) | (palette[i][2] / 17));
        uint8_t pw[2];
        writeU16BE(pw, packed);
        out.insert(out.end(), pw, pw + 2);
    }
    for (const auto& fr : frames) {
        rleEncodeNibbles(packFramePlanes(fr), out);
    }
    return out;
}

Bytes gfxEncode32FromImage(const GfxImage& img) {
    std::vector<GfxEncodeFrame> frames;
    frames.reserve(img.frames.size());
    for (const auto& f : img.frames) {
        GfxEncodeFrame ef;
        ef.width = f.width;
        ef.height = f.height;
        ef.flags = f.flags;
        ef.indices.resize(static_cast<size_t>(f.width) * f.height, 0);
        for (size_t p = 0; p < ef.indices.size(); ++p) {
            const uint8_t* px = &f.rgba[p * 4];
            if (px[3] == 0) {
                ef.indices[p] = 0;  // transparent -> pen 0
                continue;
            }
            int best = 0;
            int bestDist = 0x7fffffff;
            for (int c = 0; c < kGfxPaletteColors; ++c) {
                const int dr = px[0] - img.palette[c][0];
                const int dg = px[1] - img.palette[c][1];
                const int db = px[2] - img.palette[c][2];
                const int dist = dr * dr + dg * dg + db * db;
                if (dist < bestDist) {
                    bestDist = dist;
                    best = c;
                }
            }
            ef.indices[p] = static_cast<uint8_t>(best);
        }
        frames.push_back(std::move(ef));
    }
    return gfxEncode32(frames, img.palette, img.depth);
}

bool gfxSave32(const std::string& path, const GfxImage& img) {
    if (!img.ok || img.frames.empty()) return false;
    return writeFile(path, gfxEncode32FromImage(img));
}

}  // namespace mm2
