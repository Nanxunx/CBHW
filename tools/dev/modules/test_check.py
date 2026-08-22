import subprocess

from modules.report_writer import ReportWriter
from pathlib import Path


class TestCheck:


    def __init__(self, root):

        self.root = Path(root)



    def run(self):

        cmd = [
            "ctest",
            "--test-dir",
            "build-v46",
            "-C",
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


        report = ReportWriter(
            "test",
            self.root
        )


        report.add(
            "command",
            "ctest --test-dir build-v46 -C Debug"
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

            report.set_status(
                "PASS"
            )

            report.save(
                "test_report.json"
            )


            print(
                "TEST PASS"
            )

            return True



        report.set_status(
            "FAIL"
        )


        report.error(
            "ctest failed"
        )


        report.save(
            "test_report.json"
        )


        print(
            "TEST FAILED"
        )


        return False
