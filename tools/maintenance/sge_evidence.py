#!/usr/bin/env python3
"""Reusable temporary evidence directory manager for SYSTALIA diagnostic tools."""

from __future__ import annotations

import fcntl
import json
import os
import pathlib
import shutil
import signal
import tempfile
import time
from typing import Optional

MANAGED_BY = "sge-evidence-v1"
LOCK_FILENAME = ".sge-run.lock"
STATUS_FILENAME = "run-status.json"


def default_runs_root() -> pathlib.Path:
    tmpdir = os.environ.get("TMPDIR") or tempfile.gettempdir() or "/tmp"
    return pathlib.Path(tmpdir).resolve() / "systalia-runs"


def _is_direct_child_non_symlink(entry: pathlib.Path, runs_root: pathlib.Path) -> bool:
    if entry.is_symlink():
        return False
    try:
        resolved_entry = entry.resolve()
        resolved_root = runs_root.resolve()
        rel = resolved_entry.relative_to(resolved_root)
        return len(rel.parts) == 1
    except (OSError, ValueError):
        return False


def _try_acquire_exclusive_lock(lock_path: pathlib.Path) -> Optional[int]:
    """Acquire non-blocking exclusive flock if lock_path exists or can be opened.
    Returns open fd if acquired, or None if held by another active process.
    """
    try:
        fd = os.open(str(lock_path), os.O_CREAT | os.O_RDWR, 0o600)
    except OSError:
        return None
    try:
        fcntl.flock(fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        return fd
    except OSError:
        os.close(fd)
        return None


def prune_expired_runs(runs_root: Optional[pathlib.Path] = None, max_age_hours: float = 24.0) -> int:
    """Safely prune ONLY expired failed/interrupted managed run directories.

    Preserves:
    - Active runs (holding .sge-run.lock or status == 'active' with held lock)
    - Unknown/historical directories without valid managed_by == 'sge-evidence-v1'
    - Explicitly preserved runs (explicit_keep == True or status == 'success-kept')
    - Symlinks and paths outside runs_root
    """
    root = (runs_root or default_runs_root()).resolve()
    if not root.is_dir() or root.is_symlink():
        return 0
    cutoff = time.time() - max_age_hours * 3600.0
    removed = 0

    for entry in root.iterdir():
        if entry.is_symlink() or not entry.is_dir():
            continue
        if not _is_direct_child_non_symlink(entry, root):
            continue

        status_path = entry / STATUS_FILENAME
        if not status_path.is_file() or status_path.is_symlink():
            # Unknown or historical directory without metadata: preserve.
            continue

        try:
            meta = json.loads(status_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            continue

        if not isinstance(meta, dict) or meta.get("managed_by") != MANAGED_BY:
            continue
        if meta.get("explicit_keep") is True or meta.get("status") == "success-kept":
            continue

        status = meta.get("status")
        if status not in ("failed", "interrupted"):
            # Do not blindly remove active/unknown states unless lock proves dead AND expired;
            # per policy, TTL 24h applies only to terminated failed/interrupted runs.
            continue

        try:
            if entry.stat().st_mtime >= cutoff:
                continue
        except OSError:
            continue

        lock_fd = _try_acquire_exclusive_lock(entry / LOCK_FILENAME)
        if lock_fd is None:
            # Held by an active process: never delete.
            continue
        try:
            shutil.rmtree(entry, ignore_errors=True)
            removed += 1
        finally:
            os.close(lock_fd)

    return removed


class EvidenceRun:
    """Context manager creating a unique run directory with lock-protected lifecycle."""

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
        self._lock_fd: Optional[int] = None
        self._succeeded = False
        self._interrupted = False
        self._exit_code: Optional[int] = None
        self._prev_sigterm = None
        self._prev_sigint = None
        self._created_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())

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
            self._lock_fd = _try_acquire_exclusive_lock(self._owned_temp_dir / LOCK_FILENAME)
            self._write_status("active", explicit_keep=self.keep_requested)

    @property
    def path(self) -> pathlib.Path:
        return self._user_dir if self._user_dir is not None else self._owned_temp_dir  # type: ignore[return-value]

    @property
    def is_owned_temp(self) -> bool:
        return self._owned_temp_dir is not None and self._user_dir is None

    def _is_safe_owned_temp_dir(self, candidate: pathlib.Path) -> bool:
        if not self.is_owned_temp or self._owned_temp_dir is None:
            return False
        if candidate.is_symlink():
            return False
        try:
            resolved = candidate.resolve()
            root_resolved = self.runs_root.resolve()
            rel = resolved.relative_to(root_resolved)
            return len(rel.parts) == 1 and resolved == self._owned_temp_dir
        except (OSError, ValueError):
            return False

    def mark_success(self) -> None:
        self._succeeded = True
        self._exit_code = 0

    def mark_failure(self, exit_code: int = 1) -> None:
        self._succeeded = False
        self._exit_code = exit_code

    def _write_status(self, status: str, *, explicit_keep: bool) -> None:
        if not self.is_owned_temp or self._owned_temp_dir is None:
            return
        target = self._owned_temp_dir / STATUS_FILENAME
        payload = {
            "managed_by": MANAGED_BY,
            "tool": self.tool_name,
            "status": status,
            "pid": os.getpid(),
            "exit_code": self._exit_code,
            "explicit_keep": explicit_keep,
            "created_at": self._created_at,
            "updated_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "retention_hours": self.retention_hours,
        }
        try:
            target.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
        except OSError:
            pass

    def _release_lock(self) -> None:
        if self._lock_fd is not None:
            try:
                fcntl.flock(self._lock_fd, fcntl.LOCK_UN)
            except OSError:
                pass
            try:
                os.close(self._lock_fd)
            except OSError:
                pass
            self._lock_fd = None

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

        if not self.is_owned_temp or self._owned_temp_dir is None:
            self._release_lock()
            return False

        should_keep = self.keep_requested or (not self._succeeded and not self.clean_on_failure)
        if should_keep:
            status = "interrupted" if self._interrupted else ("success-kept" if self._succeeded else "failed")
            self._write_status(status, explicit_keep=self.keep_requested)
            self._release_lock()
            print(f"[SGE-EVIDENCE] Preserved ({status}) in {self._owned_temp_dir}", flush=True)
            return False

        self._release_lock()
        if self._is_safe_owned_temp_dir(self._owned_temp_dir):
            shutil.rmtree(self._owned_temp_dir, ignore_errors=True)
        return False
