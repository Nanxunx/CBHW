import json
from pathlib import Path
from datetime import datetime


class ReportWriter:


    def __init__(
        self,
        category,
        root=None
    ):

        self.category = category

        self.root = (
            Path(root)
            if root
            else Path.cwd()
        )


        self.data = {

            "category":
                category,

            "time":
                "",

            "status":
                "",

            "details":
                {},

            "errors":
                []

        }



    def set_status(
        self,
        status
    ):

        self.data["status"] = status



    def add(
        self,
        key,
        value
    ):

        self.data["details"][key] = value



    def error(
        self,
        message
    ):

        self.data["errors"].append(
            message
        )



    def save(
        self,
        name
    ):


        self.data["time"] = (
            datetime.now()
            .isoformat()
        )


        directory = (
            self.root
            /
            "tools"
            /
            "dev"
            /
            "reports"
            /
            self.category
        )


        directory.mkdir(
            parents=True,
            exist_ok=True
        )


        file = (
            directory
            /
            name
        )


        with open(
            file,
            "w",
            encoding="utf-8"
        ) as f:


            json.dump(
                self.data,
                f,
                indent=4,
                ensure_ascii=False
            )


        print(
            "REPORT:",
            file
        )
