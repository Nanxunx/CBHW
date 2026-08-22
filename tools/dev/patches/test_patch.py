import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


sys.path.insert(
    0,
    str(ROOT)
)


from tools.dev.modules.cpp_patch import CppPatch



patch = CppPatch(
    ROOT / "src/model/ModelScanner.cpp"
)


patch.replace_block(

"""package.m2Files.push_back(
                path
            );""",

"""package.m2Files.push_back(
                file.path().string()
            );"""

)

