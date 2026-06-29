#!/usr/bin/env python3
"""Pi tape control daemon: queue Side A/B, HTTP play/stop/status."""
from __future__ import annotations

import json
import os
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Any
from urllib.parse import urlparse

CONFIG_PATH = os.environ.get("PI_TAPE_CONFIG", "/etc/pi-tape/config.env")
PI_TAPE_ROOT = Path("/srv/pi-tape")
INBOX = PI_TAPE_ROOT / "inbox"
WORK = PI_TAPE_ROOT / "work"
STATUS_PATH = Path("/var/lib/pi-tape/status.json")
QUEUE_NAME = "tape_queue.json"
HTTP_PORT = 8765
ALSA_DEVICE = ""
AUDIO_PIPELINE_WARMUP_SEC = 1.0

_play_lock = threading.Lock()
_play_proc: subprocess.Popen | None = None
_play_thread: threading.Thread | None = None
_play_prepare_thread: threading.Thread | None = None
_play_ffmpeg_proc: subprocess.Popen | None = None
_stop_requested = False
_duration_cache: dict[str, tuple[float, float]] = {}
_prepared_meta: dict[str, tuple[float, float]] = {}
_cached_status: dict[str, Any] | None = None
_queue_cache_mtime: float = -1.0


def load_config() -> None:
    global PI_TAPE_ROOT, INBOX, WORK, HTTP_PORT, ALSA_DEVICE
    if not Path(CONFIG_PATH).is_file():
        return
    for line in Path(CONFIG_PATH).read_text().splitlines():
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        key, val = line.split("=", 1)
        key, val = key.strip(), val.strip()
        if key == "PI_TAPE_ROOT":
            PI_TAPE_ROOT = Path(val)
            INBOX = PI_TAPE_ROOT / "inbox"
            WORK = PI_TAPE_ROOT / "work"
        elif key == "CONTROL_PORT":
            HTTP_PORT = int(val)
        elif key == "ALSA_DEVICE":
            ALSA_DEVICE = val


def ffprobe_duration(path: Path) -> float:
    try:
        out = subprocess.check_output(
            [
                "ffprobe",
                "-v",
                "error",
                "-show_entries",
                "format=duration",
                "-of",
                "csv=p=0",
                str(path),
            ],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        return float(out) if out else 0.0
    except (subprocess.CalledProcessError, ValueError):
        return 0.0


def prepare_side_wav(source: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(
        [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-y",
            "-i",
            str(source),
            "-ac",
            "2",
            "-ar",
            "44100",
            "-c:a",
            "pcm_s16le",
            str(dest),
        ],
        check=True,
    )


def read_queue() -> dict[str, Any]:
    queue_path = INBOX / QUEUE_NAME
    if not queue_path.is_file():
        return {}
    try:
        return json.loads(queue_path.read_text())
    except json.JSONDecodeError:
        return {}


def side_source_path(side_key: str) -> Path | None:
    queue = read_queue()
    entry = queue.get(side_key)
    if not entry:
        return None
    rel = entry.get("remote_path", "")
    if rel.startswith("/"):
        rel = rel.lstrip("/")
    path = PI_TAPE_ROOT / rel
    return path if path.is_file() else None


def cached_ffprobe_duration(path: Path) -> float:
    if not path.is_file():
        return 0.0
    key = str(path.resolve())
    mtime = path.stat().st_mtime
    cached = _duration_cache.get(key)
    if cached is not None and cached[0] == mtime:
        return cached[1]
    duration = ffprobe_duration(path)
    _duration_cache[key] = (mtime, duration)
    return duration


def build_status(*, refresh_queue: bool = True) -> dict[str, Any]:
    global _cached_status, _queue_cache_mtime

    queue_path = INBOX / QUEUE_NAME
    queue_mtime = queue_path.stat().st_mtime if queue_path.is_file() else 0.0
    if queue_mtime != _queue_cache_mtime:
        refresh_queue = True
        _queue_cache_mtime = queue_mtime

    if not refresh_queue and _cached_status is not None:
        status = json.loads(json.dumps(_cached_status))
        if STATUS_PATH.is_file():
            try:
                saved = json.loads(STATUS_PATH.read_text())
                if isinstance(saved.get("playback"), dict):
                    status["playback"] = saved["playback"]
            except json.JSONDecodeError:
                pass
        return status

    queue = read_queue()
    status: dict[str, Any] = {
        "queue_job_id": queue.get("job_id", ""),
        "project_name": queue.get("project_name", ""),
        "playback": {"state": "idle", "side": "", "position_sec": 0.0, "duration_sec": 0.0},
        "queue": {},
    }

    if STATUS_PATH.is_file():
        try:
            saved = json.loads(STATUS_PATH.read_text())
            if isinstance(saved.get("playback"), dict):
                status["playback"] = saved["playback"]
        except json.JSONDecodeError:
            pass

    for side_key, label in (("side_a", "A"), ("side_b", "B")):
        entry = queue.get(side_key) or {}
        src = side_source_path(side_key)
        ready = src is not None
        duration = cached_ffprobe_duration(src) if ready and src else 0.0
        status["queue"][side_key] = {
            "ready": ready,
            "side": label,
            "filename": src.name if src else entry.get("filename", ""),
            "duration_sec": duration,
            "played": bool(entry.get("played", False)),
        }

    _cached_status = json.loads(json.dumps(status))
    return status


def invalidate_status_cache() -> None:
    global _cached_status
    _cached_status = None


def write_status(playback: dict[str, Any]) -> None:
    STATUS_PATH.parent.mkdir(parents=True, exist_ok=True)
    payload = build_status(refresh_queue=False)
    payload["playback"] = playback
    STATUS_PATH.write_text(json.dumps(payload, indent=2))
    if _cached_status is not None:
        _cached_status["playback"] = json.loads(json.dumps(playback))


def mark_side_played(side: str) -> None:
    queue_path = INBOX / QUEUE_NAME
    if not queue_path.is_file():
        return
    queue = read_queue()
    key = "side_a" if side == "A" else "side_b"
    if key in queue:
        queue[key]["played"] = True
        queue_path.write_text(json.dumps(queue, indent=2))
    invalidate_status_cache()


def ensure_prepared(side: str, src: Path) -> tuple[Path, float]:
    prepared = WORK / f"ready_side_{side.lower()}.wav"
    mtime = src.stat().st_mtime
    cached = _prepared_meta.get(side)
    if cached and cached[0] == mtime and prepared.is_file():
        return prepared, cached[1]

    prepare_side_wav(src, prepared)
    duration = ffprobe_duration(prepared)
    _prepared_meta[side] = (mtime, duration)
    return prepared, duration


def _start_offset_for_side(side: str, requested: float | None) -> float:
    if requested is not None:
        return max(0.0, float(requested))

    status = build_status(refresh_queue=False)
    playback = status.get("playback") or {}
    if playback.get("state") == "stopped" and str(playback.get("side", "")).upper() == side:
        return max(0.0, float(playback.get("position_sec") or 0.0))
    return 0.0


def _playback_loop(prepared: Path, side: str, duration: float, start_offset: float = 0.0) -> None:
    global _play_proc, _play_ffmpeg_proc, _stop_requested

    if start_offset > 0.05:
        ffmpeg_cmd = [
            "ffmpeg",
            "-hide_banner",
            "-loglevel",
            "error",
            "-ss",
            str(start_offset),
            "-i",
            str(prepared),
            "-f",
            "s16le",
            "-ac",
            "2",
            "-ar",
            "44100",
            "pipe:1",
        ]
        aplay_cmd = ["aplay"]
        if ALSA_DEVICE:
            aplay_cmd.extend(["-D", ALSA_DEVICE])
        aplay_cmd.extend(["-r", "44100", "-f", "S16_LE", "-c", "2", "-"])

        _play_ffmpeg_proc = subprocess.Popen(
            ffmpeg_cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL
        )
        _play_proc = subprocess.Popen(
            aplay_cmd,
            stdin=_play_ffmpeg_proc.stdout,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if _play_ffmpeg_proc.stdout is not None:
            _play_ffmpeg_proc.stdout.close()
    else:
        _play_ffmpeg_proc = None
        cmd = ["aplay"]
        if ALSA_DEVICE:
            cmd.extend(["-D", ALSA_DEVICE])
        cmd.append(str(prepared))
        _play_proc = subprocess.Popen(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

    deadline = time.monotonic() + AUDIO_PIPELINE_WARMUP_SEC
    while time.monotonic() < deadline:
        if _stop_requested or (_play_proc.poll() is not None):
            if _play_proc.poll() is None:
                _play_proc.terminate()
            if _play_ffmpeg_proc is not None and _play_ffmpeg_proc.poll() is None:
                _play_ffmpeg_proc.terminate()
            write_status(
                {
                    "state": "stopped",
                    "side": side,
                    "position_sec": start_offset,
                    "duration_sec": duration,
                }
            )
            return
        time.sleep(0.05)

    playback = {
        "state": "playing",
        "side": side,
        "position_sec": start_offset,
        "duration_sec": duration,
    }
    write_status(playback)

    start = time.monotonic()
    while _play_proc.poll() is None:
        if _stop_requested:
            _play_proc.terminate()
            try:
                _play_proc.wait(timeout=2)
            except subprocess.TimeoutExpired:
                _play_proc.kill()
            if _play_ffmpeg_proc is not None and _play_ffmpeg_proc.poll() is None:
                _play_ffmpeg_proc.terminate()
            position = min(start_offset + time.monotonic() - start, duration)
            playback = {
                "state": "stopped",
                "side": side,
                "position_sec": position,
                "duration_sec": duration,
            }
            write_status(playback)
            return

        position = min(start_offset + time.monotonic() - start, duration)
        playback["position_sec"] = position
        write_status(playback)
        time.sleep(0.05)

    if _play_ffmpeg_proc is not None and _play_ffmpeg_proc.poll() is None:
        _play_ffmpeg_proc.wait(timeout=1)

    if _stop_requested:
        position = min(start_offset + time.monotonic() - start, duration)
        playback = {
            "state": "stopped",
            "side": side,
            "position_sec": position,
            "duration_sec": duration,
        }
    else:
        playback = {
            "state": "finished",
            "side": side,
            "position_sec": duration,
            "duration_sec": duration,
        }
        mark_side_played(side)
    write_status(playback)


def _prepare_and_play(side: str, offset_sec: float | None) -> None:
    global _play_proc, _play_ffmpeg_proc, _stop_requested

    key = "side_a" if side == "A" else "side_b"
    src = side_source_path(key)
    if src is None:
        write_status({"state": "idle", "side": "", "position_sec": 0.0, "duration_sec": 0.0})
        return

    write_status(
        {
            "state": "starting",
            "side": side,
            "position_sec": 0.0,
            "duration_sec": 0.0,
        }
    )

    if _stop_requested:
        return

    try:
        prepared, duration = ensure_prepared(side, src)
    except subprocess.CalledProcessError:
        write_status({"state": "idle", "side": "", "position_sec": 0.0, "duration_sec": 0.0})
        return

    if _stop_requested:
        return

    start_offset = _start_offset_for_side(side, offset_sec)
    if start_offset >= duration - 0.05:
        start_offset = 0.0

    write_status(
        {
            "state": "starting",
            "side": side,
            "position_sec": start_offset,
            "duration_sec": duration,
        }
    )

    _playback_loop(prepared, side, duration, start_offset)


def start_play(side: str, offset_sec: float | None = None) -> tuple[bool, str]:
    global _play_thread, _play_prepare_thread, _play_proc, _play_ffmpeg_proc, _stop_requested
    side = side.upper()
    if side not in ("A", "B"):
        return False, "Invalid side"

    with _play_lock:
        if _play_proc is not None and _play_proc.poll() is None:
            return False, "Already playing"

        if _play_prepare_thread is not None and _play_prepare_thread.is_alive():
            return False, "Already starting"

        key = "side_a" if side == "A" else "side_b"
        if side_source_path(key) is None:
            return False, f"Side {side} not ready on Pi"

        _stop_requested = False
        write_status(
            {
                "state": "starting",
                "side": side,
                "position_sec": 0.0,
                "duration_sec": 0.0,
            }
        )

        _play_prepare_thread = threading.Thread(
            target=_prepare_and_play,
            args=(side, offset_sec),
            daemon=True,
        )
        _play_prepare_thread.start()
        return True, "ok"


def stop_play() -> tuple[bool, str]:
    global _stop_requested, _play_proc, _play_ffmpeg_proc
    with _play_lock:
        _stop_requested = True

        try:
            status = build_status(refresh_queue=False)
            pb = dict(status.get("playback") or {})
        except Exception:
            pb = {}

        if _play_proc is not None and _play_proc.poll() is None:
            _play_proc.terminate()
            try:
                _play_proc.wait(timeout=0.3)
            except subprocess.TimeoutExpired:
                _play_proc.kill()

        if _play_ffmpeg_proc is not None and _play_ffmpeg_proc.poll() is None:
            _play_ffmpeg_proc.terminate()
            try:
                _play_ffmpeg_proc.wait(timeout=0.3)
            except subprocess.TimeoutExpired:
                _play_ffmpeg_proc.kill()

        if pb.get("state") in ("playing", "starting"):
            pb["state"] = "stopped"
            write_status(pb)

    return True, "ok"


def cleanup_session() -> tuple[bool, str]:
    global _play_prepare_thread, _play_thread, _prepared_meta, _duration_cache, _cached_status, _queue_cache_mtime

    stop_play()

    with _play_lock:
        if _play_prepare_thread is not None and _play_prepare_thread.is_alive():
            _play_prepare_thread.join(timeout=2.0)
        _play_prepare_thread = None
        if _play_thread is not None and _play_thread.is_alive():
            _play_thread.join(timeout=1.0)
        _play_thread = None

    removed = 0
    for directory in (INBOX, WORK):
        if not directory.is_dir():
            continue
        for path in directory.iterdir():
            if not path.is_file():
                continue
            try:
                path.unlink()
                removed += 1
            except OSError:
                pass

    _prepared_meta.clear()
    _duration_cache.clear()
    invalidate_status_cache()
    write_status({"state": "idle", "side": "", "position_sec": 0.0, "duration_sec": 0.0})
    return True, f"removed {removed} files"


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt: str, *args: Any) -> None:
        print(f"[pi-tape-daemon] {self.address_string()} {fmt % args}")

    def _json_response(self, code: int, payload: dict[str, Any]) -> None:
        body = json.dumps(payload).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlparse(self.path).path
        if path == "/api/status":
            self._json_response(200, build_status(refresh_queue=False))
            return
        self._json_response(404, {"error": "not found"})

    def do_POST(self) -> None:
        path = urlparse(self.path).path
        length = int(self.headers.get("Content-Length", 0))
        raw = self.rfile.read(length).decode("utf-8") if length else "{}"
        try:
            body = json.loads(raw) if raw else {}
        except json.JSONDecodeError:
            self._json_response(400, {"error": "invalid json"})
            return

        if path == "/api/play":
            offset = body.get("offset_sec")
            offset_sec = float(offset) if offset is not None else None
            ok, msg = start_play(str(body.get("side", "")), offset_sec)
            self._json_response(200 if ok else 409, {"success": ok, "message": msg})
            return
        if path == "/api/stop":
            ok, msg = stop_play()
            self._json_response(200, {"success": ok, "message": msg})
            return
        if path == "/api/cleanup":
            ok, msg = cleanup_session()
            self._json_response(200, {"success": ok, "message": msg})
            return
        self._json_response(404, {"error": "not found"})


def main() -> None:
    load_config()
    for d in (INBOX, WORK, STATUS_PATH.parent):
        d.mkdir(parents=True, exist_ok=True)
    write_status({"state": "idle", "side": "", "position_sec": 0.0, "duration_sec": 0.0})
    server = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), Handler)
    print(f"[pi-tape-daemon] listening on :{HTTP_PORT}, inbox={INBOX}")
    server.serve_forever()


if __name__ == "__main__":
    main()
