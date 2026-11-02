#!/usr/bin/env python3
"""Prepare a *private review bundle* from the real, tested XIAO source tree.

The bundle is intended for review before any public GitHub upload.
It is NOT automatically safe to publish: review source files and merged.hex
for embedded Matter factory data, development keys, local identifiers, etc.

Run on the development Mac:
  python3 tools/export_tested_snapshot.py

Defaults assume:
  ~/Documents/XIAO/matter_mailbox
  ~/Documents/XIAO/board-root
  ~/Downloads/XIAO-nRF54L15-Matter-Vibration-Sensor-review.zip
"""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import sys
import zipfile

ROOT_FILES = {
    "CMakeLists.txt", "prj.conf", "app.overlay", "pm_static.yml",
    "sysbuild.conf", "Kconfig", "west.yml", "VERSION",
}
ROOT_DIRS = ("src", "include", "boards", "dts", "sysbuild", "modules")
ALLOWED_EXT = {
    ".c", ".cc", ".cpp", ".h", ".hpp", ".inc", ".def",
    ".conf", ".overlay", ".dts", ".dtsi", ".yml", ".yaml",
    ".cmake", ".txt", ".zap", ".json", ".xml", ".py", ".sh",
    ".md", ".rst", ".ld", ".in",
}
EXCLUDE_DIRS = {
    "build", ".git", ".venv", "__pycache__", ".cache",
    ".vscode", ".idea", "node_modules", "CMakeFiles",
}
EXCLUDE_EXT = {
    ".pem", ".key", ".p12", ".pfx", ".der", ".crt",
    ".bin", ".hex", ".elf", ".o", ".a", ".log", ".zip",
    ".tar", ".gz", ".bak", ".swp",
}
EXCLUDE_NAME_PARTS = (
    "password", "secret", "credential", "private_key",
    "thread_dataset", "factory_data", "before_", "backup_",
)
MAX_TEXT_FILE_BYTES = 3_000_000


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def safe_source(path: Path) -> bool:
    if path.is_symlink() or not path.is_file():
        return False
    if path.suffix.lower() in EXCLUDE_EXT:
        return False
    if any(x in path.name.lower() for x in EXCLUDE_NAME_PARTS):
        return False
    return (path.suffix.lower() in ALLOWED_EXT
            or path.name in ROOT_FILES
            or path.name in {"CMakeLists.txt", "Kconfig", "Kconfig.defconfig", "Makefile"})


def gather_dir(base: Path, name: str):
    directory = base / name
    if not directory.is_dir():
        return
    for dirpath, dirnames, filenames in os.walk(directory, followlinks=False):
        dirnames[:] = sorted(
            d for d in dirnames
            if d not in EXCLUDE_DIRS and not (Path(dirpath) / d).is_symlink()
        )
        for filename in sorted(filenames):
            path = Path(dirpath) / filename
            if safe_source(path):
                yield path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--project",
        type=Path,
        default=Path("~/Documents/XIAO/matter_mailbox").expanduser(),
    )
    parser.add_argument(
        "--board-root",
        type=Path,
        default=Path("~/Documents/XIAO/board-root").expanduser(),
    )
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(
            "~/Downloads/XIAO-nRF54L15-Matter-Vibration-Sensor-review.zip"
        ).expanduser(),
    )
    args = parser.parse_args()

    project = args.project.expanduser().resolve()
    board_root = args.board_root.expanduser().resolve()
    output = args.output.expanduser()

    firmware = project / "build" / "merged.hex"
    for mandatory in (
        project / "CMakeLists.txt",
        project / "prj.conf",
        project / "src" / "app_task.cpp",
        project / "src" / "battery_monitor.cpp",
        project / "src" / "default_zap" / "template.zap",
        firmware,
    ):
        if not mandatory.is_file():
            sys.exit(f"Missing required file; stopped: {mandatory}")

    if not board_root.is_dir():
        sys.exit(f"Missing custom board root; stopped: {board_root}")

    sources = {}
    for filename in sorted(ROOT_FILES):
        candidate = project / filename
        if candidate.is_file() and safe_source(candidate):
            sources[f"project/{filename}"] = candidate

    for dirname in ROOT_DIRS:
        for path in gather_dir(project, dirname):
            sources["project/" + path.relative_to(project).as_posix()] = path

    for dirname in ("boards", "dts", "include", "zephyr", "modules"):
        for path in gather_dir(board_root, dirname):
            sources["board-root/" + path.relative_to(board_root).as_posix()] = path

    if len(sources) < 8:
        sys.exit("Unexpectedly few source files; stopped before export.")

    manifest = {
        "purpose": "PRIVATE REVIEW ONLY - NOT CLEARED FOR PUBLICATION",
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "board": "xiao_nrf54l15/nrf54l15/cpuapp",
        "sdk": "nRF Connect SDK v3.0.2",
        "note": (
            "Contains actual local tested sources and merged.hex. "
            "Review for secrets/attestation private keys and licensing "
            "before publication; raw HEX may be device-specific."
        ),
        "files": [],
    }

    all_files = dict(sources)
    all_files["firmware/merged.hex"] = firmware
    output.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED,
                         compresslevel=6) as archive:
        for archive_name, original in sorted(all_files.items()):
            if original.is_symlink():
                continue
            if archive_name != "firmware/merged.hex" and original.stat().st_size > MAX_TEXT_FILE_BYTES:
                sys.exit(f"Large source file ({original}); investigate before export.")
            data = original.read_bytes()
            archive.writestr(archive_name, data)
            manifest["files"].append({
                "path": archive_name,
                "size": len(data),
                "sha256": sha256(data),
            })
        archive.writestr(
            "SNAPSHOT-MANIFEST.json",
            json.dumps(manifest, indent=2, ensure_ascii=False) + "\n",
        )

    print(f"Created private review bundle: {output}")
    print(f"Files: {len(all_files)}")
    print(f"Firmware SHA256: {sha256(firmware.read_bytes())}")
    print(f"ZIP SHA256: {sha256(output.read_bytes())}")
    print("IMPORTANT: This ZIP includes the real compiled merged.hex.")
    print("DO NOT PUBLISH IT BEFORE A FIRMWARE/KEYS PRIVACY REVIEW.")
    print("Check that the 100ms LED pulse and SED/ICD source are indeed present.")
    print("Upload this ZIP for source review; no direct public upload is performed.")


if __name__ == "__main__":
    main()
