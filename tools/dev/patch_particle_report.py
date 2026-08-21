from pathlib import Path
import shutil


target = Path(
    "tools/dev/pipelines/particle.py"
)


backup = Path(
    "tools/dev/pipelines/particle.py.bak"
)


if not backup.exists():
    shutil.copy(
        target,
        backup
    )


text = target.read_text(
    encoding="utf-8-sig"
)


# 1. 添加 import

if "ReportWriter" not in text:

    text = (
        "from tools.dev.reports.report_writer import ReportWriter\n\n"
        + text
    )


# 2. 创建 report

if "report = ReportWriter" not in text:

    marker = "def run():"

    text=text.replace(
        marker,
        marker+
        "\n\n    report = ReportWriter('particle')"
    )


# 3. 保存结果

if "particle_report.json" not in text:

    marker = 'if __name__ == "__main__":'

    insert = '''

    report.save(
        Path(
            "tools/dev/reports/particle/particle_report.json"
        )
    )

'''

    text=text.replace(
        marker,
        insert+marker
    )


target.write_text(
    text,
    encoding="utf-8-sig"
)


print("PATCH DONE")
