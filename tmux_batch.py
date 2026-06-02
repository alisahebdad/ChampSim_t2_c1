#!/usr/bin/env python3
"""
For every (binary, input file) pair: create a dedicated directory under the
output dir, copy the binary into it, then run the binary against that input --
each in its own isolated, detached tmux session whose working directory is that
dedicated folder. The input file is NOT copied; it's referenced in place.

Layout produced:

    <output>/<binary>/<file>/
        |-- <binary>    (copied in)
        '-- (whatever the binary writes lands here, isolated)

Examples
--------
    # One binary, files in ./inputs, workspace under ./out
    python3 run_tmux_batch.py ./inputs -o ./out -b ./my_binary

    # Several binaries
    python3 run_tmux_batch.py ./inputs -o ./out -b ./bin_a -b ./bin_b

    # Only .txt files, keep each session open after it finishes
    python3 run_tmux_batch.py ./inputs -o ./out -b ./my_binary --ext .txt --keep-alive

    # Extra args; {file} marks where the input path goes (else appended)
    python3 run_tmux_batch.py ./inputs -o ./out -b ./my_binary --args "--verbose {file}"

    # Preview without copying or launching anything
    python3 run_tmux_batch.py ./inputs -o ./out -b ./my_binary --dry-run
"""

import argparse
import re
import shlex
import shutil
import stat
import subprocess
import sys
from pathlib import Path


def sanitize(name: str) -> str:
    """tmux session names can't contain '.' or ':' and shouldn't have spaces."""
    return re.sub(r"[^A-Za-z0-9_-]", "_", name)


def make_executable(path: Path) -> None:
    mode = path.stat().st_mode
    path.chmod(mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


def build_command(work_dir: Path, binary_name: str, input_path: Path,
                  args_template: str) -> str:
    """
    cd into the per-file work dir and run the local binary copy against the
    input's absolute path (the input itself is not copied in).
    """
    cd_part = f"cd {shlex.quote(str(work_dir))}"
    invocation = f"./{shlex.quote(binary_name)}"
    quoted_input = shlex.quote(str(input_path))

    if args_template:
        if "{file}" in args_template:
            run_part = f"{invocation} {args_template.replace('{file}', quoted_input)}"
        else:
            run_part = f"{invocation} {args_template} {quoted_input}"
    else:
        run_part = f"{invocation} {quoted_input}"

    return f"{cd_part} && {run_part}"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Create one isolated directory per (binary, input file), "
        "copy the binary in, and run it against that input in its own detached "
        "tmux session."
    )
    parser.add_argument("directory", help="Directory containing the input files.")
    parser.add_argument(
        "-o", "--output", required=True, help="Output directory (created if needed)."
    )
    parser.add_argument(
        "-b", "--binary", action="append", required=True, dest="binaries",
        help="Binary to run. Repeatable: -b ./bin_a -b ./bin_b",
    )
    parser.add_argument(
        "--ext", action="append", default=None,
        help="Only process files with this extension (e.g. --ext .txt). Repeatable.",
    )
    parser.add_argument(
        "--recursive", action="store_true",
        help="Recurse into subdirectories when collecting input files.",
    )
    parser.add_argument(
        "--args", default="",
        help="Extra args for the binary. Use {file} for the input path; if "
        "omitted the input is appended at the end.",
    )
    parser.add_argument(
        "--prefix", default="job", help="Prefix for tmux session names (default: job).",
    )
    parser.add_argument(
        "--keep-alive", action="store_true",
        help="Keep each session open after its command finishes.",
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Print what would happen without copying or launching anything.",
    )
    args = parser.parse_args()

    if not shutil.which("tmux") and not args.dry_run:
        print("error: tmux is not installed or not on PATH.", file=sys.stderr)
        return 1

    in_dir = Path(args.directory).expanduser()
    if not in_dir.is_dir():
        print(f"error: {in_dir} is not a directory.", file=sys.stderr)
        return 1

    out_dir = Path(args.output).expanduser().resolve()

    binaries = []
    for b in args.binaries:
        p = Path(b).expanduser()
        if not p.is_file():
            print(f"error: binary not found: {p}", file=sys.stderr)
            return 1
        binaries.append(p.resolve())

    walker = in_dir.rglob("*") if args.recursive else in_dir.iterdir()
    files = sorted(p.resolve() for p in walker if p.is_file())

    if args.ext:
        wanted = {e if e.startswith(".") else f".{e}" for e in args.ext}
        files = [p for p in files if p.suffix in wanted]

    if not files:
        print("No matching input files found.", file=sys.stderr)
        return 1

    if not args.dry_run:
        out_dir.mkdir(parents=True, exist_ok=True)

    launched = 0
    used_names: set[str] = set()
    used_dirs: set[Path] = set()

    for binary in binaries:
        for idx, file_path in enumerate(files):
            # Per-(binary, file) isolated work directory. De-dup on collisions
            # (e.g. two inputs whose stems sanitize to the same name).
            work_dir = out_dir / sanitize(binary.stem) / sanitize(file_path.stem)
            n = 1
            while work_dir in used_dirs:
                work_dir = (out_dir / sanitize(binary.stem)
                            / f"{sanitize(file_path.stem)}_{n}")
                n += 1
            used_dirs.add(work_dir)

            dest_binary = work_dir / binary.name

            # Unique tmux session name.
            base = sanitize(f"{args.prefix}_{binary.stem}_{idx:03d}_{file_path.stem}")
            name, suffix = base, 1
            while name in used_names:
                name, suffix = f"{base}_{suffix}", suffix + 1
            used_names.add(name)

            command = build_command(work_dir, binary.name, file_path, args.args)
            if args.keep_alive:
                command = f"{command}; echo; echo '[done -- press enter]'; read"

            if args.dry_run:
                print(f"[dry-run] mkdir -p {work_dir}")
                print(f"[dry-run] cp {binary} -> {dest_binary}")
                print(f"[dry-run] {name}: {command}")
                launched += 1
                continue

            # Set up the isolated directory: only the binary is copied in.
            work_dir.mkdir(parents=True, exist_ok=True)
            shutil.copy2(binary, dest_binary)
            make_executable(dest_binary)

            tmux_cmd = ["tmux", "new-session", "-d", "-s", name, command]
            try:
                subprocess.run(tmux_cmd, check=True)
                print(f"started '{name}'  ->  {work_dir}")
                launched += 1
            except subprocess.CalledProcessError as exc:
                print(f"failed for {file_path}: {exc}", file=sys.stderr)

    print(f"\n{launched} session(s) {'planned' if args.dry_run else 'started'} "
          f"({len(binaries)} binary/binaries x {len(files)} file(s)).")
    if launched and not args.dry_run:
        print("List them with:   tmux ls")
        print("Attach with:      tmux attach -t <session-name>")
    return 0


if __name__ == "__main__":
    sys.exit(main())
