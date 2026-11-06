# PlatformIO pre-script: verzio beagyazasa (szoftver verzio + a REVISION fajl szama)
Import("env")  # noqa: F821
import os

VERSION = "0.2.0"
rev = 0
try:
    with open(os.path.join(env["PROJECT_DIR"], "REVISION"), encoding="utf-8") as f:  # noqa: F821
        rev = int(f.read().strip() or 0)
except Exception:
    pass
build = "%s-r%03d" % (VERSION, rev) if rev else VERSION + "-dev"
env.Append(CPPDEFINES=[("FW_BUILD", env.StringifyMacro(build))])  # noqa: F821
