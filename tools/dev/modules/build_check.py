import subprocess
from pathlib import Path

from modules.report_writer import ReportWriter


class BuildCheck:


    def __init__(self, root):

        self.root = Path(root)



    def run(self):

        report = ReportWriter(
            "build"
        )


        cmd = [
            "cmake",
            "--build",
            "build-v46",
            "--config",
            "Debug"
        ]


        result = subprocess.run(
            cmd,
            cwd=self.root,
            text=True,
            encoding="utf-8",
            errors="replace",
            capture_output=True
        )


        report.add(
            "command",
            " ".join(cmd)
        )


        report.add(
            "returncode",
            result.returncode
        )


        report.add(
            "stdout",
            result.stdout
        )


        report.add(
            "stderr",
            result.stderr
        )



        if result.returncode == 0:

            print(
                "BUILD PASS"
            )

            report.set_status(
                "PASS"
            )

            report.save(
                "build_report.json"
            )

            return True



        print(
            "BUILD FAILED"
        )


        report.set_status(
            "FAIL"
        )


        report.save(
            "build_report.json"
        )


        return False
