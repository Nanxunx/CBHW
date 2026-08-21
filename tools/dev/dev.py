import json
import subprocess
import sys
import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]

CONFIG = ROOT / "tools" / "dev" / "config"


def load(name):

    with open(
        CONFIG / name,
        "r",
        encoding="utf-8-sig"
    ) as f:

        return json.load(f)



def run(cmd):

    print("\n>>>", " ".join(cmd))

    subprocess.run(
        cmd,
        cwd=ROOT,
        check=True
    )



def build():

    project = load(
        "project.json"
    )

    build_cfg = load(
        "build.json"
    )


    run(
        [
            "cmake",
            "-S",
            ".",
            "-B",
            project["build_dir"]
        ]
    )


    run(
        [
            "cmake",
            "--build",
            project["build_dir"],
            "--config",
            build_cfg["configuration"]
        ]
    )



def test():

    project = load(
        "project.json"
    )

    build_cfg = load(
        "build.json"
    )


    run(
        [
            "ctest",
            "--test-dir",
            project["build_dir"],
            "-C",
            build_cfg["configuration"]
        ]
    )



def clean():

    project = load(
        "project.json"
    )

    build_dir = ROOT / project["build_dir"]

    if build_dir.exists():

        shutil.rmtree(
            build_dir
        )

        print(
            "Removed:",
            build_dir
        )

    else:

        print(
            "Nothing to clean"
        )



def modules():

    module_dir = ROOT / "tools" / "dev" / "modules"

    print(
        "Turtle335Converter Modules"
    )
    print(
        "-------------------------"
    )


    for f in module_dir.glob("*.json"):

        data = load_module(f)

        print(
            ""
        )

        print(
            "[" + data["name"] + "]"
        )

        print(
            "status:",
            data.get("status")
        )


def load_module(path):

    with open(
        path,
        "r",
        encoding="utf-8-sig"
    ) as f:

        return json.load(f)

    print(
        "Turtle335Converter Dev Environment"
    )

    print(
        "Root:",
        ROOT
    )


    for f in CONFIG.glob(
        "*.json"
    ):

        print(
            "Config:",
            f.name
        )




def info():

    print(
        "Turtle335Converter Dev Environment"
    )

    print(
        "Root:",
        ROOT
    )


    for f in CONFIG.glob(
        "*.json"
    ):

        print(
            "Config:",
            f.name
        )



def main():

    if len(sys.argv)<2:

        print(
            """
Usage:

python tools/dev/dev.py build
python tools/dev/dev.py test
python tools/dev/dev.py clean
python tools/dev/dev.py info
"""
        )

        return


    cmd=sys.argv[1]


    if cmd=="build":

        build()


    elif cmd=="test":

        test()


    elif cmd=="clean":

        clean()


    elif cmd=="info":

        info()

    elif cmd=="modules":

        modules()


    else:

        print(
            "Unknown command:",
            cmd
        )


if __name__=="__main__":

    main()


