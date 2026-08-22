
import subprocess
from pathlib import Path
from modules.git_baseline import GitBaseline



class GitGuard:


    def __init__(
        self,
        root,
        baseline="dev_framework"
    ):

        self.root=Path(root)

        self.baseline=baseline

        self.base=GitBaseline(
            root
        )



    def changed_files(self):

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



    def allowed(self,file,allowed_paths):


        file=Path(file)


        for item in allowed_paths:


            item=Path(item)


            if str(file).startswith(
                str(item)
            ):

                return True


        return False



    def validate(
        self,
        allowed_paths,
        allow_tool_changes=False
    ):


        changed=self.base.compare(
            self.baseline,
            self.changed_files()
        )


        print(
            "Changed files:"
        )


        errors=[]


        for f in changed:


            print(
                " -",
                f
            )


            if f.startswith(
                "tools/dev"
            ):

                if not allow_tool_changes:

                    errors.append(
                        f
                    )

                    continue



            if not self.allowed(
                f,
                allowed_paths
            ):

                errors.append(
                    f
                )



        if errors:


            print(
                "GIT GUARD FAILED"
            )


            print(
                "Unexpected files:"
            )


            for e in errors:

                print(
                    e
                )


            return False



        print(
            "GIT GUARD PASS"
        )


        return True
