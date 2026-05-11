#!/usr/bin/env python3
"""Lightweight public metadata consistency checks for FreeEQ8.

This does not build the plugin. It checks crawler-facing text, versions,
JSON metadata, and common README anchors so the repo stays coherent when
indexed by GitHub, search engines, package lists, and curated audio lists.
"""
from __future__ import annotations

import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
README = ROOT / "README.md"
CMAKE = ROOT / "CMakeLists.txt"
CONFIG = ROOT / "Source" / "Config.h"
SERVER_PACKAGE = ROOT / "server" / "package.json"
CODEMETA = ROOT / "codemeta.json"

BLOCKED_PHRASES = [
    "FabFilter killer",
    "Pro-Q replacement",
    "competing directly",
    "guaranteed professional results",
    "best EQ",
    "most advanced EQ",
    "only free EQ",
]

REQUIRED_README_PHRASES = [
    "FreeEQ8 is a free, GPL-3.0, 8-band parametric EQ plugin",
    "Current stable release:** `v2.2.0`",
    "Development lane:** `v2.3.0-dev`",
    "ProEQ8 is the optional 24-band commercial build",
]


def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)


def github_slug(text: str) -> str:
    text = text.strip().lower()
    text = re.sub(r"<[^>]+>", "", text)
    text = re.sub(r"[`*_~]", "", text)
    text = re.sub(r"[^a-z0-9\s-]", "", text)
    text = re.sub(r"\s+", "-", text.strip())
    return re.sub(r"-+", "-", text)


def main() -> int:
    readme = README.read_text(encoding="utf-8")
    cmake = CMAKE.read_text(encoding="utf-8")
    config = CONFIG.read_text(encoding="utf-8")
    server = json.loads(SERVER_PACKAGE.read_text(encoding="utf-8"))
    codemeta = json.loads(CODEMETA.read_text(encoding="utf-8"))

    cmake_version = re.search(r"project\(FreeEQ8 VERSION ([0-9.]+)\)", cmake)
    config_version = re.search(r'kVersion\s*=\s*"([0-9.]+)"', config)
    if not cmake_version or not config_version:
        fail("Could not find plugin version in CMakeLists.txt or Source/Config.h")

    plugin_version = cmake_version.group(1)
    if config_version.group(1) != plugin_version:
        fail(f"CMake version {plugin_version} != Source/Config.h {config_version.group(1)}")
    if codemeta.get("version") != plugin_version:
        fail(f"codemeta version {codemeta.get('version')} != plugin version {plugin_version}")

    if server.get("version") != "2.0.0":
        fail("server/package.json must stay at license-server API version 2.0.0 unless docs are updated")

    for phrase in BLOCKED_PHRASES:
        if phrase.lower() in readme.lower():
            fail(f"Blocked crawler-risk phrase found in README: {phrase}")

    for phrase in REQUIRED_README_PHRASES:
        if phrase not in readme:
            fail(f"Required README phrase missing: {phrase}")

    headings = {github_slug(m.group(2)) for m in re.finditer(r"^(#{1,6})\s+(.+)$", readme, flags=re.M)}
    anchors = re.findall(r"\]\(#([^\)]+)\)", readme)
    missing = [a for a in anchors if a not in headings]
    if missing:
        fail(f"Broken README anchors: {missing}")

    print("SEO consistency checks passed.")
    print(f"Plugin version: {plugin_version}")
    print(f"License server API version: {server['version']}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
