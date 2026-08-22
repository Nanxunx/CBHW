import subprocess
import json
from pathlib import Path
from datetime import datetime



class GitBaseline:



    def __init__(
        self,
        root
    ):

        self.root=Path(root)



    def report_dir(self):

        directory=(
            self.root
            /
            "tools"
            /
            "dev"
            /
            "reports"
            /
            "baseline"
        )

        directory.mkdir(
            parents=True,
            exist_ok=True
        )

        return directory




    def snapshot(self):

        result=subprocess.run(

            [
                "git",
                "status",
                "--short"
            ],

            cwd=self.root,

            text=True,

            encoding="utf-8",

            errors="replace",

            capture_output=True

        )


        files=[]


        for line in result.stdout.splitlines():

            if not line.strip():

                continue


            files.append(
                line[3:].strip()
            )


        return files





    def save(
        self,
        name
    ):


        data={

            "time":
                datetime.now().isoformat(),


            "files":
                self.snapshot()

        }



        file=(
            self.report_dir()
            /
            (name+".json")
        )


        with open(

            file,

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
            "BASELINE SAVED:",
            file
        )



        return file





    def load(
        self,
        name
    ):


        file=(
            self.report_dir()
            /
            (name+".json")
        )


        with open(
            file,
            "r",
            encoding="utf-8"
        ) as f:

            return json.load(f)



    def compare(
        self,
        name,
        current
    ):


        base=self.load(
            name
        )


        old=set(
            base["files"]
        )


        now=set(
            current
        )


        return list(
            now-old
        )

