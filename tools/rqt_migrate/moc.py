"""Getting moc's JSON for a file: run moc, or read a .json saved earlier."""

from __future__ import annotations

import glob
import json
import os
import shutil
import subprocess
import tempfile
from typing import Callable, Optional

# (absolute path, path relative to the tree) -> (moc JSON or None, error text or None)
JsonProvider = Callable[[str, str], "tuple[Optional[dict], Optional[str]]"]


def find_moc(explicit: str | None) -> str | None:
    if explicit:
        return explicit
    qtdir = os.environ.get("QTDIR")
    if qtdir:
        for candidate in (os.path.join(qtdir, "libexec", "moc"), os.path.join(qtdir, "bin", "moc")):
            if os.access(candidate, os.X_OK):
                return candidate
    return shutil.which("moc")


def qt_include_dirs(moc_path: str) -> list[str]:
    """The Qt prefix's include dir and every module dir under it, for Q_INTERFACES and QML macros."""
    prefix = os.path.dirname(os.path.dirname(os.path.realpath(moc_path)))
    include = os.path.join(prefix, "include")
    if not os.path.isdir(include):
        return []
    return [include] + sorted(d for d in glob.glob(os.path.join(include, "Qt*")) if os.path.isdir(d))


def running(moc_path: str, include_dirs: list[str], save_dir: str | None = None) -> JsonProvider:
    def provide(path: str, rel: str):
        with tempfile.TemporaryDirectory() as tmp:
            out = os.path.join(tmp, "moc_out.cpp")
            cmd = [moc_path, "--output-json", "-o", out]
            cmd += [f"-I{d}" for d in include_dirs]
            cmd += [path]
            proc = subprocess.run(cmd, capture_output=True, text=True)
            json_path = out + ".json"
            if proc.returncode != 0 or not os.path.exists(json_path):
                error = (proc.stderr or proc.stdout).strip() or f"moc exited with {proc.returncode}"
                # moc names the file by its absolute path; keep the saved text portable.
                error = error.replace(path, rel)
                _save(save_dir, rel + ".error", error + "\n")
                return None, error
            with open(json_path, encoding="utf-8") as f:
                data = json.load(f)
            _save(save_dir, rel + ".json", json.dumps(data, indent=1, sort_keys=True) + "\n")
            return data, None

    return provide


def _save(save_dir: str | None, rel: str, text: str) -> None:
    if not save_dir:
        return
    target = os.path.join(save_dir, rel)
    os.makedirs(os.path.dirname(target), exist_ok=True)
    with open(target, "w", encoding="utf-8") as f:
        f.write(text)


def saved(json_dir: str) -> JsonProvider:
    """JSON saved as <json_dir>/<relative path>.json; a missing file means moc found no class."""

    def provide(path: str, rel: str):
        target = os.path.join(json_dir, rel + ".json")
        error = os.path.join(json_dir, rel + ".error")
        if os.path.exists(error):
            with open(error, encoding="utf-8") as f:
                return None, f.read().strip()
        if not os.path.exists(target):
            return {"classes": []}, None
        with open(target, encoding="utf-8") as f:
            return json.load(f), None

    return provide
