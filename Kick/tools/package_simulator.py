"""Package the Windows community simulator from the no-ASIO dist build only."""
from argparse import ArgumentParser
from datetime import datetime, timezone
from hashlib import sha256
from pathlib import Path
import ctypes
import json
import re
import shutil
import subprocess
import zipfile

repo = Path(__file__).resolve().parents[2]
build = repo / "Kick/build/dist"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=repo)


def digest(path):
    return sha256(path.read_bytes()).hexdigest()


def executable_version(path):
    version = ctypes.windll.version
    size = version.GetFileVersionInfoSizeW(str(path), None)
    assert size, "Executable has no Windows version resource"
    data = ctypes.create_string_buffer(size)
    assert version.GetFileVersionInfoW(str(path), 0, size, data)
    fixed = ctypes.c_void_p()
    length = ctypes.c_uint()
    assert version.VerQueryValueW(data, "\\", ctypes.byref(fixed), ctypes.byref(length))
    fields = (ctypes.c_uint32 * 13).from_address(fixed.value)
    return f"{fields[2] >> 16}.{fields[2] & 65535}.{fields[3] >> 16}"


def main():
    parser = ArgumentParser(description=__doc__)
    parser.add_argument("--check-only", action="store_true", help="Validate inputs without requiring a clean tree or creating an archive")
    args = parser.parse_args()
    parts = re.findall(r"VERSION_(?:MAJOR|MINOR|PATCH)\s+(\d+)", (repo / "Kick/apps/kickem/version.txt").read_text())
    assert len(parts) == 3
    version = ".".join(parts)
    exe = build / "KT_KickEM.exe"
    assert executable_version(exe) == version, "Rebuild dist: executable version differs from source"
    cache = (build / "CMakeCache.txt").read_text()
    assert "KICK_ASIO_SDK_DIR:PATH=\n" in cache, "Only the no-ASIO dist configuration may be packaged"
    assert "USE_ASAN_KICK:BOOL=OFF" in cache
    assert "JUCE_ASIO=0;" in (build / "apps/kickem/KT_KickEM.vcxproj").read_text()
    tracked = [p.decode("utf-8") for p in git("ls-files", "-z").split(b"\0") if p]
    runtime = [p for p in tracked if p.startswith(("Kick/assets/", "Kick/templates/", "Kick/demos/"))]
    for item in runtime:
        path = Path(item).relative_to("Kick")
        assert (build / path).is_file(), f"Missing runtime file: {path}"
        assert digest(build / path) == digest(repo / item), f"Stale runtime file: {path}"
    if args.check_only:
        print(json.dumps({"version": version, "asio": False, "runtime_files": len(runtime), "exe_sha256": digest(exe)}, indent=2))
        return
    assert not git("status", "--porcelain"), "Commit all release inputs before packaging"
    commit = git("rev-parse", "HEAD").decode().strip()
    name = f"KICK-Simulator-v{version}-win64"
    stage = build / "releases" / name
    stage.mkdir(parents=True, exist_ok=False)

    def copy(source, relative):
        destination = stage / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, destination)

    copy(exe, "KT_KickEM.exe")
    for item in runtime:
        path = Path(item).relative_to("Kick")
        copy(build / path, path)
    for item in tracked:
        if item.startswith(("Kick/docs/manual/", "docs/audit/", "docs/images/")):
            copy(repo / item, item)  # Preserve relative documentation links.
        elif item.startswith(("Kick/", "AudIO/", "ComProtocol/")) and "Example/" not in item:
            if Path(item).name.lower().startswith(("license", "copying")):
                copy(repo / item, Path("licenses") / item)
    for item in ("README.md", "LICENSE", "FEATURE-INVENTORY.md", "RELEASE-NOTES.md", "AUDIT-FIXES.md", "AUDIT-REVIEW.md", "AUDIT-HANDOFF.md",
                 "Kick/README.md", "Kick/how-to/release-simulator.md", "Kick/apps/kickem/artwork/kick-panel.svg"):
        copy(repo / item, item)
    juce = list((repo / ".3rdPartySourceCache").glob("juce/7.0.12/LICENSE.md"))
    if not juce:
        juce = list((repo / ".3rdPartySourceCache").glob("juce/*/LICENSE.md"))
    assert len(juce) == 1, "Locate the JUCE license for this build"
    copy(juce[0], "licenses/JUCE-LICENSE.md")
    source_url = f"https://github.com/chillbeast/0700-AAM/tree/{commit}"
    (stage / "README.txt").write_text(
        f"KICK community simulator v{version} - Windows x64\n\n"
        "Extract the whole archive and run KT_KickEM.exe. Keep assets, templates\n"
        "and demos alongside the executable. Select a Windows device in Settings > Audio.\n"
        "Use Hardware / Original in the toolbar to choose the saved panel layout.\n"
        "Manual: Kick/docs/manual/USER_MANUAL.md. Changes: RELEASE-NOTES.md.\n"
        "Known gaps: FEATURE-INVENTORY.md. This is an unofficial desktop simulator.\n"
        "ASIO/SDK, personal preferences and commercial sample kits are excluded.\n\n"
        f"Source revision: {commit}\nSource: {source_url}\n"
        f"Source archive: https://github.com/chillbeast/0700-AAM/archive/{commit}.zip\n"
        "License: AGPL-3.0-or-later; see LICENSE and licenses/.\n", encoding="utf-8")
    (stage / "BUILD-INFO.json").write_text(json.dumps({
        "version": version, "commit": commit, "branch": "desktop-build", "configuration": "Release",
        "platform": "Windows x64", "asio": False, "address_sanitizer": False,
        "packaged_utc": datetime.now(timezone.utc).isoformat(), "source_url": source_url,
        "tracked_runtime_files": len(runtime), "exe_sha256": digest(exe),
    }, indent=2) + "\n", encoding="utf-8")
    files = sorted(p for p in stage.rglob("*") if p.is_file())
    (stage / "SHA256SUMS.txt").write_text("".join(f"{digest(p)}  {p.relative_to(stage).as_posix()}\n" for p in files), encoding="utf-8")
    files.append(stage / "SHA256SUMS.txt")
    archive = stage.parent / (name + ".zip")
    with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as output:
        for path in files:
            output.write(path, Path(name) / path.relative_to(stage))
    with zipfile.ZipFile(archive) as output:
        assert output.testzip() is None
    checksum = digest(archive)
    archive.with_suffix(".zip.sha256").write_text(f"{checksum}  {archive.name}\n", encoding="utf-8")
    print(json.dumps({"version": version, "commit": commit, "stage": str(stage), "archive": str(archive),
                      "sha256": checksum, "bytes": archive.stat().st_size, "files": len(files)}, indent=2))


if __name__ == "__main__":
    main()
