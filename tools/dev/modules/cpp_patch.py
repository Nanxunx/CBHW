from pathlib import Path
import shutil
import json
from datetime import datetime

from modules.diff_check import DiffChecker

from modules.diff_check import DiffChecker


class CppPatch:


    def __init__(self, file):

        self.file = Path(file)

        self.root = Path.cwd()

        self.before = None
        self.after = None


    def backup(self):

        backup = self.file.with_suffix(
            self.file.suffix + ".bak"
        )

        if not backup.exists():

            shutil.copy(
                self.file,
                backup
            )

        return backup



    def write_report(
        self,
        status,
        backup=None
    ):

        report_dir = Path(
            "tools/dev/reports/patch"
        )

        report_dir.mkdir(
            parents=True,
            exist_ok=True
        )


        data = {

            "file":
                str(self.file),

            "status":
                status,

            "backup":
                str(backup)
                if backup
                else "",

            "time":
                datetime.now().isoformat()

        }


        name = (
            self.file.stem
            +
            "_patch.json"
        )


        with open(
            report_dir / name,
            "w",
            encoding="utf-8"
        ) as f:

            json.dump(
                data,
                f,
                indent=4,
                ensure_ascii=False
            )



    def replace_block(
        self,
        old,
        new
    ):


        text = self.file.read_text(
            encoding="utf-8-sig"
        )

        self.before = text


        # 已经修改
        if new in text:

            print(
                "PATCH ALREADY APPLIED"
            )

            self.write_report(
                "ALREADY_APPLIED"
            )

            return



        # 找不到目标
        if old not in text:

            raise RuntimeError(
                "PATCH TARGET NOT FOUND\n"
                + old
            )


        print(
            "PATCH TARGET FOUND:"
        )

        print(
            old
        )


        print(
            "NEW:"
        )

        print(
            new
        )


        backup = self.backup()


        text = text.replace(
            old,
            new,
            1
        )


        self.file.write_text(
            text,
            encoding="utf-8-sig"
        )


        self.after = text


        diff = DiffChecker(
            self.root
        )


        diff.compare(
            backup,
            self.file
        )


        print(
            "PATCH SUCCESS"
        )


        print(
            "BACKUP:",
            backup
        )


        self.write_report(
            "SUCCESS",
            backup
        )
