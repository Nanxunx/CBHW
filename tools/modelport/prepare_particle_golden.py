from pathlib import Path
import shutil

srcfile=Path("golden/particle/particle_golden_testlist.txt")

out=Path("golden/particle/golden_input")

out.mkdir(parents=True,exist_ok=True)


count=0

for line in srcfile.read_text(
    encoding="utf-8",
    errors="ignore"
).splitlines():

    if not line or line.startswith("#"):
        continue

    parts=line.split("\t")

    if len(parts)<3:
        continue

    path=Path(parts[2].strip())

    if path.exists():

        shutil.copy2(
            path,
            out/path.name
        )

        print("COPY",path.name)

        count+=1


print()
print("Copied:",count)
