#!/usr/bin/env python3
"""Testes determinísticos de hardening para tools/homologation/run_tty3_homolog.sh e verify_homolog_env.sh."""

import os
import stat
import subprocess
import tempfile
import unittest
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[2]
RUN_TTY3 = REPO_ROOT / "tools" / "homologation" / "run_tty3_homolog.sh"
RUN_TTY3_3FRAMES = REPO_ROOT / "tools" / "homologation" / "run_tty3_3frames_homolog.sh"
VERIFY_ENV = REPO_ROOT / "tools" / "homologation" / "verify_homolog_env.sh"


def make_executable(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)


class Tty3HomologScriptsTest(unittest.TestCase):
    def setUp(self) -> None:
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        self.mock_bin = self.root / "mock_bin"
        self.homolog_dir = self.root / "homolog_prefix"
        self.log_dir = self.root / "logs"
        self.runtime_dir = self.root / "runtime"
        self.runtime_dir.mkdir(parents=True, exist_ok=True)

        # Mock binary que verifica SGE_HOMOLOG_MAX_FRAMES=1 e SGE_GRAPHICS_DIAGNOSTICS=1
        self.engine_calls_log = self.root / "engine_invocations.txt"
        make_executable(
            self.homolog_dir / "bin" / "systalia_glass_engine",
            f"""#!/usr/bin/env bash
# SGE_HOMOLOG_MAX_FRAMES
echo "MAX=$SGE_HOMOLOG_MAX_FRAMES DIAG=$SGE_GRAPHICS_DIAGNOSTICS DRM=$SGE_USE_DRM_BACKEND" >> "{self.engine_calls_log}"
exit "${{MOCK_ENGINE_RC:-0}}"
""",
        )
        qpa_so = self.homolog_dir / "bin" / "platforms" / "libsge_vulkan_offscreen.so"
        qpa_so.parent.mkdir(parents=True, exist_ok=True)
        qpa_so.write_bytes(b"mock-qpa")

        identity = self.homolog_dir / "share" / "systalia" / "BUILD_IDENTITY.txt"
        identity.parent.mkdir(parents=True, exist_ok=True)
        identity.write_text(
            "repo=LucasRamosSilva-15/systalia-glass-engine\n"
            "branch=valo-graphics-2.0\n"
            "base_commit=f2310e2\n"
            "build_type=Release\n"
            "cxx_flags=-Werror\n",
            encoding="utf-8",
        )

        # Default mocks para tty (/dev/tty3) e loginctl (sessão ativa tty3)
        self._write_tty_mock("/dev/tty3")
        self._write_loginctl_mock(tty="tty3", stype="tty", state="active", active="yes", vtnr="3", seat="seat0")

    def tearDown(self) -> None:
        self.tmp.cleanup()

    def _write_tty_mock(self, tty_name: str) -> None:
        make_executable(
            self.mock_bin / "tty",
            f"#!/usr/bin/env bash\necho '{tty_name}'\n",
        )

    def _write_loginctl_mock(self, *, tty: str, stype: str, state: str, active: str, vtnr: str, seat: str) -> None:
        make_executable(
            self.mock_bin / "loginctl",
            f"""#!/usr/bin/env bash
if [[ "$1" == "show-session" ]]; then
    case "$4" in
        TTY) echo "{tty}" ;;
        Type) echo "{stype}" ;;
        State) echo "{state}" ;;
        Active) echo "{active}" ;;
        VTNr) echo "{vtnr}" ;;
        Seat) echo "{seat}" ;;
    esac
    exit 0
fi
exit 1
""",
        )

    def _clean_env(self) -> dict[str, str]:
        env = {
            "PATH": f"{self.mock_bin}:/usr/bin:/bin",
            "HOME": str(self.root),
            "USER": os.environ.get("USER", "lucasramos"),
            "XDG_RUNTIME_DIR": str(self.runtime_dir),
            "XDG_SESSION_ID": "99",
            "XDG_VTNR": "3",
            "SYSTALIA_HOMOLOG_DIR": str(self.homolog_dir),
            "SYSTALIA_HOMOLOG_LOGS": str(self.log_dir),
        }
        return env

    def _run_tty3(self, env: dict[str, str]) -> subprocess.CompletedProcess[str]:
        return subprocess.run(["bash", str(RUN_TTY3)], env=env, capture_output=True, text=True, timeout=15)

    def test_refuses_tty2_and_graphical_session(self) -> None:
        self._write_tty_mock("/dev/tty2")
        self._write_loginctl_mock(tty="tty2", stype="tty", state="active", active="yes", vtnr="2", seat="seat0")
        env = self._clean_env()
        env["XDG_VTNR"] = "2"
        res = self._run_tty3(env)
        self.assertEqual(res.returncode, 1)
        self.assertFalse(self.engine_calls_log.exists())

        # Também recusa WAYLAND_DISPLAY mesmo se TTY mocks fossem tty3
        self._write_tty_mock("/dev/tty3")
        self._write_loginctl_mock(tty="tty3", stype="tty", state="active", active="yes", vtnr="3", seat="seat0")
        env_gfx = self._clean_env()
        env_gfx["WAYLAND_DISPLAY"] = "wayland-0"
        res_gfx = self._run_tty3(env_gfx)
        self.assertEqual(res_gfx.returncode, 1)
        self.assertFalse(self.engine_calls_log.exists())

    def test_refuses_tty4(self) -> None:
        self._write_tty_mock("/dev/tty4")
        self._write_loginctl_mock(tty="tty4", stype="tty", state="active", active="yes", vtnr="4", seat="seat0")
        env = self._clean_env()
        env["XDG_VTNR"] = "4"
        res = self._run_tty3(env)
        self.assertEqual(res.returncode, 1)
        self.assertIn("tty3", res.stderr)
        self.assertFalse(self.engine_calls_log.exists())

    def test_refuses_ssh(self) -> None:
        for var in ("SSH_CONNECTION", "SSH_CLIENT", "SSH_TTY"):
            env = self._clean_env()
            env[var] = "127.0.0.1 12345 127.0.0.1 22"
            res = self._run_tty3(env)
            self.assertEqual(res.returncode, 1, msg=f"Deveria recusar {var}")
            self.assertIn("SSH", res.stderr)
        self.assertFalse(self.engine_calls_log.exists())

    def test_refuses_inconsistent_logind_session(self) -> None:
        # 1. XDG_SESSION_ID ausente (sem fallback para list-sessions)
        env = self._clean_env()
        env.pop("XDG_SESSION_ID")
        res = self._run_tty3(env)
        self.assertEqual(res.returncode, 1)
        self.assertIn("XDG_SESSION_ID", res.stderr)

        # 2. Sessão logind inativa (Active=no)
        self._write_loginctl_mock(tty="tty3", stype="tty", state="online", active="no", vtnr="3", seat="seat0")
        res2 = self._run_tty3(self._clean_env())
        self.assertEqual(res2.returncode, 1)
        self.assertIn("inconsistente", res2.stderr)

        # 3. XDG_VTNR inconsistente com tty3
        self._write_loginctl_mock(tty="tty3", stype="tty", state="active", active="yes", vtnr="3", seat="seat0")
        env3 = self._clean_env()
        env3["XDG_VTNR"] = "2"
        res3 = self._run_tty3(env3)
        self.assertEqual(res3.returncode, 1)
        self.assertFalse(self.engine_calls_log.exists())

    def test_refuses_missing_or_invalid_build_identity(self) -> None:
        identity = self.homolog_dir / "share" / "systalia" / "BUILD_IDENTITY.txt"
        identity.write_text("branch=other\nbuild_type=Debug\nbase_commit=unknown\n", encoding="utf-8")
        res_bad = self._run_tty3(self._clean_env())
        self.assertEqual(res_bad.returncode, 1)
        self.assertIn("Identidade", res_bad.stderr)

        identity.unlink()
        res_missing = self._run_tty3(self._clean_env())
        self.assertEqual(res_missing.returncode, 1)
        self.assertFalse(self.engine_calls_log.exists())

    def test_forces_one_frame_and_diagnostics_ignoring_inherited_overrides(self) -> None:
        env = self._clean_env()
        env["SGE_HOMOLOG_MAX_FRAMES"] = "0"
        env["SGE_GRAPHICS_DIAGNOSTICS"] = "0"
        env["MOCK_ENGINE_RC"] = "0"
        res = self._run_tty3(env)
        self.assertEqual(res.returncode, 0, msg=res.stdout + res.stderr)
        recorded = self.engine_calls_log.read_text(encoding="utf-8")
        self.assertIn("MAX=1 DIAG=1 DRM=1", recorded)

    def test_blocks_concurrent_execution_with_flock(self) -> None:
        lock_file = self.runtime_dir / f"sge-homolog-{os.getuid()}.lock"
        holder = subprocess.Popen(
            ["flock", str(lock_file), "sleep", "5"],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        try:
            # Aguarda lock estar ativo
            import time
            time.sleep(0.2)
            res = self._run_tty3(self._clean_env())
            self.assertEqual(res.returncode, 1)
            self.assertIn("já existe uma instância", res.stderr.lower())
            self.assertFalse(self.engine_calls_log.exists())
        finally:
            holder.terminate()
            holder.wait(timeout=5)

    def test_handles_pre_launch_failure_without_invoking_compositor(self) -> None:
        # LOG_DIR apontando para um arquivo regular causa falha em mkdir -p antes do compositor
        blocking_file = self.root / "not_a_dir"
        blocking_file.write_text("block", encoding="utf-8")
        env = self._clean_env()
        env["SYSTALIA_HOMOLOG_LOGS"] = str(blocking_file)
        res = self._run_tty3(env)
        self.assertEqual(res.returncode, 1)
        self.assertFalse(self.engine_calls_log.exists())

    def test_three_frame_executor_requires_explicit_flag_and_forces_3_frames(self) -> None:
        # Mesmo com SGE_HOMOLOG_MAX_FRAMES=3 herdado, sem --authorize-3frames deve bloquear (exit 3)
        env = self._clean_env()
        env["SGE_HOMOLOG_MAX_FRAMES"] = "3"
        blocked = subprocess.run(
            ["bash", str(RUN_TTY3_3FRAMES)], env=env, capture_output=True, text=True, timeout=15
        )
        self.assertEqual(blocked.returncode, 3)
        self.assertIn("BLOCKED", blocked.stderr)
        self.assertFalse(self.engine_calls_log.exists())

        # Com --authorize-3frames, força MAX=3 DIAG=1 DRM=1
        env["SGE_HOMOLOG_MAX_FRAMES"] = "0"
        ok = subprocess.run(
            ["bash", str(RUN_TTY3_3FRAMES), "--authorize-3frames"],
            env=env,
            capture_output=True,
            text=True,
            timeout=15,
        )
        self.assertEqual(ok.returncode, 0, msg=ok.stdout + ok.stderr)
        self.assertIn("MAX=3 DIAG=1 DRM=1", self.engine_calls_log.read_text(encoding="utf-8"))


if __name__ == "__main__":
    unittest.main()
