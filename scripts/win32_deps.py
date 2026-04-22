#!/usr/bin/env python3
"""
Analyze a Windows binary's DLL dependencies recursively using dumpbin.
Reports which DLLs are found, which are system libraries (not recursed into),
and which are missing.

Usage: win32_deps.py <binary> [extra_search_dir ...]

Extra search dirs are checked before PATH. The binary's own directory is
always searched first.
"""

import os
import re
import shutil
import subprocess
import sys
from pathlib import Path


SYSTEM_ROOTS = []
for var in ('WINDIR', 'SYSTEMROOT'):
    val = os.environ.get(var)
    if val:
        p = Path(val).resolve()
        if p not in SYSTEM_ROOTS:
            SYSTEM_ROOTS.append(p)
if not SYSTEM_ROOTS:
    SYSTEM_ROOTS = [Path('C:/Windows')]


def _vs_installs_from_registry():
    """Return VS install paths from registry, newest version first."""
    try:
        import winreg
    except ImportError:
        return []

    paths = []
    keys = [
        (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\Microsoft\VisualStudio\SxS\VS7'),
        (winreg.HKEY_LOCAL_MACHINE, r'SOFTWARE\WOW6432Node\Microsoft\VisualStudio\SxS\VS7'),
    ]
    seen = set()
    for hive, subkey in keys:
        try:
            key = winreg.OpenKey(hive, subkey)
        except OSError:
            continue
        i = 0
        while True:
            try:
                name, value, _ = winreg.EnumValue(key, i)
                i += 1
            except OSError:
                break
            norm = str(value).lower()
            if norm not in seen:
                seen.add(norm)
                try:
                    ver_tuple = tuple(int(n) for n in name.split('.'))
                except ValueError:
                    ver_tuple = (0,)
                paths.append((ver_tuple, value))
        winreg.CloseKey(key)

    paths.sort(key=lambda x: x[0], reverse=True)
    return [p for _, p in paths]


def _find_dumpbin_in_vs(vs_root):
    """Search for dumpbin.exe inside a VS installation, latest MSVC toolset first."""
    msvc = Path(vs_root) / 'VC' / 'Tools' / 'MSVC'
    if not msvc.is_dir():
        return None

    def ver_key(p):
        try:
            return tuple(int(n) for n in p.name.split('.'))
        except ValueError:
            return (0,)

    for toolset in sorted(msvc.iterdir(), key=ver_key, reverse=True):
        for host in ('HostX64', 'HostX86'):
            for target in ('x64', 'x86'):
                candidate = toolset / 'bin' / host / target / 'dumpbin.exe'
                if candidate.is_file():
                    return candidate
    return None


def find_dumpbin():
    """Locate dumpbin.exe, preferring the newest VS installation."""
    # Already on PATH (e.g., inside a Developer Command Prompt).
    found = shutil.which('dumpbin')
    if found:
        return Path(found)

    # vswhere.exe is installed alongside the VS installer at a fixed location
    # and is the official way to enumerate VS instances.
    vswhere = Path(os.environ.get('ProgramFiles(x86)', 'C:/Program Files (x86)')) \
        / 'Microsoft Visual Studio' / 'Installer' / 'vswhere.exe'
    if vswhere.is_file():
        result = subprocess.run(
            [str(vswhere), '-latest', '-property', 'installationPath'],
            capture_output=True, text=True
        )
        if result.returncode == 0:
            install_path = result.stdout.strip()
            if install_path:
                db = _find_dumpbin_in_vs(install_path)
                if db:
                    return db

    # Registry fallback: covers older VS versions and non-standard installers.
    for vs_path in _vs_installs_from_registry():
        db = _find_dumpbin_in_vs(vs_path)
        if db:
            return db

    return None


DUMPBIN = find_dumpbin()


def get_imports(binary_path):
    if DUMPBIN is None:
        print('error: dumpbin.exe not found -- install Visual Studio with C++ tools',
              file=sys.stderr)
        sys.exit(1)

    try:
        result = subprocess.run(
            [str(DUMPBIN), '/DEPENDENTS', str(binary_path)],
            capture_output=True, text=True
        )
    except FileNotFoundError:
        print(f'error: could not run {DUMPBIN}', file=sys.stderr)
        sys.exit(1)

    if result.returncode != 0:
        print(f'  warning: dumpbin failed on {binary_path}', file=sys.stderr)
        return []

    dlls = []
    in_deps = False
    for line in result.stdout.splitlines():
        stripped = line.strip()
        if 'Image has the following dependencies:' in line:
            in_deps = True
            continue
        if in_deps:
            if re.match(r'\w.*\.(dll|DLL)', stripped):
                dlls.append(stripped)
            elif stripped.lower() == 'summary':
                break
    return dlls


def find_dll(name, search_dirs):
    for d in search_dirs:
        candidate = Path(d) / name
        if candidate.exists():
            return candidate.resolve()

    try:
        result = subprocess.run(['where', name], capture_output=True, text=True)
        if result.returncode == 0:
            first = result.stdout.splitlines()[0].strip()
            if first:
                return Path(first).resolve()
    except FileNotFoundError:
        pass

    return None


def is_system(path):
    for root in SYSTEM_ROOTS:
        try:
            path.relative_to(root)
            return True
        except ValueError:
            pass
    return False


def analyze(binary, extra_dirs=None, _visited=None, _results=None):
    if _visited is None:
        _visited = set()
    if _results is None:
        _results = {}

    binary = Path(binary).resolve()
    search_dirs = [binary.parent] + (extra_dirs or [])

    for dll in get_imports(binary):
        key = dll.lower()
        if key in _visited:
            continue
        _visited.add(key)

        path = find_dll(dll, search_dirs)
        if path is None:
            _results[dll] = (None, 'missing')
        elif is_system(path):
            _results[dll] = (path, 'system')
        else:
            _results[dll] = (path, 'found')
            analyze(path, extra_dirs, _visited, _results)

    return _results


def main():
    if len(sys.argv) < 2:
        print(f'usage: {sys.argv[0]} <binary.exe|.dll> [extra_search_dir ...]')
        sys.exit(1)

    binary = Path(sys.argv[1])
    if not binary.exists():
        print(f'error: {binary} not found', file=sys.stderr)
        sys.exit(1)

    extra_dirs = [Path(d) for d in sys.argv[2:]]

    print(f'dumpbin:   {DUMPBIN}')
    print(f'Analyzing: {binary.resolve()}')
    results = analyze(binary, extra_dirs)

    found   = {k: v for k, v in results.items() if v[1] == 'found'}
    system  = {k: v for k, v in results.items() if v[1] == 'system'}
    missing = {k: v for k, v in results.items() if v[1] == 'missing'}

    if found:
        print(f'\nFound ({len(found)}):')
        for name, (path, _) in sorted(found.items(), key=lambda x: x[0].lower()):
            print(f'  {name}')
            print(f'    {path}')

    if system:
        print(f'\nSystem ({len(system)}):')
        for name in sorted(system.keys(), key=str.lower):
            print(f'  {name}')

    if missing:
        print(f'\nMISSING ({len(missing)}):')
        for name in sorted(missing.keys(), key=str.lower):
            print(f'  {name}')
        sys.exit(1)
    else:
        print('\nAll non-system dependencies satisfied.')


if __name__ == '__main__':
    main()
