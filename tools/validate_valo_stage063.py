#!/usr/bin/env python3
"""Run opt-in diagnostics on the real SDE."""
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
mode = sys.argv[1] if len(sys.argv) > 1 else "dock"
if mode not in {
    "resize", "dock", "control", "topbar", "policy", "clients",
    "pair-stress", "limited-dock", "limited-topbar", "limited-stress",
}:
    raise SystemExit("Modo inválido")

with EvidenceRun(f"stage063-{mode}") as run:
    out = run.path
    with tempfile.TemporaryDirectory(prefix="valo063-") as runtime_dir:
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
        )
        if mode == "resize":
            env["SGE_VALO_INTERNAL_RESIZE_PROBE"] = "1"
        if mode in ["dock", "control", "policy"]:
            env["SGE_VALO_OVERLAY_DOCK_SCENARIO"] = "1"
        if mode == "topbar":
            env["SGE_VALO_OVERLAY_TOPBAR_SCENARIO"] = "1"
        if mode == "limited-dock":
            env["SGE_VALO_LIMITED_DOCK_SCENARIO"] = "1"
        if mode == "limited-topbar":
            env["SGE_VALO_LIMITED_TOPBAR_SCENARIO"] = "1"
        if mode == "limited-stress":
            env.update(SGE_VALO_LIMITED_DOCK_SCENARIO="1", SGE_VALO_PROFILE_STRESS="3")
        if mode in ["dock", "control", "topbar", "limited-dock", "limited-topbar"]:
            env.update(SGE_VALO_BACKDROP_BENCHMARK="1", SGE_VALO_OPTICAL_PROFILE="1")
        if mode == "limited-dock":
            env["SGE_VALO_OPTICAL_TIMELINE"] = "1"
        if mode == "control":
            env["SGE_VALO_WALLPAPER_ONLY"] = "1"
        if mode == "policy":
            env["SGE_VALO_POLICY_PROBE"] = "1"
        if mode == "clients":
            env.update(SGE_VALO_CLIENT_MIGRATION_PROBE="1", SGE_VALO_INTERNAL_OVERLAP="1")
        if mode == "pair-stress":
            env.update(SGE_VALO_INTERNAL_OVERLAP="1", SGE_VALO_PROFILE_STRESS="3")

        with open(out / "sde.log", "w") as f:
            p = subprocess.Popen(
                ["dbus-run-session", "--", "build/systalia_glass_engine"],
                env=env,
                stdout=f,
                stderr=subprocess.STDOUT,
                start_new_session=True,
            )
            client = None
            try:
                if mode == "clients":
                    for _ in range(100):
                        if pathlib.Path(runtime_dir, "wayland-systalia-0").exists():
                            break
                        if p.poll() is not None:
                            raise RuntimeError("SDE startup failed")
                        time.sleep(0.1)
                    clientenv = env.copy()
                    clientenv.update(WAYLAND_DISPLAY="wayland-systalia-0", WINIT_UNIX_BACKEND="wayland")
                    client = subprocess.Popen(
                        ["alacritty", "-e", "sh"],
                        env=clientenv,
                        stdout=subprocess.DEVNULL,
                        stderr=subprocess.DEVNULL,
                    )
                result = p.wait(timeout=90)
                print(mode, "SDE exit", result, flush=True)
                if result:
                    run.mark_failure(result)
                    raise SystemExit(result)
                run.mark_success()
            finally:
                if client and client.poll() is None:
                    client.terminate()
                    client.wait(timeout=5)
                if p.poll() is None:
                    os.killpg(p.pid, signal.SIGTERM)
                    p.wait(timeout=5)
