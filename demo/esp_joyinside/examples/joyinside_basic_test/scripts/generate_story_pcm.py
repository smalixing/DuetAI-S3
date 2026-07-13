#!/usr/bin/env python3
"""Generate story.pcm for joyinside_basic_test SPIFFS image.

Requires: pip install edge-tts
          ffmpeg in PATH

Output: ../spiffs_image/story.pcm
Format: 16 kHz, mono, signed 16-bit little-endian PCM
"""

from __future__ import annotations

import argparse
import asyncio
import shutil
import subprocess
import sys
from pathlib import Path

DEFAULT_TEXT = "讲一个故事"
DEFAULT_VOICE = "zh-CN-XiaoxiaoNeural"
SAMPLE_RATE = 16000


async def synthesize_mp3(text: str, voice: str, mp3_path: Path) -> None:
    import edge_tts

    communicate = edge_tts.Communicate(text=text, voice=voice)
    await communicate.save(str(mp3_path))


def convert_to_pcm(mp3_path: Path, pcm_path: Path) -> None:
    if shutil.which("ffmpeg") is None:
        raise RuntimeError("ffmpeg not found in PATH")

    subprocess.run(
        [
            "ffmpeg",
            "-y",
            "-i",
            str(mp3_path),
            "-ar",
            str(SAMPLE_RATE),
            "-ac",
            "1",
            "-f",
            "s16le",
            str(pcm_path),
        ],
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--text", default=DEFAULT_TEXT, help="TTS text")
    parser.add_argument("--voice", default=DEFAULT_VOICE, help="edge-tts voice name")
    parser.add_argument(
        "--output",
        type=Path,
        default=Path(__file__).resolve().parent.parent / "spiffs_image" / "story.pcm",
        help="Output PCM path",
    )
    args = parser.parse_args()

    try:
        import edge_tts  # noqa: F401
    except ImportError:
        print("Please install edge-tts first: pip install edge-tts", file=sys.stderr)
        return 1

    args.output.parent.mkdir(parents=True, exist_ok=True)
    mp3_path = args.output.with_suffix(".mp3")

    print(f"TTS text: {args.text}")
    print(f"Voice: {args.voice}")
    asyncio.run(synthesize_mp3(args.text, args.voice, mp3_path))
    convert_to_pcm(mp3_path, args.output)
    mp3_path.unlink(missing_ok=True)

    size = args.output.stat().st_size
    duration_ms = size * 1000 // (SAMPLE_RATE * 2)
    print(f"Wrote {args.output} ({size} bytes, ~{duration_ms} ms)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
