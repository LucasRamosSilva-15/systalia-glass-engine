#!/usr/bin/env python3
"""Capture header alignment in the actual Vulkan SDE inside a host Wayland session."""
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from sge_evidence import EvidenceRun

root = pathlib.Path(__file__).resolve().parents[1]
os.chdir(root)

with EvidenceRun("header") as run:
    out = run.path
    with tempfile.TemporaryDirectory(prefix="valo061-") as runtime_dir:
        env = os.environ.copy()
        host = env["WAYLAND_DISPLAY"]
        host = host if host.startswith("/") else env["XDG_RUNTIME_DIR"] + "/" + host
        env.update(
            XDG_RUNTIME_DIR=runtime_dir,
            WAYLAND_DISPLAY=host,
            QT_QPA_PLATFORM="wayland",
            QT_QPA_PLATFORMTHEME="generic",
            QT_NO_XDG_DESKTOP_PORTAL="1",
            SGE_VALO_STABILITY_PROBE_DIR=str(out),
            SGE_VALO_HEADER_ALIGNMENT_PROBE="1",
        )
        with open(out / "sde.log", "w") as log:
            p = subprocess.Popen(
                ["dbus-run-session", "--", "build/systalia_glass_engine"],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            c = None
            try:
                for _ in range(100):
                    if pathlib.Path(runtime_dir, "wayland-systalia-0").exists():
                        break
                    if p.poll() is not None:
                        raise RuntimeError("SDE startup failed")
                    time.sleep(0.1)
                ce = env.copy()
                ce.update(WAYLAND_DISPLAY="wayland-systalia-0", WINIT_UNIX_BACKEND="wayland")
                with open(out / "alacritty.log", "w") as cl:
                    c = subprocess.Popen(
                        [
                            "alacritty", "-e", "sh", "-c",
                            '(sleep 6; printf "\\033]0;Alacritty — título longo para verificar truncamento sem sobrepor os controles da janela VALO\\007") & exec sh',
                        ],
                        env=ce,
                        stdout=cl,
                        stderr=subprocess.STDOUT,
                    )
                result = p.wait(timeout=40)
                print("SDE", result, flush=True)
                if result:
                    run.mark_failure(result)
                    raise SystemExit(result)
                run.mark_success()
            finally:
                if c is not None and c.poll() is None:
                    c.terminate()
                    c.wait(timeout=5)
                if p.poll() is None:
                    os.killpg(p.pid, signal.SIGTERM)
                    p.wait(timeout=5)
