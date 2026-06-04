"""Find exact column positions for Cost/Age and Cond= in WinUAE char sheet."""
from PIL import Image

IMG = r"C:\Users\Adam Templeton\.cursor\projects\c-20260421-D-REC-development-MM2\assets\c__Users_Adam_Templeton_AppData_Roaming_Cursor_User_workspaceStorage_9dbfa96a0e27f22594776b2b796fd270_images_image-dfb2d251-660d-463e-8785-9865a58c9548.png"
img = Image.open(IMG)
px = img.load()
w, h = img.size

left_x = 151
right_x = 873
cell_disp = (right_x - left_x) / 40.0  # 18.05 px/col

def img_x_to_col(x):
    return (x - left_x) / cell_disp

def find_clusters(y_scan, threshold=130, min_gap=4):
    """Return list of (start_x, end_x, start_col, end_col) for each bright cluster."""
    clusters = []
    in_c = False
    sx = 0
    for x in range(left_x, right_x):
        r,g,b = px[x, y_scan][:3]
        bright = r > threshold and g > threshold and b > threshold
        if bright and not in_c:
            in_c = True
            sx = x
        elif not bright and in_c:
            # Only record if meaningful width
            if x - sx >= 2:
                clusters.append((sx, x-1, img_x_to_col(sx), img_x_to_col(x-1)))
            in_c = False
    return clusters

def merge_clusters(clusters, gap_cols=1.5):
    """Merge clusters within gap_cols of each other."""
    if not clusters:
        return []
    merged = [list(clusters[0])]
    for c in clusters[1:]:
        if c[2] - merged[-1][3] < gap_cols:
            merged[-1][1] = c[1]
            merged[-1][3] = c[3]
        else:
            merged.append(list(c))
    return merged

# Scan multiple y values per row to get best coverage
# Row layout: header y~76, then r0 at y~110, spacing ~17px
# Content rows we care about:
# Row 0 (Lv1/HP/Age): y~113
# Row 3 (Per/AC/SL/Cost/Day): y~162-176  
# Row 4 (End/Thievery/Gems): y~180-193
# Row 5 (Spd/Skill/Food): y~197-211
# Row 6 (Acy/dots/Cond): y~214-228

print("=== Row 0 scan (Lv1=7 HP=60 /60 Age=18) ===")
for y in range(111, 124):
    clust = merge_clusters(find_clusters(y))
    word_starts = [f"col{c[2]:.1f}" for c in clust]
    if len(word_starts) >= 3:
        print(f"  y={y}: {word_starts}")

print("\n=== Row 3 scan (Per=5 AC=26 SL=0 Cost=19 /Day) ===")
for y in range(162, 176):
    clust = merge_clusters(find_clusters(y))
    word_starts = [f"col{c[2]:.1f}-{c[3]:.1f}" for c in clust]
    if len(word_starts) >= 4:
        print(f"  y={y}: {word_starts}")

print("\n=== Row 4 scan (End=15 Thievery 0% Gems=17) ===")
for y in range(180, 193):
    clust = merge_clusters(find_clusters(y))
    word_starts = [f"col{c[2]:.1f}" for c in clust]
    if len(word_starts) >= 3:
        print(f"  y={y}: {word_starts}")

print("\n=== Row 5 scan (Spd=20 Athlete Food=30) ===")
for y in range(197, 211):
    clust = merge_clusters(find_clusters(y))
    word_starts = [f"col{c[2]:.1f}" for c in clust]
    if len(word_starts) >= 2:
        print(f"  y={y}: {word_starts}")

print("\n=== Row 6 scan (Acy=22 ........... Cond= Good) ===")
for y in range(214, 230):
    clust = merge_clusters(find_clusters(y, threshold=100))
    word_starts = [f"col{c[2]:.1f}-{c[3]:.1f}" for c in clust]
    if len(word_starts) >= 2:
        print(f"  y={y}: {word_starts}")
