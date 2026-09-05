#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Conjure Bilingual GUI launcher (zh_CN / en_US)

This file does NOT modify Conjure's binary/DBC/M2/SQL core.
It loads an untouched upstream checkout and translates only GUI-facing text.
"""

import argparse
import importlib.util
import json
import os
import subprocess
import sys
import tkinter as tk
from tkinter import filedialog, messagebox, scrolledtext, ttk

UPSTREAM_COMMIT = "2d09dd0f0567b8469716a3f2113f9dbcd43a47a1"
UPSTREAM_URL = f"https://github.com/TheDaveCalaz/Conjure/archive/{UPSTREAM_COMMIT}.zip"
DEFAULT_LANG = "zh_CN"
SUPPORTED = ("zh_CN", "en_US")

def resource_base():
    if getattr(sys, "frozen", False):
        return getattr(sys, "_MEIPASS", os.path.dirname(sys.executable))
    return os.path.dirname(os.path.abspath(__file__))

def writable_base():
    if getattr(sys, "frozen", False):
        return os.path.dirname(sys.executable)
    return os.path.dirname(os.path.abspath(__file__))

CONFIG_PATH = os.path.join(writable_base(), "conjure_i18n_config.json")

def load_config():
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
            return data if isinstance(data, dict) else {}
    except Exception:
        return {}

def save_lang(lang):
    try:
        data = load_config()
        data["language"] = lang
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
    except OSError:
        pass

parser = argparse.ArgumentParser(add_help=False)
parser.add_argument("--lang", choices=SUPPORTED)
args, _unknown = parser.parse_known_args()
LANG = args.lang or load_config().get("language", DEFAULT_LANG)
if LANG not in SUPPORTED:
    LANG = DEFAULT_LANG
save_lang(LANG)

EXACT = {
    "Conjure — WoW 3.3.5a Model Porting": "Conjure — WoW 3.3.5a 模型移植工具",
    "Port a Model (Guided)": "模型移植（向导）",
    "Inspect M2 (manual)": "检查 M2（手动）",
    "Bake Textures (manual)": "烘焙纹理（手动）",
    "Build DBC Rows (manual)": "生成 DBC 记录（手动）",
    "Set TextureVariations (manual / community ports)": "设置 TextureVariations（手动 / 社区模型）",
    "0. Model + folder": "0. 模型与文件夹",
    "1. Readiness check": "1. 就绪检查",
    "2. Textures": "2. 纹理",
    "3. DBC rows": "3. DBC 记录",
    "4. Repoint SQL": "4. 重定向 SQL",
    "5. Ready to pack": "5. 准备打包",
    "Browse…": "浏览…",
    "Next →": "下一步 →",
    "← Back": "← 返回",
    "Build": "生成",
    "Apply": "应用",
    "Re-check model": "重新检查模型",
    "Browse to fixed .m2…": "浏览修复后的 .m2…",
    "Select file": "选择文件",
    "Select the converted .m2": "选择已转换的 .m2",
    "M2 model": "M2 模型",
    "All files": "所有文件",
    "Choose a valid .m2 file.": "请选择有效的 .m2 文件。",
    "Enter a model folder name.": "请输入模型文件夹名称。",
    "Choose a CreatureDisplayInfo.dbc file first.": "请先选择 CreatureDisplayInfo.dbc 文件。",
    "DisplayID and CreatureGeosetData must be integers.": "DisplayID 和 CreatureGeosetData 必须是整数。",
    "DBC rows built successfully.": "DBC 记录生成成功。",
    "TextureVariations applied successfully.": "TextureVariations 应用成功。",
    "Enter at least one texture path to bake.": "请至少输入一个要烘焙的纹理路径。",
    "ID floors must be integers.": "ID 起始值必须是整数。",
    "Fill in both DBC paths and the model name.": "请填写两个 DBC 路径以及模型名称。",
    "Stage 0 — Choose the model + folder name": "步骤 0 — 选择模型与文件夹名称",
    "Stage 1 — Readiness check": "步骤 1 — 模型就绪检查",
    "Stage 2 — Textures": "步骤 2 — 纹理",
    "Stage 3 — DBC rows": "步骤 3 — DBC 记录",
    "Stage 4 — Repoint SQL": "步骤 4 — 重定向 SQL",
    "Stage 5 — Ready to pack": "步骤 5 — 准备打包",
    ".m2 file:": ".m2 文件：",
    "Model folder name:": "模型文件夹名称：",
    "DisplayID floor:": "DisplayID 起始值：",
    "ModelData floor:": "ModelData 起始值：",
    "Creature entry (for the SQL reminder, optional):": "Creature entry（用于 SQL 提示，可选）：",
    "Target DisplayID (must exist):": "目标 DisplayID（必须已存在）：",
    "CreatureGeosetData:": "CreatureGeosetData：",
    "TextureVariation1:": "TextureVariation1：",
    "TextureVariation2:": "TextureVariation2：",
    "TextureVariation3:": "TextureVariation3：",
    "Format": "格式",
    "Version": "版本",
    "Bones": "骨骼",
    "Animations": "动画",
    "Multi-LOD check": "多 LOD 检查",
    "Textures": "纹理",
    "Readiness check": "就绪检查",
    "Ready to pack": "准备打包",
    "Verification": "验证",
    "Output": "输出",
    "Backup": "备份",
}

PHRASES = [
    ("Port a Model — go stage by stage; Conjure checks each one before letting you continue, and only calls a model \"ready to pack\" once every check passes.", "模型移植 — 按步骤操作；Conjure 会在进入下一步前完成检查，只有全部检查通过后才会标记为“准备打包”。"),
    ("Browse to the CONVERTED .m2 you want to port, and give it the in-game model folder name.", "选择要移植的已转换 .m2 文件，并填写游戏内使用的模型文件夹名称。"),
    ("Conjure verifies this model is actually safe to pack before doing anything else.", "Conjure 会先验证该模型是否适合安全打包，然后才进行后续操作。"),
    ("How to fix this:", "如何修复："),
    ("Next action:", "下一步操作："),
    ("I understand the risk — let me continue anyway (advanced)", "我已了解风险 — 仍然继续（高级）"),
    ("Already baked to", "已烘焙到"),
    ("the values below are what was actually verified on disk.", "下方数值为磁盘文件重新读取后的实际验证结果。"),
    ("Edit and hit Next again to re-bake.", "如需修改，请编辑后再次点击“下一步”重新烘焙。"),
    ("No bakeable or DBC-fed texture slots were found on this model — nothing to do here.", "此模型未发现可烘焙或由 DBC 提供的纹理槽，本步骤无需处理。"),
    ("This model bakes texture paths INTO the .m2", "此模型会将纹理路径直接烘焙到 .m2 内"),
    ("Each path below must be the IN-GAME path", "下方每个路径都必须是游戏内路径"),
    ("It is NOT the file's current location on your PC", "不能填写该文件当前在电脑上的本地路径"),
    ("Manual/advanced.", "手动/高级。"),
    ("community ports", "社区模型"),
    ("Choose the model + folder name", "选择模型与文件夹名称"),
    ("Readiness check", "就绪检查"),
    ("Ready to pack", "准备打包"),
    ("Build DBC rows", "生成 DBC 记录"),
    ("Repoint SQL", "重定向 SQL"),
    ("Bake Textures", "烘焙纹理"),
    ("Inspect M2", "检查 M2"),
    ("Set TextureVariations", "设置 TextureVariations"),
    ("Model folder+name", "模型文件夹+名称"),
    ("Creature entry", "Creature entry"),
    ("Wrote:", "已写入："),
    ("Backup of original:", "原文件备份："),
    ("Verification (re-parsed from the written file):", "验证（从写入后的文件重新解析）："),
    ("SQL to repoint a creature to this display:", "将生物重定向到此 DisplayID 的 SQL："),
    ("Folder:", "文件夹："),
    ("Skin files found:", "找到的 Skin 文件："),
    (".anim files found:", "找到的 .anim 文件："),
    (".blp files found:", "找到的 .blp 文件："),
    ("(none)", "（无）"),
    ("passed", "通过"),
    ("failed", "失败"),
    ("warning", "警告"),
    ("Warning", "警告"),
    ("Error", "错误"),
    ("unexpected error", "意外错误"),
    ("refused", "已拒绝"),
    ("Select", "选择"),
    ("Browse", "浏览"),
    ("Back", "返回"),
    ("Next", "下一步"),
    ("Apply", "应用"),
    ("Build", "生成"),
    ("This model hasn't been converted yet", "此模型尚未转换"),
    ("This model isn't ready — its LODs aren't merged", "此模型尚未就绪 — LOD 尚未合并"),
    ("Conjure couldn't read this file", "Conjure 无法读取此文件"),
    ("This is a modern chunked model (MD21). Conjure can only read WotLK MD20 files, and it can't do the conversion itself — that's a separate tool's job.", "这是现代分块模型（MD21）。Conjure 只能读取 WotLK 的 MD20 文件，并且自身不负责模型转换 — 需要使用外部转换工具。"),
    ("Check the model's source folder for a .skel file.", "检查模型源文件夹中是否存在 .skel 文件。"),
    ("If there IS a .skel file: convert it with MultiConverter Shadowlands-Wotlk.exe (listfile.csv must sit next to the exe, or it won't open).", "如果存在 .skel：请使用 MultiConverter Shadowlands-Wotlk.exe 转换（listfile.csv 必须与 exe 放在同一目录，否则程序无法打开）。"),
    ("If there is NO .skel file: convert it with MultiConverter Legion-Wotlk.exe instead.", "如果没有 .skel：请改用 MultiConverter Legion-Wotlk.exe 转换。"),
    ("Come back here and load the NEW converted .m2 (it should now start with MD20).", "转换完成后回到这里，载入新的 .m2（文件头应为 MD20）。"),
    ("Browse to the converted .m2 and click \"Re-check model\".", "浏览到已转换的 .m2，然后点击“重新检查模型”。"),
    ("Open the RAW wow.export .m2 (before any conversion) in MultiConverter.", "在 MultiConverter 中打开 wow.export 导出的原始 .m2（尚未转换的版本）。"),
    ("If the model folder contains a .skel file: use MultiConverter Shadowlands-Wotlk.exe (listfile.csv must sit next to the exe).", "如果模型文件夹包含 .skel：使用 MultiConverter Shadowlands-Wotlk.exe（listfile.csv 必须与 exe 放在同一目录）。"),
    ("Convert it, then load the NEW converted .m2 back into Conjure here and press \"Re-check model\".", "完成转换后，把新的 .m2 重新载入 Conjure，并点击“重新检查模型”。"),
    ("Packing this model anyway will very likely crash the client or render broken geometry. Only continue if you understand and accept that.", "强行打包该模型很可能导致客户端崩溃或模型几何体显示异常。只有在你理解并接受风险时才继续。"),
    ("Fix the file, or choose a different one, then click \"Re-check model\".", "修复该文件或选择其他文件，然后点击“重新检查模型”。"),
    ("vertex count matches the skin span (or there's no skin file here to check against).", "顶点数量与 skin 范围匹配（或者当前目录没有可用于核对的 skin 文件）。"),
    ("external .anim file(s) required", "需要外部 .anim 文件"),
    ("these MUST be packed alongside the model or those animations will crash the client.", "这些文件必须与模型一起打包，否则相关动画可能导致客户端崩溃。"),
    ("nothing extra to pack for animations.", "动画无需额外打包文件。"),
    ("Pack into patch-c.mpq under", "打包到 patch-c.mpq 中的目录"),
    ("Place these DBCs in BOTH locations:", "将以下 DBC 同时放入两个位置："),
    ("the client patch's DBFilesClient\\  AND  (b) the server's dbc\\ folder.", "客户端补丁的 DBFilesClient\\，以及 (b) 服务端的 dbc\\ 文件夹。"),
    ("Missing the server copy = the creature will be invisible/unreachable.", "缺少服务端 DBC 副本会导致生物不可见或无法访问。"),
    ("Run the SQL below (also saved as REPOINT.sql) against acore_world.", "在 acore_world 数据库中执行下面的 SQL（同时已保存为 REPOINT.sql）。"),
    ("Clear the WDB cache", "清理 WDB 缓存"),
    ("restart worldserver, and look at the creature in-game.", "重启 worldserver，然后进入游戏查看该生物。"),
    ("SQL not generated yet — go back to Stage 4", "尚未生成 SQL — 请返回步骤 4"),
    ("Manual/advanced — community ports.", "手动/高级 — 社区模型。"),
    ("Use this when textures are fed via the DBC", "当纹理由 DBC 提供时使用此功能"),
    ("For your own baked-texture ports, use Bake Textures instead.", "如果是你自己制作的烘焙纹理模型，请改用“烘焙纹理”。"),
    ("Choose a valid", "请选择有效的"),
    ("must be integers", "必须是整数"),
    ("successfully", "成功"),
    ("Model folder name", "模型文件夹名称"),
    ("Model folder", "模型文件夹"),
    ("Model name", "模型名称"),
    ("Texture paths", "纹理路径"),
    ("texture path", "纹理路径"),
    ("Output directory", "输出目录"),
]

def tr(value):
    if LANG != "zh_CN" or not isinstance(value, str) or not value:
        return value
    if value in EXACT:
        return EXACT[value]
    out = value
    for a, b in sorted(PHRASES, key=lambda x: len(x[0]), reverse=True):
        out = out.replace(a, b)
    return out

_patched = False

def patch_gui():
    global _patched
    if _patched:
        return
    _patched = True

    widget_classes = [
        ttk.Label, ttk.Button, ttk.LabelFrame, ttk.Checkbutton,
        ttk.Radiobutton, tk.Label, tk.Button, tk.Checkbutton, tk.Radiobutton
    ]
    for cls in widget_classes:
        original_init = cls.__init__
        def make_init(orig):
            def wrapped(self, *a, **kw):
                if "text" in kw:
                    kw["text"] = tr(kw["text"])
                return orig(self, *a, **kw)
            return wrapped
        cls.__init__ = make_init(original_init)

        original_configure = cls.configure
        def make_configure(orig):
            def wrapped(self, cnf=None, **kw):
                if isinstance(cnf, dict) and "text" in cnf:
                    cnf = dict(cnf)
                    cnf["text"] = tr(cnf["text"])
                if "text" in kw:
                    kw["text"] = tr(kw["text"])
                if cnf is None:
                    return orig(self, **kw)
                return orig(self, cnf, **kw)
            return wrapped
        cls.configure = make_configure(original_configure)
        cls.config = cls.configure

    original_add = ttk.Notebook.add
    def notebook_add(self, child, **kw):
        if "text" in kw:
            kw["text"] = tr(kw["text"])
        return original_add(self, child, **kw)
    ttk.Notebook.add = notebook_add

    original_title = tk.Tk.title
    def title(self, string=None):
        if string is None:
            return original_title(self)
        return original_title(self, tr(string))
    tk.Tk.title = title

    for name in ("showinfo", "showwarning", "showerror", "askyesno", "askokcancel", "askretrycancel", "askquestion"):
        if hasattr(messagebox, name):
            orig = getattr(messagebox, name)
            def make_box(fn):
                def wrapped(title, message, *a, **kw):
                    return fn(tr(title), tr(message), *a, **kw)
                return wrapped
            setattr(messagebox, name, make_box(orig))

    for name in ("askopenfilename", "askopenfilenames", "asksaveasfilename", "askdirectory"):
        if hasattr(filedialog, name):
            orig = getattr(filedialog, name)
            def make_fd(fn):
                def wrapped(*a, **kw):
                    if "title" in kw:
                        kw["title"] = tr(kw["title"])
                    if "filetypes" in kw and kw["filetypes"]:
                        kw["filetypes"] = [(tr(label), pattern) for label, pattern in kw["filetypes"]]
                    return fn(*a, **kw)
                return wrapped
            setattr(filedialog, name, make_fd(orig))

    original_tk_init = tk.Tk.__init__
    def tk_init(self, *a, **kw):
        original_tk_init(self, *a, **kw)
        try:
            menubar = tk.Menu(self)
            language_menu = tk.Menu(menubar, tearoff=0)
            lang_var = tk.StringVar(value=LANG)

            def switch(new_lang):
                if new_lang == LANG:
                    return
                save_lang(new_lang)
                if getattr(sys, "frozen", False):
                    os.execl(sys.executable, sys.executable, "--lang", new_lang)
                else:
                    os.execl(sys.executable, sys.executable, os.path.abspath(__file__), "--lang", new_lang)

            language_menu.add_radiobutton(label="简体中文", variable=lang_var, value="zh_CN", command=lambda: switch("zh_CN"))
            language_menu.add_radiobutton(label="English", variable=lang_var, value="en_US", command=lambda: switch("en_US"))
            menubar.add_cascade(label="语言 / Language", menu=language_menu)
            self.configure(menu=menubar)
        except tk.TclError:
            pass
    tk.Tk.__init__ = tk_init

def upstream_gui_path():
    root = resource_base()
    candidates = [
        os.path.join(root, "upstream", "conjure.py"),
        os.path.join(root, "Conjure-main", "conjure.py"),
        os.path.join(root, "conjure.py"),
    ]
    me = os.path.abspath(__file__)
    for p in candidates:
        if os.path.exists(p) and os.path.abspath(p) != me:
            return p
    return None

def show_missing_upstream():
    root = tk.Tk()
    root.withdraw()
    msg = (
        "未找到内置的原版 Conjure 源码。\n\n请重新下载完整成品包。"
        if LANG == "zh_CN" else
        "Bundled upstream Conjure source was not found.\n\nPlease download the complete build again."
    )
    messagebox.showerror("Conjure Bilingual", msg)
    root.destroy()

def main():
    patch_gui()
    gui_path = upstream_gui_path()
    if not gui_path:
        show_missing_upstream()
        return 2

    app_dir = os.path.dirname(gui_path)
    if app_dir not in sys.path:
        sys.path.insert(0, app_dir)

    spec = importlib.util.spec_from_file_location("conjure_upstream_gui", gui_path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)

    if hasattr(module, "GUIDED_STAGE_NAMES"):
        module.GUIDED_STAGE_NAMES = [tr(x) for x in module.GUIDED_STAGE_NAMES]
    if hasattr(module, "WINDOW_TITLE"):
        module.WINDOW_TITLE = tr(module.WINDOW_TITLE)

    if hasattr(module, "set_report_text"):
        original_set_report_text = module.set_report_text
        def set_report_text_i18n(widget, text):
            return original_set_report_text(widget, tr(text))
        module.set_report_text = set_report_text_i18n

    module.main()
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
