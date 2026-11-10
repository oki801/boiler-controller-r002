# PlatformIO pre-script: verzio beagyazasa (szoftver verzio + a REVISION fajl szama)
Import("env")  # noqa: F821
import os
import re

VERSION = "0.2.0"
rev, suffix = 0, ""
try:
    with open(os.path.join(env["PROJECT_DIR"], "REVISION"), encoding="utf-8") as f:  # noqa: F821
        m = re.match(r"^(\d+)([A-Za-z]*)", f.read().strip())     # pl. "10D": a szam utani betu a valtozat jele
        if m:
            rev, suffix = int(m.group(1)), m.group(2)
except Exception:
    pass
build = "%s-r%03d%s" % (VERSION, rev, suffix) if rev else VERSION + "-dev"
env.Append(CPPDEFINES=[("FW_BUILD", env.StringifyMacro(build))])  # noqa: F821
