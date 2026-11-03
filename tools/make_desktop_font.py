#!/usr/bin/env python3
"""Compatibility entry point for the SrvrUI font generator (requires Pillow)."""
from pathlib import Path
import runpy
runpy.run_path(str(Path(__file__).resolve().parents[1] / "userspace/widgets/tools/make_desktop_font.py"), run_name="__main__")
