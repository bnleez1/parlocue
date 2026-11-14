#!/usr/bin/env python3
"""Bridge between Recite (C++/Qt) and Microsoft Neural TTS via edge-tts."""

import argparse
import asyncio
import glob
import json
import os
import sys


def _attach_known_venvs():
    home = os.path.expanduser("~")
    patterns = [
        os.path.join(home, ".local/share/recite/venv/lib/python*/site-packages"),
        os.path.join(home, ".local/share/pipx/venvs/edge-tts/lib/python*/site-packages"),
    ]
    for pattern in patterns:
        for path in glob.glob(pattern):
            if path not in sys.path:
                sys.path.insert(0, path)


_attach_known_venvs()


def _import_edge_tts():
    try:
        import edge_tts
        return edge_tts, None
    except ImportError as exc:
        return None, str(exc)


async def cmd_voices(edge_tts):
    voices = await edge_tts.list_voices()
    result = []
    for v in voices:
        tag = v.get("VoiceTag") or {}
        result.append({
            "shortName": v.get("ShortName", ""),
            "name": v.get("Name", ""),
            "friendlyName": v.get("FriendlyName", ""),
            "locale": v.get("Locale", ""),
            "gender": v.get("Gender", ""),
            "categories": tag.get("ContentCategories") or [],
            "personalities": tag.get("VoicePersonalities") or [],
        })
    print(json.dumps(result, ensure_ascii=False))


async def cmd_synthesize(edge_tts, text_file, voice, rate, pitch, out_path):
    with open(text_file, "r", encoding="utf-8") as f:
        text = f.read().strip()
    if not text:
        raise ValueError("Cannot synthesize empty text")

    tmp_path = out_path + ".part"
    communicate = edge_tts.Communicate(text, voice, rate=rate, pitch=pitch)
    await communicate.save(tmp_path)
    if not os.path.exists(tmp_path) or os.path.getsize(tmp_path) == 0:
        raise RuntimeError("edge-tts produced an empty audio file")
    os.replace(tmp_path, out_path)


def main():
    parser = argparse.ArgumentParser(description="Recite TTS bridge")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("check")
    sub.add_parser("voices")

    synth = sub.add_parser("synthesize")
    synth.add_argument("--text-file", required=True)
    synth.add_argument("--voice", required=True)
    synth.add_argument("--rate", default="+0%")
    synth.add_argument("--pitch", default="+0Hz")
    synth.add_argument("--out", required=True)

    args = parser.parse_args()
    edge_tts, err = _import_edge_tts()

    if args.command == "check":
        if edge_tts is None:
            print(json.dumps({"ok": False, "error": err}))
            return 1
        version = getattr(edge_tts, "__version__", "unknown")
        print(json.dumps({"ok": True, "version": version}))
        return 0

    if edge_tts is None:
        print(f"edge-tts Python package not found: {err}", file=sys.stderr)
        return 2

    try:
        if args.command == "voices":
            asyncio.run(cmd_voices(edge_tts))
        elif args.command == "synthesize":
            asyncio.run(cmd_synthesize(
                edge_tts, args.text_file, args.voice, args.rate, args.pitch, args.out
            ))
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
