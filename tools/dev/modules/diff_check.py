from pathlib import Path
import difflib
import json
from datetime import datetime



class DiffChecker:


    def __init__(self, root):

        self.root = Path(root)



    def compare(
        self,
        before,
        after
    ):

        before = Path(before)
        after = Path(after)


        old = before.read_text(
            encoding="utf-8-sig"
        ).splitlines()


        new = after.read_text(
            encoding="utf-8-sig"
        ).splitlines()



        diff=list(
            difflib.unified_diff(
                old,
                new,
                fromfile=str(before),
                tofile=str(after)
            )
        )



        self.save_report(
            diff
        )


        return diff



    def save_report(
        self,
        diff
    ):


        directory=(
            self.root
            /
            "tools"
            /
            "dev"
            /
            "reports"
            /
            "diff"
        )


        directory.mkdir(
            parents=True,
            exist_ok=True
        )


        data={

            "time":
                datetime.now().isoformat(),

            "changed":
                len(diff)>0,

            "lines":
                diff

        }



        with open(
            directory /
            "diff_report.json",

            "w",

            encoding="utf-8"

        ) as f:


            json.dump(
                data,
                f,
                indent=4,
                ensure_ascii=False
            )


        print(
            "DIFF REPORT:",
            directory /
            "diff_report.json"
        )
