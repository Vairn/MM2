"""Targeted column analysis of WinUAE Sir Hyron character sheet."""
from PIL import Image

IMG = r"C:\Users\Adam Templeton\.cursor\projects\c-20260421-D-REC-development-MM2\assets\c__Users_Adam_Templeton_AppData_Roaming_Cursor_User_workspaceStorage_9dbfa96a0e27f22594776b2b796fd270_images_image-dfb2d251-660d-463e-8785-9865a58c9548.png"
img = Image.open(IMG)
px = img.load()
w, h = img.size

# From previous analysis:
# Red border left at x=151.
# Row spacing ≈17px per Amiga row (17/8 = 2.125 px per Amiga pixel... but let's verify)
# Actually: display width for 40 Amiga cols = 40*8 = 320 Amiga px.
# Border inner right edge should be around col 39 = x=151+39*cell_disp
# Let's find right border red pixel
right_x = None
for x in range(w-1, 0, -1):
    for y in range(h):
        r,g,b = px[x,y][:3]
        if r > 180 and g < 60 and b < 60:
            right_x = x
            break
    if right_x is not None:
        break

left_x = 151
print(f"Border: left={left_x}, right={right_x}")
display_w = right_x - left_x
cell_disp = display_w / 40.0  # display pixels per Amiga column (8 Amiga px per col)
amiga_px_scale = cell_disp / 8.0
print(f"Display width={display_w}px for 40 cols -> {cell_disp:.2f}px/col, {amiga_px_scale:.3f}px/Amiga-px")

def amiga_col_to_img_x(col):
    return left_x + col * cell_disp

def img_x_to_amiga_col(img_x):
    return (img_x - left_x) / cell_disp

# Content row y positions (from previous analysis, text band starts):
# row0=110, row1=128, row2=145, row3=162, row4=180, row5=197, row6=214, row7=232
row_y = {0: 113, 1: 131, 2: 148, 3: 165, 4: 183, 5: 200, 6: 217, 7: 236}

# For each content row, find the x positions of bright pixel clusters
def find_text_starts_in_row(y_scan, threshold=150):
    """Return list of (img_x, amiga_col) where text clusters start."""
    in_text = False
    starts = []
    for x in range(left_x, right_x):
        r,g,b = px[x, y_scan][:3]
        bright = r > threshold and g > threshold and b > threshold
        if bright and not in_text:
            in_text = True
            starts.append(x)
        elif not bright and in_text:
            in_text = False
    return [(x, img_x_to_amiga_col(x)) for x in starts]

print("\nColumn positions per content row:")
for row_idx, y in sorted(row_y.items()):
    starts = find_text_starts_in_row(y)
    # Merge starts that are within 1 Amiga col of each other (likely same character)
    merged = []
    for img_x, col in starts:
        if not merged or img_x - merged[-1][0] > cell_disp * 0.8:
            merged.append((img_x, col))
    # Show as Amiga cols (rounded)
    cols = [f"{col:.1f}" for _, col in merged]
    print(f"  Row {row_idx} (y={y}): cols {cols}")
