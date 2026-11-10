#!/usr/bin/env python3
"""A webes feluletek (data/www/*.html) beagyazasa a firmware-be, tomoritve.

Igy a felulet nem fugg a LittleFS-tol: nincs `uploadfs`, nincs elterő firmware/fajlrendszer
verzio, es serult fajlrendszerrel is elerheto marad. PlatformIO-bol pre-scriptkent fut,
de onalloan is hasznalhato:  python3 scripts/embed_ui.py

  data/www/index.html      -> include/ui_html.gz.h  (UI_HTML_GZ)   a teljes kezelofelulet, "/"
  data/www/dashboard.html  -> include/ui_dash.gz.h  (UI_DASH_GZ)   a felhasznaloi dashboard, "/dash" (valtozatlanul beagyazva)
  data/www/dashboard_v2.html -> include/ui_dash2.gz.h (UI_DASH2_GZ) az adatkotott alverzio, "/dash2"
  data/www/dashboard_v3.html -> include/ui_dash3.gz.h (UI_DASH3_GZ) a folyekony (animalt) valtozat, "/dash3"
  data/www/service.html    -> include/ui_service.gz.h (UI_SERVICE_GZ) a szervizoldal (szarazfutas), "/service"
"""
import gzip
import os

TARGETS = [
    ("index.html", "ui_html.gz.h", "UI_HTML_GZ"),
    ("dashboard.html", "ui_dash.gz.h", "UI_DASH_GZ"),
    ("dashboard_v2.html", "ui_dash2.gz.h", "UI_DASH2_GZ"),
    ("dashboard_v3.html", "ui_dash3.gz.h", "UI_DASH3_GZ"),
    ("service.html", "ui_service.gz.h", "UI_SERVICE_GZ"),
]


def generate_one(root, src_name, dst_name, symbol):
    src = os.path.join(root, "data", "www", src_name)
    dst = os.path.join(root, "include", dst_name)
    with open(src, "rb") as f:
        data = gzip.compress(f.read(), 9, mtime=0)          # mtime=0: determinisztikus kimenet
    lines = []
    for i in range(0, len(data), 16):
        lines.append("  " + ", ".join("0x%02x" % b for b in data[i:i + 16]))
    out = (
        "#pragma once\n"
        "// GENERATED FILE (scripts/embed_ui.py), do not edit. Source: data/www/%s\n"
        "#include <Arduino.h>\n"
        "static const uint8_t %s[] PROGMEM = {\n%s\n};\n"
        "static const size_t %s_LEN = %d;\n" % (src_name, symbol, ",\n".join(lines), symbol, len(data))
    )
    old = open(dst, encoding="utf-8").read() if os.path.exists(dst) else ""
    if out != old:
        with open(dst, "w", encoding="utf-8", newline="\n") as f:
            f.write(out)
    return len(data)


def generate(root):
    return {t[0]: generate_one(root, *t) for t in TARGETS}


try:
    Import("env")  # noqa: F821  (PlatformIO/SCons alatt)
    generate(env["PROJECT_DIR"])  # noqa: F821
except NameError:
    sizes = generate(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    for k, v in sizes.items():
        print("%s: %d bajt tomoritve" % (k, v))
