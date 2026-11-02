#!/usr/bin/env python3
"""Import the reviewed 2026-11-02 source snapshot into the public repository.

Copies source and configuration only. The HEX (with Matter factory data)
is verified but NEVER copied, committed or pushed.

Usage on the development Mac:
    python3 tools/import_reviewed_snapshot.py --push
"""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import subprocess
import sys
import zipfile

REPO_URL = "https://github.com/blb4github/XIAO-nRF54L15-Matter-Vibration-Sensor.git"
EXPECTED_ZIP_SHA256 = "0e2ded4b1dca70aed41d33279d3876920d0afb282be52be9dcbd97060dbffa40"
EXPECTED_FIRMWARE_SHA256 = "17a7dae001e7a008205ec06e5f712771bbc5107c8a71ca221ceb4092e89c05f9"

SENSITIVE_PATTERNS = [
    re.compile(rb"-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----"),
    re.compile(rb"(?im)^\s*(?:api_key|access_token|password|private_key)\s*[:=]\s*['\"][^'\"]+['\"]"),
    re.compile(rb"\b(?:172\.31\.|192\.168\.|10\.10\.)\d{1,3}(?:\.\d{1,3}){1,2}\b"),
    re.compile(rb"(?i)\b(?:mqtt\.pet10|bierhaus\.nl|pushover)\b"),
]


def run(*cmd):
    print("$", " ".join(str(x) for x in cmd), flush=True)
    subprocess.run([str(x) for x in cmd], check=True)


def fail(message):
    sys.exit("ERROR: " + message)


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--zip",
        type=Path,
        default=Path("~/Downloads/XIAO-nRF54L15-Matter-Vibration-Sensor-review.zip").expanduser(),
        help="Local ZIP created from tested Mac project",
    )
    parser.add_argument(
        "--destination",
        type=Path,
        default=Path("~/Documents/XIAO/XIAO-nRF54L15-Matter-Vibration-Sensor-public").expanduser(),
        help="New directory for public Git working copy",
    )
    parser.add_argument(
        "--push", action="store_true",
        help="Push committed source (no firmware) to origin/main",
    )
    args = parser.parse_args()
    zip_path = args.zip.expanduser().resolve()
    destination = args.destination.expanduser()

    if not zip_path.is_file():
        fail(f"Review ZIP not found: {zip_path}")
    if sha256(zip_path.read_bytes()) != EXPECTED_ZIP_SHA256:
        fail("ZIP hash differs from the reviewed snapshot; stopped")
    if destination.exists():
        fail(f"Destination exists; choose a different --destination: {destination}")

    source_files = {}
    with zipfile.ZipFile(zip_path) as z:
        manifest = json.loads(z.read("SNAPSHOT-MANIFEST.json"))
        manifest_paths = {e["path"] for e in manifest["files"]}
        if set(z.namelist()) != manifest_paths | {"SNAPSHOT-MANIFEST.json"}:
            fail("ZIP filenames differ from manifest")
        if "firmware/merged.hex" not in manifest_paths:
            fail("Missing firmware in original snapshot")

        for entry in manifest["files"]:
            name = entry["path"]
            parts = PurePosixPath(name).parts
            if not parts or any(part in (".", "..") for part in parts):
                fail(f"Unsafe archive path: {name!r}")
            data = z.read(name)
            if sha256(data) != entry["sha256"] or len(data) != entry["size"]:
                fail(f"Manifest hash or length mismatch: {name}")

            if name == "firmware/merged.hex":
                if sha256(data) != EXPECTED_FIRMWARE_SHA256:
                    fail("Firmware hash differs from reviewed build")
                continue

            if name.startswith("project/"):
                target = name.removeprefix("project/")
            elif name.startswith("board-root/"):
                target = name
            else:
                fail(f"Unexpected source path: {name}")

            if b"\x00" in data:
                fail(f"Unexpected binary data in source: {name}")
            for pattern in SENSITIVE_PATTERNS:
                if pattern.search(data):
                    fail(f"Potential sensitive data: {name}; review manually")
            source_files[target] = data

    if len(source_files) < 45:
        fail(f"Unexpectedly few source files: {len(source_files)}")
    print(f"Verified source snapshot: {len(source_files)} files")
    print("IMPORTANT: firmware/merged.hex is excluded")

    destination.parent.mkdir(parents=True, exist_ok=True)
    run("git", "clone", REPO_URL, destination)

    for relative, data in sorted(source_files.items()):
        path = destination / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        if path.exists():
            fail(f"Refusing to replace existing repository file: {relative}")
        path.write_bytes(data)

    note = (
        "# Tested source snapshot - 2 November 2026\n\n"
        "Copied byte-for-byte from the reviewed Mac project export.\n\n"
        "Board: Seeed XIAO nRF54L15 Sense\n\n"
        "SDK: Nordic NCS 3.0.2\n\n"
        "Firmware SHA-256: " + EXPECTED_FIRMWARE_SHA256 + "\n\n"
        "Reviewed export ZIP SHA-256: " + EXPECTED_ZIP_SHA256 + "\n\n"
        "The firmware HEX is NOT included: its factory-data section contains "
        "commissioning/attestation material. Do not redistribute the binary "
        "without generating a device-neutral and audited provisioning scheme.\n\n"
        "Original Nordic LicenseRef-Nordic-5-Clause and Apache-2.0 headers "
        "remain intact. The repository MIT license does not relicense "
        "third-party code. Some original mailbox-related identifiers remain "
        "in the tested source; the firmware is not yet renamed/refactored.\n"
    )
    (destination / "SOURCE-SNAPSHOT.md").write_text(note)

    run("git", "-C", destination, "add", "--",
        "CMakeLists.txt", "Kconfig", "VERSION", "app.overlay",
        "pm_static.yml", "prj.conf", "sysbuild.conf", "boards",
        "src", "sysbuild", "board-root", "SOURCE-SNAPSHOT.md")
    run("git", "-C", destination, "status", "--short")
    run("git", "-C", destination, "commit", "-m",
        "firmware: import verified XIAO Matter vibration sensor source")

    if args.push:
        run("git", "-C", destination, "push", "origin", "HEAD:main")
        print("Source published. Firmware and device factory data NOT published.")
    else:
        print("Committed locally, not pushed. To publish:")
        print(f"  git -C {destination} push origin HEAD:main")
    print("Local Git working copy:", destination)


if __name__ == "__main__":
    main()
