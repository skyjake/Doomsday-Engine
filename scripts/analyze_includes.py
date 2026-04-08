#!/usr/bin/env python3
"""Analyze #include usage across the Doomsday Engine source tree.

Counts how many translation units (.cpp files) include each header.
Useful for deciding which headers to put in precompiled header files.

Usage:
    python3 scripts/analyze_includes.py [options]

Options:
    --top N          Show top N headers per section (default: 30)
    --filter CAT     Filter by category: stl | system | project
    --dirs D1,D2     Comma-separated subdirs to scan (default: libs,apps)
    --min-count N    Only show headers included in at least N TUs (default: 3)
    --by-target      Break down by immediate subdirectory (one section per target)
    --target NAME    Restrict to a single target subdirectory (e.g. "libs/core")
"""

import sys, os, re, argparse
from collections import defaultdict, Counter

STL_HEADERS = {
    'vector', 'list', 'deque', 'array', 'forward_list',
    'set', 'map', 'unordered_set', 'unordered_map',
    'stack', 'queue', 'priority_queue', 'bitset', 'span',
    'algorithm', 'functional', 'numeric', 'iterator',
    'utility', 'tuple', 'optional', 'variant', 'any',
    'type_traits', 'typeinfo',
    'iostream', 'fstream', 'sstream', 'ostream', 'istream',
    'iomanip', 'streambuf',
    'string', 'string_view', 'regex',
    'memory', 'new', 'limits',
    'thread', 'mutex', 'condition_variable', 'atomic', 'future',
    'cstdio', 'cstdlib', 'cstring', 'cassert', 'cmath',
    'cstdint', 'cinttypes', 'cerrno', 'ctime', 'clocale',
    'climits', 'cfloat', 'cctype', 'cwchar',
    'exception', 'stdexcept', 'system_error', 'chrono',
    'random', 'ratio', 'complex', 'valarray',
    'initializer_list', 'compare', 'concepts',
}


def is_stl(header):
    name = header.strip('<>').split('/')[-1]
    return name in STL_HEADERS


def classify(header, is_angle):
    if is_angle:
        return 'stl' if is_stl(header) else 'system'
    return 'project'


def scan_file(path, counts, per_header_files):
    try:
        with open(path, encoding='utf-8', errors='replace') as f:
            for line in f:
                line = line.strip()
                m = re.match(r'#\s*include\s+([<"])([^>"]+)[>"]', line)
                if m:
                    bracket, name = m.group(1), m.group(2)
                    is_angle = bracket == '<'
                    full = ('<' if is_angle else '"') + name + ('>' if is_angle else '"')
                    cat = classify(full, is_angle)
                    counts[cat][full] += 1
                    per_header_files[full].add(path)
    except Exception as e:
        print(f"Warning: could not read {path}: {e}", file=sys.stderr)


SKIP_DIRS = {'build', 'products', '.git', 'assimp', 'glbinding', 'glm',
             'catch', 'tests', '__pycache__'}


def collect_cpp_files(scan_dir):
    result = []
    for dirpath, dirnames, filenames in os.walk(scan_dir):
        dirnames[:] = sorted(d for d in dirnames if d not in SKIP_DIRS)
        for fn in filenames:
            if fn.endswith('.cpp'):
                result.append(os.path.join(dirpath, fn))
    return result


def print_table(counter, total_cpp, top, min_count, header_label='Header'):
    items = [(h, n) for h, n in counter.most_common() if n >= min_count]
    if not items:
        print("  (none)\n")
        return
    for h, n in items[:top]:
        pct = 100 * n / total_cpp if total_cpp else 0
        print(f"  {n:5d} ({pct:5.1f}%)  {h}")
    if len(items) > top:
        print(f"  ... and {len(items) - top} more")
    print()


def analyse(cpp_files, categories, top, min_count):
    counts = {'stl': Counter(), 'system': Counter(), 'project': Counter()}
    per_header = defaultdict(set)
    for path in cpp_files:
        scan_file(path, counts, per_header)
    total = len(cpp_files)
    cat_labels = {
        'stl':     'C++ STL / standard library',
        'system':  'Other system (<angle-bracket>)',
        'project': 'Project ("quoted")',
    }
    for cat in categories:
        print(f"  --- {cat_labels[cat]} ---")
        print_table(counts[cat], total, top, min_count)
    return counts, total


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--top', type=int, default=30)
    parser.add_argument('--filter', choices=['stl', 'system', 'project'])
    parser.add_argument('--dirs', default='libs,apps')
    parser.add_argument('--min-count', type=int, default=3)
    parser.add_argument('--by-target', action='store_true',
                        help='Break down by top-level component directory')
    parser.add_argument('--target',
                        help='Restrict scan to a single subdir (e.g. libs/core)')
    args = parser.parse_args()

    root = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
    categories = [args.filter] if args.filter else ['stl', 'system', 'project']

    if args.target:
        # Single-target mode
        target_path = os.path.join(root, args.target)
        cpp_files = collect_cpp_files(target_path)
        print(f"\n=== {args.target} ({len(cpp_files)} TUs) ===\n")
        analyse(cpp_files, categories, args.top, args.min_count)
        return

    if args.by_target:
        # Enumerate immediate subdirs of each scan root
        targets = []
        for d in args.dirs.split(','):
            base = os.path.join(root, d)
            if not os.path.isdir(base):
                continue
            for sub in sorted(os.listdir(base)):
                full = os.path.join(base, sub)
                if os.path.isdir(full) and sub not in SKIP_DIRS:
                    targets.append((f"{d}/{sub}", full))
        for label, path in targets:
            cpp_files = collect_cpp_files(path)
            if not cpp_files:
                continue
            print(f"\n=== {label} ({len(cpp_files)} TUs) ===\n")
            analyse(cpp_files, categories, args.top, args.min_count)
        return

    # Global aggregate mode
    scan_dirs = [os.path.join(root, d) for d in args.dirs.split(',')]
    all_cpp = []
    for d in scan_dirs:
        all_cpp += collect_cpp_files(d)

    print(f"Scanned {len(all_cpp)} .cpp files\n")
    analyse(all_cpp, categories, args.top, args.min_count)


if __name__ == '__main__':
    main()
