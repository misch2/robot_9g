"""PlatformIO pre-build hook: delegate BMP -> RGB565 header generation to
build_assets.ps1 in this folder. PlatformIO's extra_scripts entry must be
Python, so this file exists only as a thin shim around the PowerShell
script that actually does the work."""

import subprocess
import sys
from pathlib import Path

Import("env")  # noqa: F821 — provided by PlatformIO at script load time

# SCons exec's this file without setting __file__, so anchor on the
# PlatformIO project dir instead.
project_dir = Path(env["PROJECT_DIR"])  # noqa: F821
ps_script   = project_dir / "src" / "assets" / "build_assets.ps1"

result = subprocess.run(
    ["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass",
     "-File", str(ps_script)],
    check=False,
)
if result.returncode != 0:
    sys.exit(result.returncode)
