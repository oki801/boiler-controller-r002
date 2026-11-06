#!/usr/bin/env python3
"""Valtozat-szamozas es csomagolas.

Minden modositasnal a REVISION szam eggyel no, a csomag es a benne levo konyvtar neve tartalmazza:
    boiler-controller-r012.zip  ->  boiler-controller-r012/...

Hasznalat:
    python3 scripts/release.py "rovid megjegyzes a valtozasrol"          # +1, CHANGELOG sor, csomag a szulo mappaba
    python3 scripts/release.py "megjegyzes" --out /utvonal               # a csomag helye
    python3 scripts/release.py --no-bump                                  # csak csomagolas, a szam valtozatlan
"""
import argparse
import datetime
import os
import sys
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXCLUDE_DIRS = {".pio", "__pycache__", ".git"}
EXCLUDE_EXT = (".zip", ".pyc")


def read_rev():
    try:
        return int(open(os.path.join(ROOT, "REVISION"), encoding="utf-8").read().strip() or 0)
    except Exception:
        return 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("note", nargs="?", default="")
    ap.add_argument("--out", default=os.path.dirname(ROOT))
    ap.add_argument("--no-bump", action="store_true")
    a = ap.parse_args()

    rev = read_rev()
    if not a.no_bump:
        rev += 1
        with open(os.path.join(ROOT, "REVISION"), "w", encoding="utf-8", newline="\n") as f:
            f.write("%d\n" % rev)
        line = "- **r%03d** (%s): %s\n" % (rev, datetime.date.today().isoformat(), a.note or "modositas")
        path = os.path.join(ROOT, "CHANGELOG.md")
        old = open(path, encoding="utf-8").read() if os.path.exists(path) else "# Valtozasnaplo\n\nMinden modositas eggyel noveli a REVISION szamot (a csomag neve: boiler-controller-rNNN).\n\n"
        head, sep, rest = old.partition("\n\n- ")
        if sep:
            new = head + "\n\n" + line + "- " + rest
        else:
            new = old.rstrip("\n") + "\n\n" + line
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(new)

    name = "boiler-controller-r%03d" % rev
    out = os.path.join(a.out, name + ".zip")
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for root, dirs, files in os.walk(ROOT):
            dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS]
            for fn in files:
                if fn.endswith(EXCLUDE_EXT):
                    continue
                full = os.path.join(root, fn)
                z.write(full, os.path.join(name, os.path.relpath(full, ROOT)))
    print("%s  (%d kB)" % (out, os.path.getsize(out) // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main())
