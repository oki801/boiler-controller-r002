#!/usr/bin/env python3
"""A partitions_*.csv ellenorzese: atfedes, igazitas, meret. Hasznalat: python3 scripts/check_partitions.py [fajl] [flash_meret_bajt]"""
import csv, sys

def parse(path):
    rows = []
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.split("#")[0].strip()
            if not line:
                continue
            c = [x.strip() for x in line.split(",")]
            rows.append((c[0], c[1], int(c[3], 0), int(c[4], 0)))
    return rows

def check(path, flash=16 * 1024 * 1024):
    rows = sorted(parse(path), key=lambda r: r[2])
    ok = True
    prev_end, prev_name = 0x8000 + 0x1000, "partition table"   # a tabla a 0x8000-on van, 4 kB
    for name, typ, off, size in rows:
        if off < prev_end:
            print("ATFEDES: %s (0x%x) kezdete a(z) %s vege (0x%x) elott" % (name, off, prev_name, prev_end)); ok = False
        if typ == "app" and off % 0x10000:
            print("IGAZITAS: app partition (%s) 0x10000-re igazitott kell legyen" % name); ok = False
        if off % 0x1000:
            print("IGAZITAS: %s nem 4 kB-ra igazitott" % name); ok = False
        prev_end, prev_name = off + size, name
    if prev_end > flash:
        print("MERET: a tabla (0x%x) nagyobb a flash-nel (0x%x)" % (prev_end, flash)); ok = False
    return ok

if __name__ == "__main__":
    p = sys.argv[1] if len(sys.argv) > 1 else "partitions_16MB.csv"
    fl = int(sys.argv[2], 0) if len(sys.argv) > 2 else 16 * 1024 * 1024
    good = check(p, fl)
    print("partitions OK" if good else "partitions HIBAS")
    sys.exit(0 if good else 1)
