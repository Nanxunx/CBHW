import sys
from pathlib import Path
import re

if len(sys.argv) < 2:
    print("usage: particle_analyzer.py candidates.txt")
    exit(1)

src = Path(sys.argv[1])

lines = src.read_text(
    encoding="utf-8",
    errors="ignore"
).splitlines()


items=[]

for line in lines:

    line=line.strip()

    if not line:
        continue

    m=re.search(r"(.+)\|particles=(\d+)", line)

    if m:
        path=m.group(1)
        count=int(m.group(2))

        items.append(
            (
                count,
                Path(path).name,
                path
            )
        )


items.sort(
    key=lambda x:x[0],
    reverse=True
)


outdir=src.parent


top50=outdir/"particle_top50.txt"
complex_file=outdir/"particle_complex.txt"
golden=outdir/"particle_golden_testlist.txt"


with top50.open("w",encoding="utf-8") as f:

    f.write("# Particle Top50\n\n")

    for count,name,path in items[:50]:

        f.write(
            f"{count}\t{name}\t{path}\n"
        )


with complex_file.open("w",encoding="utf-8") as f:

    f.write("# particles >=20\n\n")

    for count,name,path in items:

        if count>=20:

            f.write(
                f"{count}\t{name}\t{path}\n"
            )


selected=[]


# 高复杂
selected += [
    x for x in items
    if x[0]>=20
][:5]


# 中等
selected += [
    x for x in items
    if 5<=x[0]<20
][:10]


# 基础
selected += [
    x for x in items
    if 1<=x[0]<5
][:5]


with golden.open("w",encoding="utf-8") as f:

    f.write("# Particle Golden Test List V47\n\n")

    for count,name,path in selected:

        f.write(
            f"{count}\t{name}\t{path}\n"
        )


print("==============================")
print("Particle Analyze Complete")
print("Total models:",len(items))
print("Top50:",top50)
print("Complex:",complex_file)
print("Golden:",golden)
print("==============================")
