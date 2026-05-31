from amitools.binfmt.hunk.HunkReader import HunkReader
import sys

hr = HunkReader()
hr.read_file(sys.argv[1])
for i, h in enumerate(hr.hunks):
    t = h.get("type_name", "?")
    extra = ""
    if "data" in h:
        extra += f" data={len(h['data'])}"
    if "size" in h:
        extra += f" size={h['size']}"
    print(f"{i:2} @{h.get('hunk_file_offset', 0):06x} {t}{extra}")
