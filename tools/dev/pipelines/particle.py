
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]

sys.path.insert(
    0,
    str(ROOT / "tools" / "dev" / "reports")
)

from report_writer import ReportWriter


import json
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]


CONFIG_DIR = ROOT / "tools" / "dev" / "config"

MODULE_DIR = ROOT / "tools" / "dev" / "modules"


def load_json(path):

    with open(
        path,
        "r",
        encoding="utf-8-sig"
    ) as f:

        return json.load(f)



def run():

    report = ReportWriter(
        "particle"
    )


    module = load_json(
        MODULE_DIR / "particle.json"
    )


    project = load_json(
        CONFIG_DIR / "project.json"
    )


    paths = load_json(
        CONFIG_DIR / "paths.json"
    )


    build = project["build_dir"]


    converter = (
        ROOT
        /
        build
        /
        "Release"
        /
        "turtle335_convert_m2.exe"
    )


    validator = (
        ROOT
        /
        build
        /
        "Debug"
        /
        "turtle335_validate_m2.exe"
    )


    golden = (
        ROOT
        /
        module["golden"]
    )


    input_dir = (
        golden
        /
        "creature_input"
    )


    output_dir = (
        golden
        /
        "v47_convert_output"
    )


    output_dir.mkdir(
        exist_ok=True
    )


    dbc = Path(
        paths["wotlk_data"]
    ) / "DBFilesClient" / "AnimationData.dbc"



    for source in input_dir.glob("*.M2"):

        name = source.stem


        output = (
            output_dir
            /
            f"{name}.m2"
        )


        print("")
        print(
            "==========",
            name,
            "=========="
        )


        subprocess.run(
            [
                str(converter),
                str(source),
                str(dbc),
                str(output)
            ],
            check=True
        )


        subprocess.run(
            [
                str(validator),
                str(output)
            ],
            check=True
        )


        report.add(
            name,
            output.stat().st_size,
            True
        )








    report.save(
        Path(
            "tools/dev/reports/particle/particle_report.json"
        )
    )


if __name__ == "__main__":

    run()