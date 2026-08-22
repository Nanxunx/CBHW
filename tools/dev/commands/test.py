from modules.test_check import TestCheck


def run(root):

    checker = TestCheck(
        root
    )

    return checker.run()

