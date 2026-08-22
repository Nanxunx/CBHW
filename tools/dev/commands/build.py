from pathlib import Path

from modules.build_check import BuildCheck


def run(root):

    checker = BuildCheck(
        root
    )

    return checker.run()

