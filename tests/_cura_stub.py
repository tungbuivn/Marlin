"""Gia lap toi thieu moi truong plugin cua Cura de chay script hau xu ly ngoai Cura.

Cura nap script hau xu ly nhu mot module trong plugin cua chinh no:

    <plugin>/__init__.py
    <plugin>/Script.py                 <-- lop co so cura.Script
    <plugin>/scripts/__init__.py
    <plugin>/scripts/<TenScript>.py    <-- dung "from ..Script import Script"

Nen khong the import thang file script: phai dung lai cay thu muc do. Module nay
lam viec do cho ca test hoi quy lan harness chay tren file gcode that.
"""
import importlib
import os
import re
import shutil
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
SCRIPTS = os.path.join(os.path.dirname(HERE), "cura_profile", "scripts")

STUB = '''\
class Script:
    """Stub toi thieu cua cura.Script: chi nhung gi cac script nay dung."""

    def getSettingValueByKey(self, key):
        return self._settings.get(key)

    def getValue(self, line, key, default=None):
        if key in line:
            try:
                return line.split(key)[1].split(" ")[0]
            except IndexError:
                return default
        return default

    def putValue(self, line, **kwargs):
        for key, value in kwargs.items():
            if key in line:
                line = line.replace(key + self.getValue(line, key), key + str(value))
            else:
                line += " " + key + str(value)
        return line
'''


def build_module(script_name, pkg_name=None):
    """Nap <script_name>.py tu cura_profile/scripts, tra ve module da import."""
    pkg = os.path.join(tempfile.gettempdir(),
                       pkg_name or ("cura_stub_" + script_name.lower()))
    if os.path.isdir(pkg):
        shutil.rmtree(pkg)
    scripts = os.path.join(pkg, "scripts")
    os.makedirs(scripts)
    for d in (pkg, scripts):
        with open(os.path.join(d, "__init__.py"), "w", encoding="utf-8") as f:
            f.write("")
    with open(os.path.join(pkg, "Script.py"), "w", encoding="utf-8") as f:
        f.write(STUB)
    shutil.copyfile(os.path.join(SCRIPTS, script_name + ".py"),
                    os.path.join(scripts, script_name + ".py"))
    parent = os.path.dirname(pkg)
    if parent not in sys.path:
        sys.path.insert(0, parent)
    return importlib.import_module(os.path.basename(pkg) + ".scripts." + script_name)


def chunk_like_cura(text):
    """Chia gcode thanh chunk theo dung kieu Cura: moi ';LAYER:n' mo dau mot chunk.

    Cura truyen cho script mot list cac chunk; Start G-code va duong purge nam
    CHUNG chunk voi ';LAYER:0' chu khong tach rieng. Cach chia nay giu dung
    tinh chat do -- day chinh la chi tiet tung lam script quet thieu retract.
    """
    chunks, cur = [], []
    for line in text.split("\n"):
        if line.startswith(";LAYER:") and cur:
            chunks.append("\n".join(cur))
            cur = []
        cur.append(line)
    if cur:
        chunks.append("\n".join(cur))
    return chunks


def read_cura_settings(global_cfg, script_name):
    """Doc khoi 'post_processing_scripts' cua machine instance -> dict setting.

    Cura luu ca khoi duoi dang mot chuoi co escape newline. So dau `\\` truoc `n`
    KHONG co dinh: file that tren may nay ghi **ba** dau `\\` + `n`
    (`[FirstLayerTwice]\\\\\\nenabled = True\\\\\\n...`). Vi vay phai khop ca cum
    `\\+n` bang regex, chu khop cung 2 dau `\\` se de lai mot dau `\\` lung o dau
    moi dong -> dong tieu de `[TenScript]` khong con nhan ra duoc.

    Tra ve dict cua rieng <script_name>, hoac None neu khong tim thay.
    """
    with open(global_cfg, "r", encoding="utf-8") as f:
        for line in f:
            if not line.startswith("post_processing_scripts"):
                continue
            raw = re.sub(r"\\+n", "\n", line.split("=", 1)[1])
            settings, current = {}, None
            for entry in raw.split("\n"):
                entry = entry.strip()
                if entry.startswith("[") and entry.endswith("]"):
                    current = entry[1:-1]
                    settings[current] = {}
                elif "=" in entry and current is not None:
                    key, value = entry.split("=", 1)
                    settings[current][key.strip()] = value.strip()
            return settings.get(script_name)
    return None


def coerce(value):
    """Chuyen chuoi trong cfg ve bool/int/float/str cho giong Cura tra ve."""
    if value in ("True", "False"):
        return value == "True"
    for cast in (int, float):
        try:
            return cast(value)
        except ValueError:
            pass
    return value
