# MM2 Amiga graphics formats: `.32` and `.anm`

All multibyte fields are big-endian.

## Shared bitmap encoding

Decoded frame data is five bitplanes, stored plane-by-plane:

```text
row_bytes  = ((width + 15) >> 3) & 0xFFFE
plane_size = height * row_bytes
frame_size = 5 * plane_size
```

For pixel `(x, y)`:

```text
byte_offset = y * row_bytes + (x >> 3)
bit         = 7 - (x & 7)
index       = bit[0] | (bit[1] << 1) | ... | (bit[4] << 4)
```

Bitplane 0 starts at `0`; plane `p` starts at `p * plane_size`.

### Palette

The palette contains 32 12-bit RGB entries. Each entry is stored in a 16-bit big-endian word; the high nibble is unused:

```text
bits 15..12   0000
bits 11..8    red
bits  7..4    green
bits  3..0    blue
```

The palette occupies 64 bytes:

```text
offset       size       entry
a + 0        2          colour 0
a + 2        2          colour 1
...
a + 62       2          colour 31
```

`a` is the first byte after the frame table. Palette index 0 is the transparent pen. Indices 1–31 are colour pens. To expand a 4-bit channel to 8-bit RGB, use `channel8 = channel4 * 17`.

### Nibble RLE

The compressed stream expands to `frame_size` bytes. Nibbles are consumed high-nibble first.

```text
p = next byte

(p & 0xF0) == 0x00 or 0xF0:
    output (p >> 4), (p & 0x0F) + 1 times

otherwise:
    output (p >> 4)
    output (p & 0x0F)
```

Runs exist only for nibble `0x0` and nibble `0xF`. All other tokens contain two literal nibbles.

## `.32`

Image chunk starts at offset `0`:

```text
offset  size              field
0x00    2                 frame_count
0x02    2                 depth_or_mode
0x04    frame_count * 6   frame table
...     64                palette
...     variable          frame RLE streams
```

Frame table entry:

```text
offset  size  field
+0      2     width
+2      2     height
+4      2     flags
```

Frame `i` is decoded using its own `width`, `height`, and RLE stream. Frames are complete images; no frame compositing is performed. `depth_or_mode` is not the bitplane count. Preserve `flags` and `depth_or_mode` when rewriting files.

`globe.32` and `disk.32` are non-image blobs and do not use this layout.

## `.anm`

An `.anm` file contains a TV header, animation sequences, and the shared image chunk.

```text
offset       size       field
0x00         2          zero
0x02         2          ASCII "TV"
0x04         44         11 patch descriptors
0x30         1          seq_a
0x31         1          seq_b
0x32         1          seq_c
0x33         variable   sequence stream
variable     2          FF 00 marker
marker + 1   variable   image chunk
```

The `FF 00` marker is also the image-chunk header boundary: the second byte (`00`) is the first byte of `frame_count`.

Patch descriptor `i` (four bytes) applies to stored image frame `i + 1`:

```text
+x  x position
+y  y position
+w  clear/blit width
+h  clear/blit height
```

`FF FF FF FF` marks an unused descriptor.

### Sequence stream

```text
(frame_index, delay) ... FF
(frame_index, delay) ... FF
...
FF FF                         optional sequence-list terminator
FF 00                         image-chunk marker
```

`frame_index` is a composed-frame index. `delay` is a game-tick hold count. Locate the image chunk by scanning for `FF 00`; `seq_a`, `seq_b`, and `seq_c` are metadata, not reliable length fields.

### Frame composition

Stored frame 0 is the base image. Stored frames `1..N-1` are patches.

```text
compose(k):
    draw stored[0]
    if k != 0:
        d = descriptor[k - 1]
        clear canvas[d.x : d.x + d.w, d.y : d.y + d.h]
        blit stored[k] at (d.x, d.y)
```

Patch pen 0 is transparent. Sequence entries select `compose(frame_index)`; they do not select independent full-size bitmaps.
## Example structures and pseudocode

The structures below describe fixed fields only. Frame RLE streams and sequence data are variable-length and are not represented by C flexible-array members.

```c
#include <stdint.h>

#pragma pack(push, 1)
typedef struct {
    uint16_t frame_count;       /* big-endian on disk */
    uint16_t depth_or_mode;     /* big-endian on disk */
} ImageHeader;

typedef struct {
    uint16_t width;             /* big-endian on disk */
    uint16_t height;            /* big-endian on disk */
    uint16_t flags;             /* big-endian on disk */
} FrameInfo;

typedef struct {
    uint8_t x;
    uint8_t y;
    uint8_t width;
    uint8_t height;
} AnmPatch;

typedef struct {
    uint8_t reserved[2];        /* 00 00 */
    uint8_t magic[2];            /* 54 56, "TV" */
    AnmPatch patch[11];
    uint8_t seq_a;
    uint8_t seq_b;
    uint8_t seq_c;
} AnmHeader;                    /* 0x33 bytes */
#pragma pack(pop)
```

`uint16_t` values in these structures must be byte-swapped when read on a little-endian host. The palette is not part of `ImageHeader` because it follows the variable-sized frame table:

```c
uint32_t palette_offset = 4 + frame_count * sizeof(FrameInfo);
uint16_t palette[32];            /* u16be, 0x0RGB */
uint32_t frame_data_offset = palette_offset + 32 * 2;
```

### Nibble RLE decode

```text
function decode_rle(src, expected_bytes):
    output = empty byte buffer
    pending_high = none

    while length(output) < expected_bytes:
        p = read_byte(src)
        hi = p >> 4
        lo = p & 0x0F

        if hi == 0x0 or hi == 0xF:
            repeat = lo + 1
            repeat repeat times:
                emit_nibble(hi)
        else:
            emit_nibble(hi)
            emit_nibble(lo)

    return output

function emit_nibble(n):
    if pending_high is none:
        pending_high = n
    else:
        output.append((pending_high << 4) | n)
        pending_high = none
```

### Planar pixel lookup

```text
function pixel_index(frame, x, y, width, height):
    row_bytes = ((width + 15) >> 3) & 0xFFFE
    plane_size = height * row_bytes
    byte_offset = y * row_bytes + (x >> 3)
    mask = 0x80 >> (x & 7)
    index = 0

    for plane = 0 to 4:
        b = frame[plane * plane_size + byte_offset]
        if (b & mask) != 0:
            index |= 1 << plane

    return index
```

### Palette decode

```text
function decode_palette_word(word):
    word = read_u16be(word)
    red   = (word >> 8) & 0x0F
    green = (word >> 4) & 0x0F
    blue  = word & 0x0F

    return (red * 17, green * 17, blue * 17)
```

### `.32` frame decode

```text
read frame_count    = read_u16be(file + 0)
read depth_or_mode  = read_u16be(file + 2)
read frame_count FrameInfo records
read 32 palette words

for i = 0 to frame_count - 1:
    width  = be16(frame[i].width)
    height = be16(frame[i].height)
    row_bytes  = ((width + 15) >> 3) & 0xFFFE
    expected   = 5 * height * row_bytes
    planar[i]  = decode_rle(file, expected)
```

The RLE streams are sequential. The start of frame `i + 1` is the byte immediately following the decoded stream for frame `i`.

### `.anm` frame composition

```text
function compose_anm_frame(k):
    canvas = transparent canvas sized for stored frame 0
    blit(stored[0], canvas, 0, 0, pen_zero_is_transparent)

    if k == 0:
        return canvas

    d = patch[k - 1]
    clear(canvas, d.x, d.y, d.width, d.height)
    blit(stored[k], canvas, d.x, d.y, pen_zero_is_transparent)
    return canvas
```
