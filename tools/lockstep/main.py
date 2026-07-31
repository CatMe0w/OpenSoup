#!/usr/bin/env python3
"Verifier that keeps OpenSoup bit-identical to the original Souptoys engine."
from __future__ import annotations

import argparse
import concurrent.futures
import os
import queue
import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
SCENARIOS = HERE / "scenarios"
PATCHER = REPO / "tools/patch-toybox-harness.py"

ASSETS_LEAF = "cat.me0w.opensoup/assets"


def default_assets() -> Path:
    if override := os.environ.get("OPENSOUP_ASSETS"):
        return Path(override)
    if sys.platform == "darwin":
        return Path.home() / "Library/Application Support" / ASSETS_LEAF
    if sys.platform == "win32":
        return Path(os.environ.get("LOCALAPPDATA", "")) / ASSETS_LEAF
    data_home = os.environ.get("XDG_DATA_HOME") or Path.home() / ".local/share"
    return Path(data_home) / ASSETS_LEAF


DEFAULT_BINARY = Path(
    os.environ.get("OPENSOUP_RUBYSCRIPT") or REPO / "build/opensoup_rubyscript"
)

PROMPT = "souptoys>"
# Windows prints three-digit exponents; C99 elsewhere prints two.
EXPONENT = re.compile(r"e([+-])0*(\d{2,})")


def normalize(text: str) -> list[str]:
    lines = []
    for line in text.replace("\r", "").split("\n"):
        while line.startswith(PROMPT):
            line = line[len(PROMPT) :]
        if line.startswith("=> "):  # console echo of a statement's value
            continue
        line = EXPONENT.sub(r"e\1\2", line).rstrip()
        if line:
            lines.append(line)
    return lines


def scenario_names(requested: list[str]) -> list[str]:
    known = sorted(p.stem for p in SCENARIOS.glob("*.rb"))
    if not requested:
        return known
    unknown = [n for n in requested if n not in known]
    if unknown:
        sys.exit(f"lockstep: no such scenario: {', '.join(unknown)}")
    return requested


# OpenSoup side


def run_opensoup(name: str, binary: Path, assets: Path) -> list[str]:
    result = subprocess.run(
        [str(binary), str(assets), str(SCENARIOS / f"{name}.rb")],
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        sys.stderr.write(result.stderr)
    return normalize(result.stdout)


# Original side


LOAD_REPEATS = 3
REGISTRY_SUBKEYS = (r"Software\Wow6432Node\Souptoys", r"Software\Souptoys")


def require_windows() -> Path:
    if sys.platform != "win32":
        sys.exit(
            "lockstep: recording requires Windows"
        )
    return Path(os.environ["TEMP"]) / "lockstep"


def install_dir() -> Path:
    import winreg

    for subkey in REGISTRY_SUBKEYS:
        try:
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, subkey) as key:
                return Path(winreg.QueryValueEx(key, "Path")[0])
        except OSError:
            continue
    sys.exit("lockstep: the original Souptoys is not installed")


def build_harness(staging: Path) -> None:
    source = install_dir()
    staging.mkdir(parents=True, exist_ok=True)
    (staging / "fmod.dll").write_bytes((source / "fmod.dll").read_bytes())
    subprocess.run(
        [sys.executable, str(PATCHER), str(source / "Toybox.exe"),
         str(staging / "harness.exe")],
        check=True,
    )


def record_original(staging: Path, name: str, slot: int, timeout: float) -> list[str]:
    work = staging / f"w{slot}"
    work.mkdir(parents=True, exist_ok=True)

    # Path must be absolute: the engine chdirs during boot.
    (work / "scenario.rb").write_text(
        (SCENARIOS / f"{name}.rb").read_text() + "\n$stdout.flush\nexit!\n"
    )
    load = str(work / "scenario.rb").replace("\\", "/")
    (work / "harness.in").write_text(f"$ran ||= load('{load}')\n" * LOAD_REPEATS)
    out = work / "harness.out"
    out.unlink(missing_ok=True)

    subprocess.run(
        [str(staging / "harness.exe")],
        env=os.environ | {
            "TOYBOX_HARNESS_IN": str(work / "harness.in"),
            "TOYBOX_HARNESS_OUT": str(out),
        },
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        timeout=timeout,
    )
    if not out.exists():
        raise RuntimeError("the original produced no output")
    lines = normalize(out.read_text(errors="replace"))
    if not lines:
        raise RuntimeError("the original printed nothing")
    return lines


# Commands


def report_failure(name: str, expected: list[str], actual: list[str], full: bool):
    differing = [
        i
        for i in range(max(len(expected), len(actual)))
        if expected[i : i + 1] != actual[i : i + 1]
    ]
    print(f"FAIL {name:<18} {len(expected)} expected, {len(actual)} actual, "
          f"{len(differing)} differ")
    for i in differing if full else differing[:3]:
        print(f"  line {i + 1}")
        print(f"    expected  {expected[i] if i < len(expected) else '<missing>'}")
        print(f"    actual    {actual[i] if i < len(actual) else '<missing>'}")
    if not full and len(differing) > 3:
        print(f"  ... {len(differing) - 3} more (--diff for all)")


def cmd_run(args) -> int:
    if not args.binary.exists():
        sys.exit(f"lockstep: no runner at {args.binary}; build it, or pass --binary")
    failed = []
    for name in scenario_names(args.names):
        golden = SCENARIOS / f"{name}.expected"
        if not golden.exists():
            print(f"FAIL {name:<18} no golden; run `record {name}`")
            failed.append(name)
            continue
        expected = golden.read_text().split("\n")[:-1]
        actual = run_opensoup(name, args.binary, args.assets)
        if expected == actual:
            print(f"PASS {name:<18} {len(actual)} lines")
        else:
            report_failure(name, expected, actual, args.diff)
            failed.append(name)
    print()
    print("failed:", " ".join(failed) if failed else "none")
    return 1 if failed else 0


def cmd_record(args) -> int:
    names = scenario_names(args.names)
    staging = require_windows()
    build_harness(staging)

    slots: queue.SimpleQueue[int] = queue.SimpleQueue()
    for slot in range(args.jobs):
        slots.put(slot)

    def record_one(name: str) -> tuple[str, str | None]:
        slot = slots.get()
        try:
            lines = record_original(staging, name, slot, args.timeout)
        except (RuntimeError, subprocess.SubprocessError) as error:
            return name, str(error) or type(error).__name__
        finally:
            slots.put(slot)
        (SCENARIOS / f"{name}.expected").write_text("\n".join(lines) + "\n")
        print(f"recorded {name:<18} {len(lines)} lines")
        return name, None

    failed = []
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as pool:
        for name, error in pool.map(record_one, names):
            if error:
                print(f"FAILED   {name:<18} {error}")
                failed.append(name)
    print()
    print("failed:", " ".join(failed) if failed else "none")
    return 1 if failed else 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    sub = parser.add_subparsers(dest="command")

    run = sub.add_parser("run", help="verify OpenSoup against the frozen goldens")
    run.add_argument("names", nargs="*")
    run.add_argument("--diff", action="store_true", help="show every differing line")
    run.add_argument("--assets", type=Path, default=default_assets())
    run.add_argument("--binary", type=Path, default=DEFAULT_BINARY)
    run.set_defaults(func=cmd_run)

    record = sub.add_parser("record", help="refresh the goldens from the original")
    record.add_argument("names", nargs="*")
    record.add_argument("-j", "--jobs", type=int, default=6)
    record.add_argument("--timeout", type=float, default=120.0)
    record.set_defaults(func=cmd_record)

    args = parser.parse_args(sys.argv[1:] or ["run"])
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
