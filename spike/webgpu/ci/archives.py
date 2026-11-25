# Lists vcpkg binary archives by port, largest first:  python archives.py <folder>
import pathlib
import sys
import zipfile

rows = []
for path in pathlib.Path(sys.argv[1]).rglob("*.zip"):
    name = "?"
    with zipfile.ZipFile(path) as z:
        if "CONTROL" in z.namelist():
            fields = {}
            for line in z.read("CONTROL").decode("utf-8", "replace").splitlines():
                if not line.strip():
                    break
                key, _, value = line.partition(": ")
                fields[key] = value
            name = fields.get("Package", "?") + ":" + fields.get("Architecture", "?")
    rows.append((path.stat().st_size, name))
rows.sort(reverse=True)
print(f"FACT: archives {len(rows)}, total {sum(r[0] for r in rows) / 1e6:.1f} MB")
for size, name in rows:
    if size > 50_000:
        print(f"FACT: archive {size / 1e6:7.2f} MB  {name}")
