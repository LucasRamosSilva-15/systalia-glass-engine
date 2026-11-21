#!/usr/bin/env python3
"""Automated tests for SYSTALIA temporary evidence management (Consolidation 06.6A.2)."""

import json
import os
import pathlib
import signal
import subprocess
import sys
import tempfile
import threading
import time
import unittest

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(REPO_ROOT / "tools" / "maintenance"))

from sge_evidence import (  # noqa: E402
    MANAGED_BY,
    EvidenceRun,
    default_runs_root,
    prune_expired_runs,
)


class TestEvidenceCleanup(unittest.TestCase):
    def setUp(self) -> None:
        self._tmp = tempfile.TemporaryDirectory(prefix="tst-sge-evidence-")
        self._old_tmpdir = os.environ.get("TMPDIR")
        self._old_keep = os.environ.get("SGE_KEEP_EVIDENCE")
        self._old_keep_dir = os.environ.get("SGE_KEEP_EVIDENCE_DIR")
        os.environ["TMPDIR"] = self._tmp.name
        os.environ.pop("SGE_KEEP_EVIDENCE", None)
        os.environ.pop("SGE_KEEP_EVIDENCE_DIR", None)

    def tearDown(self) -> None:
        if self._old_tmpdir is None:
            os.environ.pop("TMPDIR", None)
        else:
            os.environ["TMPDIR"] = self._old_tmpdir
        if self._old_keep is None:
            os.environ.pop("SGE_KEEP_EVIDENCE", None)
        else:
            os.environ["SGE_KEEP_EVIDENCE"] = self._old_keep
        if self._old_keep_dir is None:
            os.environ.pop("SGE_KEEP_EVIDENCE_DIR", None)
        else:
            os.environ["SGE_KEEP_EVIDENCE_DIR"] = self._old_keep_dir
        self._tmp.cleanup()

    def test_successful_run_cleans_up_automatically(self) -> None:
        captured_path = None
        with EvidenceRun("unit-success") as run:
            captured_path = run.path
            self.assertTrue(captured_path.is_dir())
            self.assertEqual(captured_path.parent, default_runs_root())
            (captured_path / "sample.log").write_text("ok\n", encoding="utf-8")
            run.mark_success()
        self.assertIsNotNone(captured_path)
        self.assertFalse(captured_path.exists())

    def test_failed_subprocess_preserves_evidence_and_prunes_expired(self) -> None:
        captured_path = None
        with self.assertRaises(RuntimeError):
            with EvidenceRun("unit-failure") as run:
                captured_path = run.path
                (captured_path / "sde.log").write_text("boom\n", encoding="utf-8")
                run.mark_failure(2)
                raise RuntimeError("simulated subprocess failure")
        self.assertIsNotNone(captured_path)
        self.assertTrue(captured_path.exists())
        status = json.loads((captured_path / "run-status.json").read_text(encoding="utf-8"))
        self.assertEqual(status["managed_by"], MANAGED_BY)
        self.assertEqual(status["status"], "failed")
        self.assertEqual(status["exit_code"], 2)
        self.assertEqual(status["retention_hours"], 24.0)
        self.assertFalse(status["explicit_keep"])

        # Backdate mtime by 48h and verify TTL pruning cleans terminated failed run.
        old_time = time.time() - 48 * 3600
        os.utime(captured_path, (old_time, old_time))
        removed = prune_expired_runs(default_runs_root(), max_age_hours=24.0)
        self.assertEqual(removed, 1)
        self.assertFalse(captured_path.exists())

    def test_explicit_retention_survives_beyond_24h_ttl(self) -> None:
        os.environ["SGE_KEEP_EVIDENCE"] = "1"
        with EvidenceRun("unit-keep-forever") as run:
            kept_path = run.path
            (kept_path / "capture.png").write_bytes(b"PNG")
            run.mark_success()

        old_time = time.time() - 72 * 3600
        os.utime(kept_path, (old_time, old_time))
        removed = prune_expired_runs(default_runs_root(), max_age_hours=24.0)
        self.assertEqual(removed, 0)
        self.assertTrue(kept_path.exists())
        status = json.loads((kept_path / "run-status.json").read_text(encoding="utf-8"))
        self.assertEqual(status["status"], "success-kept")
        self.assertTrue(status["explicit_keep"])

    def test_active_run_with_old_directory_is_never_pruned(self) -> None:
        with EvidenceRun("unit-active-long", prune_expired=False) as active_run:
            active_path = active_run.path
            old_time = time.time() - 72 * 3600
            os.utime(active_path, (old_time, old_time))
            removed = prune_expired_runs(default_runs_root(), max_age_hours=24.0)
            self.assertEqual(removed, 0)
            self.assertTrue(active_path.exists())
            active_run.mark_success()

    def test_unknown_directory_without_metadata_is_preserved(self) -> None:
        runs_root = default_runs_root()
        runs_root.mkdir(parents=True, exist_ok=True)
        unknown_dir = runs_root / "historical-manual-debug"
        unknown_dir.mkdir()
        sentinel = unknown_dir / "notes.txt"
        sentinel.write_text("historical data\n", encoding="utf-8")

        old_time = time.time() - 96 * 3600
        os.utime(unknown_dir, (old_time, old_time))
        removed = prune_expired_runs(runs_root, max_age_hours=24.0)
        self.assertEqual(removed, 0)
        self.assertTrue(unknown_dir.exists())
        self.assertTrue(sentinel.exists())

    def test_symlink_and_path_traversal_protection(self) -> None:
        external_target = pathlib.Path(self._tmp.name) / "external-sensitive"
        external_target.mkdir(parents=True, exist_ok=True)
        external_file = external_target / "secret.txt"
        external_file.write_text("do not delete\n", encoding="utf-8")

        runs_root = default_runs_root()
        runs_root.mkdir(parents=True, exist_ok=True)
        symlink_entry = runs_root / "malicious-link"
        symlink_entry.symlink_to(external_target)
        old_time = time.time() - 96 * 3600
        os.utime(external_target, (old_time, old_time))

        removed = prune_expired_runs(runs_root, max_age_hours=24.0)
        self.assertEqual(removed, 0)
        self.assertTrue(external_target.exists())
        self.assertTrue(external_file.exists())

        with EvidenceRun("unit-traversal", prune_expired=False) as run:
            self.assertFalse(run._is_safe_owned_temp_dir(symlink_entry))
            self.assertFalse(run._is_safe_owned_temp_dir(run.path / ".." / "other"))
            self.assertFalse(run._is_safe_owned_temp_dir(runs_root))
            run.mark_success()

    def test_interruption_preserves_evidence_with_interrupted_status(self) -> None:
        script = (
            "import pathlib, sys, time\n"
            f"sys.path.insert(0, {str(REPO_ROOT / 'tools' / 'maintenance')!r})\n"
            "from sge_evidence import EvidenceRun\n"
            "with EvidenceRun('unit-sigterm') as run:\n"
            "    (run.path / 'marker.txt').write_text('running', encoding='utf-8')\n"
            "    print('RUN_DIR=' + str(run.path), flush=True)\n"
            "    time.sleep(30)\n"
        )
        proc = subprocess.Popen(
            [sys.executable, "-c", script],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            env=os.environ.copy(),
        )
        run_dir = None
        try:
            assert proc.stdout is not None
            for line in proc.stdout:
                if line.startswith("RUN_DIR="):
                    run_dir = pathlib.Path(line.strip().split("=", 1)[1])
                    break
            self.assertIsNotNone(run_dir)
            proc.send_signal(signal.SIGTERM)
            proc.wait(timeout=10)
        finally:
            if proc.stdout is not None:
                proc.stdout.close()
            if proc.poll() is None:
                proc.kill()
                proc.wait(timeout=5)

        assert run_dir is not None
        self.assertTrue(run_dir.exists())
        status = json.loads((run_dir / "run-status.json").read_text(encoding="utf-8"))
        self.assertEqual(status["managed_by"], MANAGED_BY)
        self.assertEqual(status["status"], "interrupted")

    def test_custom_directory_is_never_deleted(self) -> None:
        custom_dir = pathlib.Path(self._tmp.name) / "user-custom-evidence"
        custom_dir.mkdir(parents=True, exist_ok=True)
        sentinel = custom_dir / "existing-user-file.txt"
        sentinel.write_text("preserve me\n", encoding="utf-8")

        with EvidenceRun("unit-custom", explicit_dir=custom_dir) as run:
            self.assertEqual(run.path, custom_dir.resolve())
            self.assertFalse(run.is_owned_temp)
            (run.path / "new.log").write_text("data\n", encoding="utf-8")
            run.mark_success()

        self.assertTrue(custom_dir.exists())
        self.assertTrue(sentinel.exists())

    def test_concurrent_runs_do_not_collide(self) -> None:
        paths: list[pathlib.Path] = []
        lock = threading.Lock()
        errors: list[Exception] = []

        def worker(idx: int) -> None:
            try:
                with EvidenceRun(f"unit-conc-{idx}", prune_expired=False) as run:
                    with lock:
                        paths.append(run.path)
                    (run.path / f"w{idx}.txt").write_text("ok\n", encoding="utf-8")
                    time.sleep(0.05)
                    run.mark_success()
            except Exception as exc:  # pragma: no cover
                with lock:
                    errors.append(exc)

        threads = [threading.Thread(target=worker, args=(i,)) for i in range(4)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        self.assertFalse(errors)
        self.assertEqual(len(paths), 4)
        self.assertEqual(len(set(paths)), 4)
        for p in paths:
            self.assertFalse(p.exists())

    def test_external_directory_protection(self) -> None:
        external_dir = pathlib.Path(self._tmp.name) / "external-project-dir"
        external_dir.mkdir(parents=True, exist_ok=True)
        important = external_dir / "important.cpp"
        important.write_text("int main() {}\n", encoding="utf-8")

        os.environ["SGE_KEEP_EVIDENCE_DIR"] = str(external_dir)
        with EvidenceRun("unit-external") as run:
            self.assertFalse(run._is_safe_owned_temp_dir(external_dir))
            self.assertFalse(run._is_safe_owned_temp_dir(default_runs_root()))
            run.mark_success()

        self.assertTrue(external_dir.exists())
        self.assertTrue(important.exists())


if __name__ == "__main__":
    unittest.main()
