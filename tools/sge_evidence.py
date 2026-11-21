#!/usr/bin/env python3
"""Reusable temporary evidence directory manager for SYSTALIA diagnostic tools."""

from __future__ import annotations

import json
import os
import pathlib
import shutil
import signal
import tempfile
import time
from typing import Optional


def default_runs_root() -> pathlib.Path:
    tmpdir = os.environ.get("TMPDIR") or tempfile.gettempdir() or "/tmp"
    return pathlib.Path(tmpdir).resolve() / "systalia-runs"


def prune_expired_runs(runs_root: Optional[pathlib.Path] = None, max_age_hours: float = 24.0) -> int:
    """Remove expired temporary run directories under runs_root older than max_age_hours."""
    root = (runs_root or default_runs_root()).resolve()
    if not root.is_dir():
        return 0
    cutoff = time.time() - max_age_hours * 3600.0
    removed = 0
    for entry in root.iterdir():
        if not entry.is_dir() or entry.is_symlink():
            continue
        try:
            if entry.stat().st_mtime < cutoff:
                shutil.rmtree(entry, ignore_errors=True)
                removed += 1
        except OSError:
            pass
    return removed


class EvidenceRun:
    """Context manager creating a unique run directory with safe cleanup semantics.

    - Successful run (mark_success()): cleans up the owned temp directory unless
      SGE_KEEP_EVIDENCE=1 or a user-supplied directory (explicit_dir / SGE_KEEP_EVIDENCE_DIR)
      was used.
    - Failed or interrupted run: preserves the owned temp directory for diagnosis with a
      run-status.json metadata file (retention_hours=24) unless SGE_CLEAN_ON_FAILURE=1.
    - Never deletes user-supplied or external directories.
    """

    def __init__(
        self,
        tool_name: str,
        *,
        explicit_dir: Optional[str | pathlib.Path] = None,
        retention_hours: float = 24.0,
        prune_expired: bool = True,
    ) -> None:
        self.tool_name = tool_name
        self.retention_hours = retention_hours
        self.runs_root = default_runs_root()
        self.keep_requested = os.environ.get("SGE_KEEP_EVIDENCE", "") == "1"
        self.clean_on_failure = os.environ.get("SGE_CLEAN_ON_FAILURE", "") == "1"

        env_dir = os.environ.get("SGE_KEEP_EVIDENCE_DIR", "").strip()
        chosen_user_dir = explicit_dir if explicit_dir is not None else (env_dir or None)

        self._owned_temp_dir: Optional[pathlib.Path] = None
        self._user_dir: Optional[pathlib.Path] = None
        self._succeeded = False
        self._interrupted = False
        self._exit_code: Optional[int] = None
        self._prev_sigterm = None
        self._prev_sigint = None

        if prune_expired:
            prune_expired_runs(self.runs_root, max_age_hours=self.retention_hours)

        if chosen_user_dir is not None:
            self._user_dir = pathlib.Path(chosen_user_dir).resolve()
            self._user_dir.mkdir(parents=True, exist_ok=True)
        else:
            self.runs_root.mkdir(parents=True, exist_ok=True)
            stamp = time.strftime("%Y%m%d-%H%M%S")
            prefix = f"{tool_name}-{stamp}-{os.getpid()}-"
            self._owned_temp_dir = pathlib.Path(
                tempfile.mkdtemp(prefix=prefix, dir=str(self.runs_root))
            ).resolve()

    @property
    def path(self) -> pathlib.Path:
        return self._user_dir if self._user_dir is not None else self._owned_temp_dir  # type: ignore[return-value]

    @property
    def is_owned_temp(self) -> bool:
        return self._owned_temp_dir is not None and self._user_dir is None

    def _is_safe_owned_temp_dir(self, candidate: pathlib.Path) -> bool:
        if not self.is_owned_temp or self._owned_temp_dir is None:
            return False
        try:
            resolved = candidate.resolve()
            root_resolved = self.runs_root.resolve()
            if resolved == root_resolved or resolved.is_symlink():
                return False
            resolved.relative_to(root_resolved)
            return resolved == self._owned_temp_dir
        except ValueError:
            return False

    def mark_success(self) -> None:
        self._succeeded = True
        self._exit_code = 0

    def mark_failure(self, exit_code: int = 1) -> None:
        self._succeeded = False
        self._exit_code = exit_code

    def _write_status(self, status: str) -> None:
        target = self.path / "run-status.json"
        payload = {
            "tool": self.tool_name,
            "status": status,
            "exit_code": self._exit_code,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "retention_hours": self.retention_hours,
        }
        try:
            target.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        except OSError:
            pass

    def _handle_signal(self, signum: int, _frame) -> None:
        self._interrupted = True
        self._exit_code = 128 + signum
        raise InterruptedError(f"Interrupted by signal {signum}")

    def __enter__(self) -> "EvidenceRun":
        try:
            self._prev_sigterm = signal.getsignal(signal.SIGTERM)
            self._prev_sigint = signal.getsignal(signal.SIGINT)
            signal.signal(signal.SIGTERM, self._handle_signal)
            signal.signal(signal.SIGINT, self._handle_signal)
        except ValueError:
            # Not in main thread
            pass
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> bool:
        try:
            if self._prev_sigterm is not None:
                signal.signal(signal.SIGTERM, self._prev_sigterm)
            if self._prev_sigint is not None:
                signal.signal(signal.SIGINT, self._prev_sigint)
        except ValueError:
            pass

        if exc_type is not None:
            if isinstance(exc_val, (KeyboardInterrupt, InterruptedError)):
                self._interrupted = True
                if self._exit_code is None:
                    self._exit_code = 130
            else:
                self._succeeded = False
                if self._exit_code is None:
                    self._exit_code = 1

        # Never delete user-supplied directories.
        if not self.is_owned_temp or self._owned_temp_dir is None:
            return False

        should_keep = self.keep_requested or (not self._succeeded and not self.clean_on_failure)
        if should_keep:
            status = "interrupted" if self._interrupted else ("success-kept" if self._succeeded else "failed")
            self._write_status(status)
            print(f"[SGE-EVIDENCE] Preserved ({status}) in {self._owned_temp_dir}", flush=True)
            return False

        if self._is_safe_owned_temp_dir(self._owned_temp_dir):
            shutil.rmtree(self._owned_temp_dir, ignore_errors=True)
        return False
