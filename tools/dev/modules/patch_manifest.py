import json
from pathlib import Path


class PatchManifest:


    def __init__(self, root):

        self.root=Path(root)



    def load(self,name):

        file=(

            self.root
            /
            "tools"
            /
            "dev"
            /
            "patches"
            /
            "config"
            /
            (name+".json")

        )


        if not file.exists():

            raise RuntimeError(
                "Patch manifest missing:"
                +str(file)
            )


        with open(
            file,
            "r",
            encoding="utf-8-sig"
        ) as f:

            return json.load(f)
