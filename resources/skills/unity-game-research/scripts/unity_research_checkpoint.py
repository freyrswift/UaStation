#!/usr/bin/env python3
"""
Build checkpoint artifacts for Unity game research.

This script keeps extraction lightweight and deterministic so long-running
research can resume from disk instead of rebuilding context every session.
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import stat
import subprocess
from collections import Counter, defaultdict
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


TOPIC_KEYWORDS = {
    "economy": ["gold", "coin", "gem", "crystal", "currency", "shop", "price", "reward"],
    "progression": ["level", "exp", "xp", "rank", "upgrade", "ascend", "enhance", "evolve"],
    "combat": ["battle", "combat", "attack", "damage", "skill", "buff", "debuff", "target"],
    "skill-system": ["skill", "cooldown", "passive", "active", "ultimate", "talent", "ability"],
    "hero-movement-on-battlefield": [
        "move",
        "movement",
        "path",
        "pathfind",
        "navmesh",
        "grid",
        "dash",
        "position",
    ],
    "inventory": ["inventory", "item", "equipment", "equip", "bag", "slot", "rarity"],
    "quests": ["quest", "mission", "task", "objective", "chapter", "stage"],
    "guild-system": ["guild", "clan", "alliance", "party", "friend", "social"],
    "monetization": ["iap", "purchase", "offer", "battle pass", "subscription", "vip", "ad"],
    "networking": ["login", "auth", "server", "socket", "match", "session", "packet"],
}

TEXT_EXTENSIONS = {
    ".txt",
    ".json",
    ".xml",
    ".yaml",
    ".yml",
    ".csv",
    ".ini",
    ".cfg",
    ".bytes",
    ".lua",
    ".js",
    ".ts",
    ".proto",
}

BINARY_STRING_EXTENSIONS = {
    ".dll",
    ".so",
    ".dat",
    ".assets",
    ".bundle",
    ".ress",
    ".resource",
    ".unity3d",
    ".bin",
}

ENDPOINT_PATTERN = re.compile(
    r"(?i)\b(?:https?://[^\s\"'<>]+|wss?://[^\s\"'<>]+|[A-Za-z0-9._-]+\.(?:com|net|io|gg|cn|jp|kr|ru|dev|games|app|api)(?:/[^\s\"'<>]*)?)"
)
PRINTABLE_RE = re.compile(rb"[\x20-\x7e]{6,}")
IDENTIFIER_RE = re.compile(r"\b(?:[A-Za-z_][A-Za-z0-9_]*\.){0,4}[A-Z][A-Za-z0-9_]{2,}\b")
SYMBOL_RE = re.compile(r"\b(?:[A-Za-z_][A-Za-z0-9_]*::)?[A-Za-z_][A-Za-z0-9_]{2,}\b")
TOOL_ENV_OVERRIDES = {
    "apktool": "UNITY_RESEARCH_APKTOOL",
    "jadx": "UNITY_RESEARCH_JADX",
    "adb": "UNITY_RESEARCH_ADB",
    "ghidra": "UNITY_RESEARCH_GHIDRA",
    "assetripper": "UNITY_RESEARCH_ASSETRIPPER",
    "ilspycmd": "UNITY_RESEARCH_ILSPYCMD",
    "monodis": "UNITY_RESEARCH_MONODIS",
    "cpp2il": "UNITY_RESEARCH_CPP2IL",
    "il2cppdumper": "UNITY_RESEARCH_IL2CPP_DUMPER",
    "readelf": "UNITY_RESEARCH_READELF",
    "nm": "UNITY_RESEARCH_NM",
    "objdump": "UNITY_RESEARCH_OBJDUMP",
    "file": "UNITY_RESEARCH_FILE",
    "strings": "UNITY_RESEARCH_STRINGS",
}
TOOL_CANDIDATES = {
    "apktool": ["apktool", "apktool.jar"],
    "jadx": ["jadx", "jadx-cli", "jadx.bat"],
    "adb": ["adb", "adb.exe"],
    "ghidra": ["ghidra", "analyzeHeadless", "ghidraRun", "ghidraRun.bat"],
    "assetripper": ["AssetRipper", "AssetRipper.Console", "AssetRipper.CLI", "AssetRipper.exe"],
    "ilspycmd": ["ilspycmd", "ILSpyCmd", "ilspycmd.exe", "ILSpyCmd.exe"],
    "monodis": ["monodis"],
    "cpp2il": ["cpp2il", "Cpp2IL", "cpp2il.exe", "Cpp2IL.exe"],
    "il2cppdumper": ["il2cppdumper", "Il2CppDumper", "il2cppdumper.exe", "Il2CppDumper.exe"],
    "readelf": ["readelf"],
    "nm": ["nm"],
    "objdump": ["objdump"],
    "file": ["file"],
    "strings": ["strings"],
}
COMMON_TOOL_DIRS = [
    "",
    "tools",
    "_tools",
    "bin",
    "Tools",
    "reverse-tools",
    "ReverseTools",
    "UnityTools",
    "unity-tools",
]
REQUIRED_TOOL_GROUPS = {
    "android-unpack": ["apktool", "jadx"],
    "unity-managed": ["ilspycmd"],
    "unity-assets": ["assetripper"],
    "unity-il2cpp": ["cpp2il", "il2cppdumper"],
    "native-re": ["ghidra"],
    "device-runtime": ["adb"],
}
PACKAGE_MANAGERS = ["apt", "apt-get", "dnf", "yum", "pacman", "snap", "flatpak"]
APT_PACKAGES = {
    "apktool": "apktool",
    "jadx": "jadx",
    "adb": "adb",
    "ghidra": "ghidra",
}
ABI_DIR_HINTS = ("arm64-v8a", "armeabi-v7a", "x86", "x86_64")


@dataclass
class ScanResult:
    unity_confirmed: bool
    backend: str
    unity_markers: list[str]
    files_tree: list[str]
    managed_assemblies: list[str]
    native_libs: list[str]
    asset_files: list[str]
    metadata_files: list[str]
    cache_candidates: list[str]
    topic_hits: dict[str, list[str]]
    endpoint_hits: list[str]
    tool_availability: dict[str, str]
    managed_identifiers: dict[str, list[str]]
    native_symbols: dict[str, list[str]]
    il2cpp_targets: dict[str, str]
    recovery_plan: list[str]
    readiness_lines: list[str]
    install_plan_lines: list[str]
    os_info: dict[str, str]
    package_manager_paths: dict[str, str]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Build Unity game research checkpoints.")
    parser.add_argument("input_root", nargs="?", default=".", help="Path to unpacked game root")
    parser.add_argument(
        "--output-root",
        default=None,
        help="Output folder. Defaults to <input_root>/game-knowledge",
    )
    parser.add_argument(
        "--refresh-raw",
        action="store_true",
        help="Regenerate raw string/topic artifacts even if they already exist",
    )
    parser.add_argument(
        "--max-file-size-mb",
        type=int,
        default=32,
        help="Skip raw string extraction for files larger than this many MB",
    )
    return parser.parse_args()


def detect_tool_path(tool_name: str, search_roots: list[Path] | None = None) -> str | None:
    override = os.environ.get(TOOL_ENV_OVERRIDES.get(tool_name, ""), "").strip()
    if override:
        return override

    for candidate in TOOL_CANDIDATES.get(tool_name, [tool_name]):
        found = shutil.which(candidate)
        if found:
            return found

    roots = search_roots or []
    extra_roots = [
        Path.home() / ".local" / "bin",
        Path.home() / "bin",
        Path.home() / "tools",
        Path.home() / "Tools",
    ]
    for root in [*roots, *extra_roots]:
        for subdir in COMMON_TOOL_DIRS:
            base = root / subdir if subdir else root
            for candidate in TOOL_CANDIDATES.get(tool_name, [tool_name]):
                path = base / candidate
                if path.is_file():
                    return str(path)
    return None


def run_tool(command: list[str]) -> str:
    try:
        proc = subprocess.run(
            command,
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            encoding="utf-8",
            errors="ignore",
        )
    except OSError:
        return ""
    if proc.returncode != 0:
        return ""
    return proc.stdout


def read_os_release() -> dict[str, str]:
    info: dict[str, str] = {}
    path = Path("/etc/os-release")
    if not path.exists():
        return info
    for line in path.read_text(errors="ignore").splitlines():
        if "=" not in line:
            continue
        key, value = line.split("=", 1)
        info[key] = value.strip().strip('"')
    return info


def detect_package_managers() -> dict[str, str]:
    result: dict[str, str] = {}
    for name in PACKAGE_MANAGERS:
        result[name] = shutil.which(name) or ""
    return result


def slugify(value: str) -> str:
    slug = re.sub(r"[^a-z0-9]+", "-", value.lower()).strip("-")
    return slug or "topic"


def relpath(path: Path, root: Path) -> str:
    return path.relative_to(root).as_posix()


def _normalized_rel(rel: str) -> str:
    return rel.replace("\\", "/")


def _path_parts_lower(rel: str) -> list[str]:
    return [part.lower() for part in Path(_normalized_rel(rel)).parts]


def _has_lib_abi_layout(rel: str) -> bool:
    parts = _path_parts_lower(rel)
    for index in range(len(parts) - 1):
        if parts[index] == "lib" and parts[index + 1] in ABI_DIR_HINTS:
            return True
    return False


def _has_unity_metadata_layout(rel: str) -> bool:
    parts = _path_parts_lower(rel)
    if parts[-1:] != ["global-metadata.dat"]:
        return False
    for index, part in enumerate(parts[:-1]):
        if part != "data":
            continue
        tail = parts[index + 1 : -1]
        for tail_index, tail_part in enumerate(tail):
            if tail_part in {"managed", "il2cpp_data"} and "metadata" in tail[tail_index + 1 :]:
                return True
    return False


def is_managed_game_assembly(rel: str) -> bool:
    normalized = _normalized_rel(rel)
    path = Path(normalized)
    return (
        path.suffix.lower() == ".dll"
        and any(part.lower() == "managed" for part in path.parts)
        and "assembly-csharp" in path.name.lower()
    )


def is_libil2cpp_path(rel: str) -> bool:
    path = Path(_normalized_rel(rel))
    return path.name.lower() == "libil2cpp.so" and _has_lib_abi_layout(rel)


def is_libunity_path(rel: str) -> bool:
    path = Path(_normalized_rel(rel))
    return path.name.lower() == "libunity.so" and _has_lib_abi_layout(rel)


def is_metadata_path(rel: str) -> bool:
    return _has_unity_metadata_layout(rel)


def is_native_library_path(rel: str) -> bool:
    normalized = _normalized_rel(rel)
    path = Path(normalized)
    if path.suffix.lower() != ".so":
        return False
    return is_libil2cpp_path(normalized) or is_libunity_path(normalized)


def is_interesting_file(path: Path) -> bool:
    name = path.name.lower()
    suffix = path.suffix.lower()
    if suffix in TEXT_EXTENSIONS or suffix in BINARY_STRING_EXTENSIONS:
        return True
    return any(
        token in name
        for token in (
            "globalgamemanagers",
            "resources.assets",
            "sharedassets",
            "metadata",
            "assembly-csharp",
            "libil2cpp",
            "libunity",
        )
    )


def extract_ascii_strings(path: Path, max_file_size: int) -> list[str]:
    try:
        if path.stat().st_size > max_file_size:
            return []
        data = path.read_bytes()
    except OSError:
        return []

    strings = [match.decode("utf-8", errors="ignore").strip() for match in PRINTABLE_RE.findall(data)]
    unique: list[str] = []
    seen = set()
    for item in strings:
        if len(item) < 6 or item in seen:
            continue
        seen.add(item)
        unique.append(item)
        if len(unique) >= 400:
            break
    return unique


def collect_text_lines(path: Path, max_file_size: int) -> list[str]:
    try:
        if path.stat().st_size > max_file_size:
            return []
        text = path.read_text(errors="ignore")
    except OSError:
        return []
    lines = []
    for raw_line in text.splitlines():
        line = raw_line.strip()
        if len(line) < 4:
            continue
        lines.append(line)
        if len(lines) >= 400:
            break
    return lines


def collect_tool_availability(input_root: Path) -> dict[str, str]:
    search_roots = [
        input_root,
        input_root.parent,
        Path.cwd(),
    ]
    tools = {}
    for tool_name in TOOL_ENV_OVERRIDES:
        tools[tool_name] = detect_tool_path(tool_name, search_roots=search_roots) or ""
    return tools


def choose_il2cpp_targets(native_libs: list[str], metadata_files: list[str]) -> dict[str, str]:
    libil2cpp_candidates = [rel for rel in native_libs if rel.endswith("/libil2cpp.so")]
    preferred_binary = ""
    if libil2cpp_candidates:
        arm64 = [rel for rel in libil2cpp_candidates if "/arm64-v8a/" in rel]
        preferred_binary = (arm64 or libil2cpp_candidates)[0]

    preferred_metadata = ""
    if metadata_files:
        exact = [rel for rel in metadata_files if rel.endswith("global-metadata.dat")]
        preferred_metadata = (exact or metadata_files)[0]

    return {
        "binary": preferred_binary,
        "metadata": preferred_metadata,
    }


def build_recovery_plan(
    input_root: Path,
    output_root: Path,
    tool_availability: dict[str, str],
    il2cpp_targets: dict[str, str],
) -> list[str]:
    lines = ["# Recovery Plan", ""]

    binary = il2cpp_targets.get("binary", "")
    metadata = il2cpp_targets.get("metadata", "")

    if tool_availability.get("cpp2il"):
        lines.append("## Cpp2IL")
        lines.append("")
        if binary and metadata:
            command = (
                f"{tool_availability['cpp2il']} "
                f"--game-path {input_root} "
                f"--exe-name {Path(binary).name} "
                f"--output-root {output_root / 'raw' / 'cpp2il'}"
            )
            lines.append(f"- Detected: `{tool_availability['cpp2il']}`")
            lines.append(f"- Candidate command: `{command}`")
            lines.append(f"- Binary: `{binary}`")
            lines.append(f"- Metadata: `{metadata}`")
        else:
            lines.append("- Detected, but `libil2cpp.so` or `global-metadata.dat` is missing.")
        lines.append("")

    if tool_availability.get("il2cppdumper"):
        lines.append("## Il2CppDumper")
        lines.append("")
        if binary and metadata:
            command = (
                f"{tool_availability['il2cppdumper']} "
                f"{input_root / binary} {input_root / metadata} {output_root / 'raw' / 'il2cppdumper'}"
            )
            lines.append(f"- Detected: `{tool_availability['il2cppdumper']}`")
            lines.append(f"- Candidate command: `{command}`")
            lines.append(f"- Binary: `{binary}`")
            lines.append(f"- Metadata: `{metadata}`")
        else:
            lines.append("- Detected, but `libil2cpp.so` or `global-metadata.dat` is missing.")
        lines.append("")

    if not tool_availability.get("cpp2il") and not tool_availability.get("il2cppdumper"):
        lines.append("## IL2CPP Tools")
        lines.append("")
        lines.append("- No `Cpp2IL` or `Il2CppDumper` binary auto-detected.")
        if binary:
            lines.append(f"- Preferred binary: `{binary}`")
        if metadata:
            lines.append(f"- Preferred metadata: `{metadata}`")
        lines.append("")

    return lines


def build_tool_readiness(tool_availability: dict[str, str]) -> list[str]:
    lines = ["# Tool Readiness", ""]
    for group, tools in REQUIRED_TOOL_GROUPS.items():
        lines.append(f"## {group}")
        lines.append("")
        available = [tool for tool in tools if tool_availability.get(tool)]
        status = "ready" if available else "missing"
        lines.append(f"- Status: `{status}`")
        lines.append(f"- Expected tools: `{', '.join(tools)}`")
        if available:
            lines.append(f"- Detected: `{', '.join(available)}`")
        else:
            lines.append("- Detected: `<none>`")
        lines.append("")

    lines.append("## All Detected Tools")
    lines.append("")
    for tool, path in sorted(tool_availability.items()):
        lines.append(f"- `{tool}`: `{path or 'missing'}`")
    return lines


def build_install_plan(
    tool_availability: dict[str, str],
    package_manager_paths: dict[str, str],
    os_info: dict[str, str],
) -> list[str]:
    lines = ["# Install Plan", ""]
    missing_tools = [tool for tool, path in sorted(tool_availability.items()) if not path and tool in TOOL_ENV_OVERRIDES]
    os_name = os_info.get("PRETTY_NAME") or os_info.get("NAME") or "unknown"
    lines.append(f"- OS: `{os_name}`")

    pm_name = next((name for name in ("apt", "apt-get", "dnf", "yum", "pacman", "snap", "flatpak") if package_manager_paths.get(name)), "")
    lines.append(f"- Preferred package manager: `{pm_name or 'none detected'}`")
    lines.append("")

    if not missing_tools:
        lines.append("All tracked tools are already present.")
        return lines

    lines.append("## Missing Tools")
    lines.append("")
    for tool in missing_tools:
        lines.append(f"- `{tool}`")
    lines.append("")

    if package_manager_paths.get("apt") or package_manager_paths.get("apt-get"):
        apt_cmd = package_manager_paths.get("apt") or package_manager_paths.get("apt-get") or "apt"
        apt_targets = [APT_PACKAGES[tool] for tool in missing_tools if tool in APT_PACKAGES]
        if apt_targets:
            deduped = " ".join(sorted(set(apt_targets)))
            lines.append("## APT")
            lines.append("")
            lines.append(f"- Candidate command: `sudo {apt_cmd} update`")
            lines.append(f"- Candidate command: `sudo {apt_cmd} install -y {deduped}`")
            lines.append("")

    direct_downloads = []
    for tool in missing_tools:
        if tool in {"ilspycmd", "assetripper", "cpp2il", "il2cppdumper"}:
            direct_downloads.append(tool)
    if direct_downloads:
        lines.append("## Manual Binaries")
        lines.append("")
        lines.append("- Download release binaries for these tools and place them in one of:")
        lines.append("  `./tools`, `../tools`, `~/tools`, `~/.local/bin`")
        lines.append("- Or set the matching environment variable override from `references/tool-adapters.md`.")
        for tool in direct_downloads:
            lines.append(f"- Tool: `{tool}`")
        lines.append("")

    if "jadx" in missing_tools:
        lines.append("## JADX")
        lines.append("")
        lines.append("- If `jadx` is unavailable from the package manager, install a release archive and expose `jadx/bin/jadx` on `PATH`.")
        lines.append("")

    if "ghidra" in missing_tools:
        lines.append("## Ghidra")
        lines.append("")
        lines.append("- Ghidra is commonly installed from a release archive rather than the system package manager.")
        lines.append("- Expose `analyzeHeadless` or `ghidraRun` via `PATH` or `UNITY_RESEARCH_GHIDRA`.")
        lines.append("")

    if "adb" in missing_tools:
        lines.append("## ADB")
        lines.append("")
        lines.append("- If `adb` is unavailable from the package manager, install Android Platform Tools and expose `adb` on `PATH`.")
        lines.append("")

    lines.append("## Policy")
    lines.append("")
    lines.append("- This install plan is optional guidance only.")
    lines.append("- Installation requires explicit user approval.")
    lines.append("- Continue with available fallbacks when installation is not approved.")
    lines.append("- After installation, rerun the checkpoint script with `--refresh-raw` to refresh readiness and recovery plans.")
    return lines


def extract_managed_identifiers(
    dlls: list[Path], root: Path, max_file_size: int, tool_availability: dict[str, str]
) -> dict[str, list[str]]:
    results: dict[str, list[str]] = {}
    ilspycmd = tool_availability.get("ilspycmd")
    monodis = tool_availability.get("monodis")

    for dll in dlls:
        identifiers: list[str] = []
        seen = set()

        if ilspycmd:
            output = run_tool([ilspycmd, str(dll), "--ilcode"])
            for match in IDENTIFIER_RE.findall(output):
                if match not in seen:
                    seen.add(match)
                    identifiers.append(match)
                if len(identifiers) >= 200:
                    break

        if not identifiers and monodis:
            output = run_tool([monodis, str(dll)])
            for match in IDENTIFIER_RE.findall(output):
                if match not in seen:
                    seen.add(match)
                    identifiers.append(match)
                if len(identifiers) >= 200:
                    break

        if not identifiers:
            for line in extract_ascii_strings(dll, max_file_size):
                for match in IDENTIFIER_RE.findall(line):
                    if match not in seen:
                        seen.add(match)
                        identifiers.append(match)
                    if len(identifiers) >= 200:
                        break
                if len(identifiers) >= 200:
                    break

        results[relpath(dll, root)] = identifiers[:200]
    return results


def extract_native_symbols(libs: list[Path], root: Path, tool_availability: dict[str, str]) -> dict[str, list[str]]:
    results: dict[str, list[str]] = {}
    readelf = tool_availability.get("readelf")
    nm = tool_availability.get("nm")

    for lib in libs:
        symbols: list[str] = []
        seen = set()

        if readelf:
            output = run_tool([readelf, "-Ws", str(lib)])
            for match in SYMBOL_RE.findall(output):
                if match.startswith(("GLIBC", "_ITM", "__cxa", "__gnu")):
                    continue
                if match not in seen:
                    seen.add(match)
                    symbols.append(match)
                if len(symbols) >= 300:
                    break

        if len(symbols) < 50 and nm:
            output = run_tool([nm, "-D", "--defined-only", str(lib)])
            for line in output.splitlines():
                parts = line.split()
                if not parts:
                    continue
                candidate = parts[-1]
                if not SYMBOL_RE.fullmatch(candidate):
                    continue
                if candidate.startswith(("GLIBC", "_ITM", "__cxa", "__gnu")):
                    continue
                if candidate not in seen:
                    seen.add(candidate)
                    symbols.append(candidate)
                if len(symbols) >= 300:
                    break

        results[relpath(lib, root)] = symbols[:300]
    return results


def find_unity_markers(relative_files: set[str]) -> tuple[bool, list[str]]:
    markers = []
    wanted = [
        "assets/bin/Data",
        "assets/bin/Data/Managed",
        "assets/bin/Data/il2cpp_data",
        "assets/bin/Data/il2cpp_data/Metadata/global-metadata.dat",
        "assets/bin/Data/globalgamemanagers",
        "assets/bin/Data/resources.assets",
    ]
    for item in wanted:
        if item in relative_files or any(path.startswith(f"{item}/") for path in relative_files):
            markers.append(item)

    for rel in sorted(relative_files):
        if is_libunity_path(rel):
            markers.append(rel)
        if is_libil2cpp_path(rel):
            markers.append(rel)
        if "/sharedassets" in rel or rel.endswith(".unity3d"):
            markers.append(rel)

    deduped = sorted(set(markers))
    return (len(deduped) > 0, deduped)


def discover_cache_candidates(relative_files: set[str]) -> list[str]:
    candidates: list[str] = []
    for rel in sorted(relative_files):
        lowered = _normalized_rel(rel).lower()
        if "unitycache/shared" in lowered:
            candidates.append(rel)
            continue
        if lowered.endswith("/__data") or lowered.endswith("__data"):
            if any(token in lowered for token in ("/cache/files/", "/cache/bundlefiles/", "/bundlefiles/", "/assetbundle/", "/assetbundles/")):
                candidates.append(rel)
                continue
        if lowered.endswith(".bundle") and any(token in lowered for token in ("/bundlefiles/", "/cache/", "/assetbundle/", "/assetbundles/")):
            candidates.append(rel)
            continue
    return sorted(set(candidates))


def classify_backend(relative_files: set[str]) -> str:
    has_managed_game = any(is_managed_game_assembly(rel) for rel in relative_files)
    has_metadata = any(is_metadata_path(rel) for rel in relative_files)
    has_libil2cpp = any(is_libil2cpp_path(rel) for rel in relative_files)

    if has_managed_game and has_metadata and has_libil2cpp:
        return "mixed"
    if has_managed_game:
        return "managed"
    if has_metadata and has_libil2cpp:
        return "il2cpp"
    return "unknown"


def scan_tree(root: Path, max_file_size: int, refresh_raw: bool, output_root: Path) -> ScanResult:
    files_tree: list[str] = []
    relative_files: set[str] = set()
    managed_assemblies: list[str] = []
    native_libs: list[str] = []
    asset_files: list[str] = []
    metadata_files: list[str] = []
    topic_hits: dict[str, list[str]] = defaultdict(list)
    endpoint_hits: list[str] = []
    seen_topic_lines: dict[str, set[str]] = defaultdict(set)
    seen_endpoints: set[str] = set()
    tool_availability = collect_tool_availability(root)
    managed_identifiers: dict[str, list[str]] = {}
    native_symbols: dict[str, list[str]] = {}
    il2cpp_targets: dict[str, str] = {}
    recovery_plan: list[str] = []
    readiness_lines: list[str] = []
    install_plan_lines: list[str] = []
    os_info = read_os_release()
    package_manager_paths = detect_package_managers()

    raw_strings_path = output_root / "raw" / "strings-interesting.txt"
    reuse_strings = raw_strings_path.exists() and not refresh_raw

    for path in sorted(root.rglob("*")):
        try:
            st = path.lstat()
        except OSError:
            continue
        if not stat.S_ISREG(st.st_mode):
            continue
        if output_root in path.parents:
            continue

        rel = relpath(path, root)
        relative_files.add(rel)
        files_tree.append(rel)

        lower_name = path.name.lower()
        suffix = path.suffix.lower()

        if is_managed_game_assembly(rel):
            managed_assemblies.append(rel)
        if is_native_library_path(rel):
            native_libs.append(rel)
        if any(token in lower_name for token in ("resources.assets", "sharedassets", "globalgamemanagers")) or suffix in {
            ".assets",
            ".unity3d",
            ".bundle",
            ".ress",
        }:
            asset_files.append(rel)
        if is_metadata_path(rel):
            metadata_files.append(rel)

        if reuse_strings or not is_interesting_file(path):
            continue

        lines = collect_text_lines(path, max_file_size) if suffix in TEXT_EXTENSIONS else extract_ascii_strings(path, max_file_size)
        if not lines:
            continue

        for line in lines:
            for match in ENDPOINT_PATTERN.findall(line):
                if match not in seen_endpoints:
                    seen_endpoints.add(match)
                    endpoint_hits.append(match)
            lowered = line.lower()
            for topic, keywords in TOPIC_KEYWORDS.items():
                if any(keyword in lowered for keyword in keywords):
                    bucket = seen_topic_lines[topic]
                    if line not in bucket:
                        bucket.add(line)
                        topic_hits[topic].append(f"{rel}: {line}")
                    break

    unity_confirmed, unity_markers = find_unity_markers(relative_files)
    cache_candidates = discover_cache_candidates(relative_files)
    backend = classify_backend(relative_files)

    managed_paths = [root / rel for rel in managed_assemblies]
    native_paths = [root / rel for rel in native_libs]
    il2cpp_targets = choose_il2cpp_targets(native_libs, metadata_files)

    if reuse_strings:
        for topic in TOPIC_KEYWORDS:
            topic_file = output_root / "raw" / "topic-hits" / f"{topic}.txt"
            if topic_file.exists():
                topic_hits[topic] = [
                    line for line in topic_file.read_text(errors="ignore").splitlines() if line and line != "<none>"
                ]
        endpoints_file = output_root / "raw" / "endpoints.txt"
        if endpoints_file.exists():
            endpoint_hits = [
                line.strip() for line in endpoints_file.read_text(errors="ignore").splitlines() if line and line != "<none>"
            ]
        managed_identifiers = load_mapping(output_root / "raw" / "managed-identifiers.txt")
        native_symbols = load_mapping(output_root / "raw" / "native-symbols.txt")
        recovery_plan_path = output_root / "raw" / "recovery-plan.txt"
        if recovery_plan_path.exists():
            recovery_plan = recovery_plan_path.read_text(errors="ignore").splitlines()
        readiness_path = output_root / "indexes" / "tool-readiness.md"
        if readiness_path.exists():
            readiness_lines = readiness_path.read_text(errors="ignore").splitlines()
        install_path = output_root / "indexes" / "install-plan.md"
        if install_path.exists():
            install_plan_lines = install_path.read_text(errors="ignore").splitlines()
    else:
        managed_identifiers = extract_managed_identifiers(managed_paths, root, max_file_size, tool_availability)
        native_symbols = extract_native_symbols(native_paths, root, tool_availability)
        recovery_plan = build_recovery_plan(root, output_root, tool_availability, il2cpp_targets)
        readiness_lines = build_tool_readiness(tool_availability)
        install_plan_lines = build_install_plan(tool_availability, package_manager_paths, os_info)

    return ScanResult(
        unity_confirmed=unity_confirmed,
        backend=backend,
        unity_markers=unity_markers,
        files_tree=files_tree,
        managed_assemblies=sorted(managed_assemblies),
        native_libs=sorted(native_libs),
        asset_files=sorted(set(asset_files)),
        metadata_files=sorted(set(metadata_files)),
        cache_candidates=cache_candidates,
        topic_hits={topic: hits for topic, hits in sorted(topic_hits.items())},
        endpoint_hits=sorted(endpoint_hits),
        tool_availability=tool_availability,
        managed_identifiers=managed_identifiers,
        native_symbols=native_symbols,
        il2cpp_targets=il2cpp_targets,
        recovery_plan=recovery_plan,
        readiness_lines=readiness_lines,
        install_plan_lines=install_plan_lines,
        os_info=os_info,
        package_manager_paths=package_manager_paths,
    )


def ensure_dirs(output_root: Path) -> None:
    for rel in ("raw", "raw/topic-hits", "indexes", "evidence", "topics"):
        (output_root / rel).mkdir(parents=True, exist_ok=True)


def write_lines(path: Path, lines: Iterable[str]) -> None:
    path.write_text("\n".join(lines).rstrip() + "\n", encoding="utf-8")


def write_mapping(path: Path, mapping: dict[str, list[str]]) -> None:
    lines: list[str] = []
    for key, values in mapping.items():
        lines.append(f"## {key}")
        if values:
            lines.extend(values)
        else:
            lines.append("<none>")
        lines.append("")
    write_lines(path, lines or ["<none>"])


def load_mapping(path: Path) -> dict[str, list[str]]:
    if not path.exists():
        return {}
    mapping: dict[str, list[str]] = {}
    current_key = None
    current_values: list[str] = []
    for line in path.read_text(errors="ignore").splitlines():
        if line.startswith("## "):
            if current_key is not None:
                mapping[current_key] = [value for value in current_values if value and value != "<none>"]
            current_key = line[3:].strip()
            current_values = []
        elif current_key is not None and line.strip():
            current_values.append(line.strip())
    if current_key is not None:
        mapping[current_key] = [value for value in current_values if value and value != "<none>"]
    return mapping


def yaml_scalar(value) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None:
        return "null"
    if isinstance(value, (int, float)):
        return str(value)
    text = str(value)
    if text == "" or any(ch in text for ch in [":", "#", "{", "}", "[", "]"]) or text.strip() != text:
        escaped = text.replace('"', '\\"')
        return f'"{escaped}"'
    return text


def write_yaml(path: Path, data: dict) -> None:
    def render(value, indent=0):
        spaces = " " * indent
        if isinstance(value, dict):
            lines = []
            for key, inner in value.items():
                if isinstance(inner, (dict, list)):
                    lines.append(f"{spaces}{key}:")
                    lines.extend(render(inner, indent + 2))
                else:
                    lines.append(f"{spaces}{key}: {yaml_scalar(inner)}")
            return lines
        if isinstance(value, list):
            lines = []
            for item in value:
                if isinstance(item, (dict, list)):
                    lines.append(f"{spaces}-")
                    lines.extend(render(item, indent + 2))
                else:
                    lines.append(f"{spaces}- {yaml_scalar(item)}")
            return lines
        return [f"{spaces}{yaml_scalar(value)}"]

    path.write_text("\n".join(render(data)).rstrip() + "\n", encoding="utf-8")


def build_manifest(root: Path, output_root: Path, result: ScanResult, topic_counter: Counter) -> dict:
    return {
        "input_root": str(root.resolve()),
        "output_root": str(output_root.resolve()),
        "unity_confirmed": result.unity_confirmed,
        "backend": result.backend,
        "os": result.os_info.get("PRETTY_NAME", ""),
        "package_managers": {name: bool(path) for name, path in result.package_manager_paths.items()},
        "tools": {name: bool(path) for name, path in result.tool_availability.items()},
        "tool_groups": {
            group: any(result.tool_availability.get(tool) for tool in tools) for group, tools in REQUIRED_TOOL_GROUPS.items()
        },
        "unity_markers": result.unity_markers,
        "il2cpp_targets": result.il2cpp_targets,
        "managed_assemblies_count": len(result.managed_assemblies),
        "native_libs_count": len(result.native_libs),
        "asset_files_count": len(result.asset_files),
        "cache_candidates_count": len(result.cache_candidates),
        "managed_identifier_sources": len([key for key, values in result.managed_identifiers.items() if values]),
        "native_symbol_sources": len([key for key, values in result.native_symbols.items() if values]),
        "top_topics": [f"{topic}:{count}" for topic, count in topic_counter.most_common(8)],
    }


def build_progress(root: Path, output_root: Path, result: ScanResult, topic_counter: Counter) -> dict:
    topics = {}
    for topic, count in topic_counter.most_common():
        topics[topic] = {
            "status": "indexed",
            "doc": f"topics/{slugify(topic)}.md",
            "hits": count,
        }
    return {
        "input_root": str(root.resolve()),
        "output_root": str(output_root.resolve()),
        "backend": result.backend,
        "unity_confirmed": result.unity_confirmed,
        "phases": {
            "detect": "done",
            "extract": "done",
            "index": "done",
            "infer": "pending",
            "write": "pending",
        },
        "topics": topics,
    }


def write_raw_outputs(output_root: Path, result: ScanResult) -> None:
    write_lines(output_root / "raw" / "files-tree.txt", result.files_tree)
    write_lines(output_root / "raw" / "unity-markers.txt", result.unity_markers or ["<none>"])
    write_lines(output_root / "raw" / "managed-assemblies.txt", result.managed_assemblies or ["<none>"])
    write_lines(output_root / "raw" / "native-libs.txt", result.native_libs or ["<none>"])
    write_lines(output_root / "raw" / "asset-files.txt", result.asset_files or ["<none>"])
    write_lines(output_root / "raw" / "metadata-files.txt", result.metadata_files or ["<none>"])
    write_lines(output_root / "raw" / "cache-candidates.txt", result.cache_candidates or ["<none>"])
    write_lines(output_root / "raw" / "endpoints.txt", result.endpoint_hits or ["<none>"])
    write_lines(
        output_root / "raw" / "tool-availability.txt",
        [f"{name}={path or '<missing>'}" for name, path in sorted(result.tool_availability.items())],
    )
    write_mapping(output_root / "raw" / "managed-identifiers.txt", result.managed_identifiers)
    write_mapping(output_root / "raw" / "native-symbols.txt", result.native_symbols)
    write_lines(output_root / "raw" / "recovery-plan.txt", result.recovery_plan or ["<none>"])

    string_lines = []
    for topic, hits in result.topic_hits.items():
        string_lines.append(f"## {topic}")
        if hits:
            string_lines.extend(hits)
        else:
            string_lines.append("<none>")
        string_lines.append("")
        write_lines(output_root / "raw" / "topic-hits" / f"{topic}.txt", hits or ["<none>"])
    write_lines(output_root / "raw" / "strings-interesting.txt", string_lines or ["<none>"])


def write_files_inventory(output_root: Path, result: ScanResult) -> None:
    lines = [
        "# Files Inventory",
        "",
        f"- Unity confirmed: `{str(result.unity_confirmed).lower()}`",
        f"- Backend: `{result.backend}`",
        f"- Managed assemblies: `{len(result.managed_assemblies)}`",
        f"- Native libs: `{len(result.native_libs)}`",
        f"- Asset files: `{len(result.asset_files)}`",
        "",
        "## Tool Availability",
        "",
    ]
    for name, path in sorted(result.tool_availability.items()):
        lines.append(f"- `{name}`: `{path or 'missing'}`")

    lines.extend([
        "",
        "## Unity Markers",
        "",
    ])
    for marker in result.unity_markers or ["<none>"]:
        lines.append(f"- `{marker}`")

    lines.extend(["", "## Notable Files", ""])
    notable = result.managed_assemblies[:20] + result.native_libs[:20] + result.asset_files[:20]
    for rel in notable or ["<none>"]:
        lines.append(f"- `{rel}`")
    lines.extend(["", "## Cache-Like Bundle Candidates", ""])
    for rel in result.cache_candidates or ["<none>"]:
        lines.append(f"- `{rel}`")
    write_lines(output_root / "evidence" / "files-inventory.md", lines)


def write_indexes(output_root: Path, result: ScanResult) -> Counter:
    lines = [
        "# Assemblies Index",
        "",
        "## Managed Assemblies",
        "",
    ]
    for rel in result.managed_assemblies or ["<none>"]:
        lines.append(f"- `{rel}`")
    lines.extend(["", "## Native Libraries", ""])
    for rel in result.native_libs or ["<none>"]:
        lines.append(f"- `{rel}`")
    lines.extend(["", "## Metadata Files", ""])
    for rel in result.metadata_files or ["<none>"]:
        lines.append(f"- `{rel}`")
    write_lines(output_root / "indexes" / "assemblies-index.md", lines)

    type_lines = ["# Types Index", ""]
    for rel, identifiers in sorted(result.managed_identifiers.items()):
        type_lines.append(f"## {rel}")
        type_lines.append("")
        for item in identifiers[:80] or ["<none>"]:
            type_lines.append(f"- `{item}`")
        type_lines.append("")
    write_lines(output_root / "indexes" / "types-index.md", type_lines or ["# Types Index", "", "<none>"])

    asset_lines = ["# Assets Index", ""]
    for rel in result.asset_files or ["<none>"]:
        asset_lines.append(f"- `{rel}`")
    asset_lines.extend(["", "## Cache-Like Bundle Candidates", ""])
    for rel in result.cache_candidates or ["<none>"]:
        asset_lines.append(f"- `{rel}`")
    write_lines(output_root / "indexes" / "assets-index.md", asset_lines)

    symbol_lines = ["# Native Symbols", ""]
    for rel, symbols in sorted(result.native_symbols.items()):
        symbol_lines.append(f"## {rel}")
        symbol_lines.append("")
        for item in symbols[:120] or ["<none>"]:
            symbol_lines.append(f"- `{item}`")
        symbol_lines.append("")
    write_lines(output_root / "indexes" / "native-symbols.md", symbol_lines or ["# Native Symbols", "", "<none>"])

    endpoint_lines = ["# Network Endpoints", ""]
    for item in result.endpoint_hits or ["<none>"]:
        endpoint_lines.append(f"- `{item}`")
    write_lines(output_root / "indexes" / "network-endpoints.md", endpoint_lines)

    recovery_lines = result.recovery_plan or ["# Recovery Plan", "", "<none>"]
    write_lines(output_root / "indexes" / "recovery-plan.md", recovery_lines)
    readiness_lines = result.readiness_lines or ["# Tool Readiness", "", "<none>"]
    write_lines(output_root / "indexes" / "tool-readiness.md", readiness_lines)
    install_lines = result.install_plan_lines or ["# Install Plan", "", "<none>"]
    write_lines(output_root / "indexes" / "install-plan.md", install_lines)

    topic_lines = ["# Strings By Topic", ""]
    topic_scores = Counter()
    candidate_lines = ["# Candidate Systems", ""]
    for topic, hits in result.topic_hits.items():
        real_hits = [hit for hit in hits if hit != "<none>"]
        topic_scores[topic] = len(real_hits)
        topic_lines.append(f"## {topic}")
        topic_lines.append("")
        for hit in real_hits[:25] or ["<none>"]:
            topic_lines.append(f"- `{hit}`")
        topic_lines.append("")
    write_lines(output_root / "indexes" / "strings-by-topic.md", topic_lines)

    for topic, count in topic_scores.most_common():
        candidate_lines.append(f"- `{topic}`: {count} matching strings")
    write_lines(output_root / "indexes" / "candidate-systems.md", candidate_lines)
    return topic_scores


def main() -> int:
    args = parse_args()
    input_root = Path(args.input_root).resolve()
    output_root = Path(args.output_root).resolve() if args.output_root else input_root / "game-knowledge"

    ensure_dirs(output_root)

    result = scan_tree(
        input_root,
        max_file_size=args.max_file_size_mb * 1024 * 1024,
        refresh_raw=args.refresh_raw,
        output_root=output_root,
    )

    write_raw_outputs(output_root, result)
    write_files_inventory(output_root, result)
    topic_scores = write_indexes(output_root, result)
    write_yaml(output_root / "manifest.yaml", build_manifest(input_root, output_root, result, topic_scores))
    write_yaml(output_root / "progress.yaml", build_progress(input_root, output_root, result, topic_scores))

    print(f"input_root={input_root}")
    print(f"output_root={output_root}")
    print(f"unity_confirmed={str(result.unity_confirmed).lower()}")
    print(f"backend={result.backend}")
    print(f"managed_assemblies={len(result.managed_assemblies)}")
    print(f"native_libs={len(result.native_libs)}")
    print(f"asset_files={len(result.asset_files)}")
    print(f"tool_hits={sum(1 for path in result.tool_availability.values() if path)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
