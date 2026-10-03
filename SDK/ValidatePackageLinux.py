# Copyright (c) 2026 Jose Antonio Escribano joseantonioescribanoayllon@gmail.com
"""Validate an extracted Linux SDK using only the files in that package."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import tempfile

VARIANTS = ("DebugPlain", "DebugInstrumented", "ReleasePlain", "ReleaseInstrumented")
FORBIDDEN_PATH = re.compile(r"(?:^|/)Domain/(?:Interpreter|Nodes|Parser|Loader|Semantic|Tooling)/|(?:^|/)HTNDomainHelpers\.|HTNInterpreted(?:PlannerHook|PlanningUnit)\.")
FORBIDDEN_REFERENCE = re.compile(r"Domain[/\\](?:Interpreter|Nodes|Parser|Loader|Semantic|Tooling)[/\\]|HTNNodeVisitorContextBase|HTNInterpreted(?:PlannerHook|PlanningUnit)|Domain[/\\]HTNDomainHelpers\.h")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def digest(path):
    with open(path, "rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def run(command, log, cwd=None, failure=None):
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("w") as output:
        result = subprocess.run(list(map(str, command)), cwd=cwd, stdout=output, stderr=subprocess.STDOUT)
    if failure is not None:
        require(result.returncode != 0 and failure in log.read_text(), f"Expected failure '{failure}': {log}")
    else:
        require(result.returncode == 0, f"Command failed ({result.returncode}): {log}\n{log.read_text()[-6000:]}")
    print(f"PASS: {log}", flush=True)


def verify_package(root):
    manifest = json.loads((root / "manifest.json").read_text())
    require(manifest["platform"] == "linux-x86_64" and platform.system() == "Linux" and platform.machine() == "x86_64",
            "This package requires Linux x86_64")
    require(tuple(v["id"] for v in manifest["variants"]) == VARIANTS, "Unexpected Linux SDK variants")
    receipt = json.loads((root / "build-provenance.json").read_text())
    require(receipt["build_id"] == manifest["build_id"] and receipt["version"] == manifest["sdk_version"] and receipt["rebuilt"],
            "Package/build provenance mismatch")
    checked = set()
    for line in (root / "CHECKSUMS.sha256").read_text().splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        require(match is not None, "Invalid checksum entry")
        expected, name = match.groups()
        path = root / name
        require(path.resolve().is_relative_to(root) and name not in checked and not path.is_symlink(), f"Invalid checksum path: {name}")
        require(path.is_file() and digest(path) == expected, f"Checksum mismatch: {name}")
        checked.add(name)
    files = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()}
    require(files == checked | {"CHECKSUMS.sha256"}, "Unverified or missing package files")
    for artifact in receipt["artifacts"]:
        require(artifact["package_path"] in checked and digest(root / artifact["package_path"]) == artifact["sha256"],
                f"Artifact does not match recorded build: {artifact['package_path']}")
    require(len(receipt["artifacts"]) == 13, "Expected twelve libraries and one translator")
    for header in (root / "include").rglob("*"):
        if header.is_file():
            require(not FORBIDDEN_PATH.search(header.relative_to(root).as_posix()) and
                    not FORBIDDEN_REFERENCE.search(header.read_text()), f"Interpreter dependency in SDK: {header}")
    print("PASS: manifest, artifact provenance, all checksums and header boundary", flush=True)
    return manifest


def validate(root, build, cc, cxx, jobs):
    verify_package(root)
    require(not build.exists(), f"Validation requires a fresh directory: {build}")
    # Copy examples away from the package so even their CMake source tree is external.
    import shutil
    shutil.copytree(root / "examples", build / "examples")
    source = build / "examples"
    common = ["cmake", "-S", source, "-G", "Ninja", f"-DHTN_DIR={root}/cmake",
              "-DCMAKE_BUILD_TYPE=Validation", f"-DCMAKE_C_COMPILER={cc}", f"-DCMAKE_CXX_COMPILER={cxx}"]
    for variant in VARIANTS:
        directory = build / variant
        run([*common, "-B", directory, f"-DHTN_VARIANT_VALIDATION={variant}"], build / f"{variant}-configure.log")
        run(["cmake", "--build", directory, "--parallel", jobs], build / f"{variant}-build.log")
        run(["ctest", "--test-dir", directory, "--output-on-failure"], build / f"{variant}-tests.log")
        require(re.search(r"100% tests passed, 0 tests failed out of 5\b", (build / f"{variant}-tests.log").read_text()),
                "Expected five passing package checks per variant")
    run([*common, "-B", build / "unknown", "-DHTN_VARIANT_VALIDATION=UnknownVariant"],
        build / "unknown-variant.log", failure="Unknown HTN variant")
    run([*common, "-B", build / "old-stdlib-abi", "-DHTN_VARIANT_VALIDATION=ReleasePlain",
         "-DCMAKE_CXX_FLAGS=-D_GLIBCXX_USE_CXX11_ABI=0"], build / "incompatible-stdlib.log", failure="HTN requires libstdc++")
    run([*common, "-B", build / "debug-stdlib", "-DHTN_VARIANT_VALIDATION=DebugPlain",
         "-DCMAKE_CXX_FLAGS=-D_GLIBCXX_DEBUG"], build / "incompatible-debug-stdlib.log", failure="HTN requires libstdc++")
    print(f"PASS: four Linux variants, 12 consumer executions, four ELF export checks, four incompatible-domain checks ({cxx}).", flush=True)
    print(f"PASS: unknown variant and incompatible libstdc++ ABIs rejected. Logs: {build}", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--cc", default=os.environ.get("CC", "gcc-14"))
    parser.add_argument("--cxx", default=os.environ.get("CXX", "g++-14"))
    parser.add_argument("--jobs", type=int, default=4)
    args = parser.parse_args()
    root = Path(__file__).resolve().parent
    build = args.build_root or Path(tempfile.mkdtemp(prefix="htn-sdk-linux-validation-")) / "consumer"
    validate(root, build.resolve(), args.cc, args.cxx, args.jobs)
