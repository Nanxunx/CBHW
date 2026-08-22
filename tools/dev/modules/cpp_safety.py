from pathlib import Path
import json
import re


class CppSafetyValidator:


    def __init__(self, root):

        self.root = Path(root)

        self.config = (
            self.root
            /
            "tools"
            /
            "dev"
            /
            "config"
            /
            "cpp_safety.json"
        )



    def load_rules(self):

        with open(
            self.config,
            "r",
            encoding="utf-8-sig"
        ) as f:

            return json.load(f)["rules"]



    def check_file(self, file):

        file = Path(file)

        text = file.read_text(
            encoding="utf-8-sig"
        )


        errors=[]


        for rule in self.load_rules():


            if not rule.get(
                "enabled",
                False
            ):

                continue



            name = rule["name"]



            if name=="brace_check":

                if text.count("{") != text.count("}"):

                    errors.append(
                        "brace count mismatch"
                    )


                continue



            if name=="parenthesis_check":

                if text.count("(") != text.count(")"):

                    errors.append(
                        "parenthesis count mismatch"
                    )


                continue



            pattern = rule.get(
                "pattern"
            )


            if pattern:

                if re.search(
                    pattern,
                    text,
                    re.MULTILINE
                ):

                    errors.append(
                        "bad pattern: "
                        + name
                    )



        if errors:

            return False, errors


        return True, []




    def validate_changed_files(self, files):


        result=True

        report={}



        for file in files:


            ok,errors = self.check_file(
                file
            )


            report[str(file)] = {

                "status":
                    "PASS"
                    if ok
                    else "FAIL",

                "errors":
                    errors

            }


            if not ok:

                result=False



        return result,report
