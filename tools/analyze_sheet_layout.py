"""Pixel-analyze the WinUAE character sheet screenshot to derive Amiga grid column positions."""
import sys
from PIL import Image

IMG = r"C:\Users\Adam Templeton\.cursor\projects\c-20260421-D-REC-development-MM2\assets\c__Users_Adam_Templeton_AppData_Roaming_Cursor_User_workspaceStorage_9dbfa96a0e27f22594776b2b796fd270_images_image-dfb2d251-660d-463e-8785-9865a58c9548.png"

img = Image.open(IMG)
px = img.load()
w, h = img.size
print(f"Image: {w}x{h}")

# Find red border left edge (top-left corner of the red frame)
left_x = None
for x in range(w):
    for y in range(h):
        r, g, b = px[x, y][:3]
        if r > 180 and g < 60 and b < 60:
            left_x = x
            break
    if left_x is not None:
        break

top_y = None
for y in range(h):
    for x in range(w):
        r, g, b = px[x, y][:3]
        if r > 180 and g < 60 and b < 60:
            top_y = y
            break
    if top_y is not None:
        break

print(f"Red border top-left: ({left_x}, {top_y})")

# Find cell width by scanning header row for character pixel runs
# The header "A) Sir Hyron..." is in the top border row.
# Look for bright pixel clusters to estimate cell size.
# Scan y = top_y + 4 (middle of first border row)
def bright_runs(y_scan, threshold=160):
    runs = []
    in_run = False
    start = 0
    for x in range(left_x, w):
        r, g, b = px[x, y_scan][:3]
        bright = (r > threshold and g > threshold and b > threshold)
        if bright and not in_run:
            in_run = True
            start = x
        elif not bright and in_run:
            in_run = False
            runs.append((start, x - 1))
    return runs

# Scan first header row
header_y = top_y + 4
runs = bright_runs(header_y)
print(f"\nHeader row y={header_y} bright runs (first 10): {runs[:10]}")

# Estimate cell_w: gaps between run starts should be multiples of cell_w
# Try to find the cell_w from inter-character spacing
starts = [r[0] for r in runs]
if len(starts) > 5:
    diffs = [starts[i+1] - starts[i] for i in range(len(starts)-1)]
    # Most common small diff is likely cell_w or sub-cell
    from collections import Counter
    cnt = Counter(diffs)
    print(f"Gap distribution (top 10): {cnt.most_common(10)}")

# Now scan the content rows to find where text appears
# Content rows start just below the header row
# Estimate: header is 1 cell tall, then content starts
# Let's scan rows from top_y to find text density per row
print("\nRow-by-row bright pixel count (to find text rows):")
row_data = []
for y_off in range(0, 500, 1):
    y_test = top_y + y_off
    if y_test >= h:
        break
    count = sum(1 for x in range(left_x, w)
                if px[x, y_test][0] > 160 and px[x, y_test][1] > 160 and px[x, y_test][2] > 160)
    row_data.append((y_test, count))

# Print rows with significant brightness
sig_rows = [(y, c) for y, c in row_data if c > 15]
for y, c in sig_rows:
    print(f"  y={y}: {c} bright px")

# Find cell height from gaps between text rows
if len(sig_rows) > 3:
    # Group consecutive bright rows into bands
    bands = []
    band_start = sig_rows[0][0]
    prev_y = sig_rows[0][0]
    for y, c in sig_rows[1:]:
        if y - prev_y > 3:
            bands.append((band_start, prev_y))
            band_start = y
        prev_y = y
    bands.append((band_start, prev_y))
    print(f"\nText bands (top to bottom): {bands[:20]}")
    if len(bands) > 2:
        gaps = [bands[i+1][0] - bands[i][0] for i in range(len(bands)-1)]
        from collections import Counter
        print(f"Band spacing (cell height estimate): {Counter(gaps).most_common(5)}")
