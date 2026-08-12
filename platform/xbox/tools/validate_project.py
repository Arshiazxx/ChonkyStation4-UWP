#!/usr/bin/env python3
"""Static validation for the Xbox/UWP project structure.

This does not replace a Windows/MSBuild build. It catches broken project references,
manifest targeting mistakes, malformed XML, and missing assets/source files on any host.
"""
from __future__ import annotations

import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PROJECT = ROOT / "ChonkyStation4.Xbox" / "ChonkyStation4.Xbox.vcxproj"
MANIFEST = ROOT / "ChonkyStation4.Xbox" / "Package.appxmanifest"

MSBUILD_NS = {"m": "http://schemas.microsoft.com/developer/msbuild/2003"}
APPX_NS = {"a": "http://schemas.microsoft.com/appx/manifest/foundation/windows10"}


def fail(message: str) -> None:
    print(f"ERROR: {message}", file=sys.stderr)
    raise SystemExit(1)


def main() -> int:
    project_tree = ET.parse(PROJECT)
    manifest_tree = ET.parse(MANIFEST)

    project_root = project_tree.getroot()
    project_dir = PROJECT.parent

    referenced: list[Path] = []
    item_tags = ("ClCompile", "ClInclude", "ApplicationDefinition", "Page", "AppxManifest", "Image")
    for tag in item_tags:
        for node in project_root.findall(f".//m:{tag}", MSBUILD_NS):
            include = node.get("Include")
            if include:
                referenced.append(project_dir / Path(include.replace("\\", "/")))

    missing = [path for path in referenced if not path.exists()]
    if missing:
        fail("missing project files: " + ", ".join(str(path.relative_to(ROOT)) for path in missing))

    manifest_root = manifest_tree.getroot()
    target = manifest_root.find(".//a:Dependencies/a:TargetDeviceFamily", APPX_NS)
    if target is None:
        fail("Package.appxmanifest has no TargetDeviceFamily")
    if target.get("Name") != "Windows.Xbox":
        fail(f"unexpected target device family: {target.get('Name')!r}")

    globals_group = None
    for group in project_root.findall("m:PropertyGroup", MSBUILD_NS):
        if group.get("Label") == "Globals":
            globals_group = group
            break
    if globals_group is None:
        fail("vcxproj has no Globals PropertyGroup")

    def get_global(name: str) -> str | None:
        node = globals_group.find(f"m:{name}", MSBUILD_NS)
        return None if node is None else node.text

    if get_global("AppContainerApplication") != "true":
        fail("AppContainerApplication must be true")
    if get_global("ApplicationType") != "Windows Store":
        fail("ApplicationType must be Windows Store")

    print(f"PASS: {PROJECT.relative_to(ROOT)} XML and file references")
    print(f"PASS: manifest target {target.get('Name')} {target.get('MinVersion')}..{target.get('MaxVersionTested')}")
    print(f"PASS: AppContainerApplication={get_global('AppContainerApplication')}")
    print(f"PASS: ApplicationType={get_global('ApplicationType')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
