#!/usr/bin/env python3
"""Bridge between ParloCue (C++/Qt) and Microsoft Neural TTS via edge-tts."""

import argparse
import asyncio
import glob
import json
import os
import sys


def _attach_known_venvs():
    home = os.path.expanduser("~")
    patterns = [
        os.path.join(home, ".local/share/parlocue/venv/lib/python*/site-packages"),
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


def cmd_extract_docx(path):
    import zipfile
    import xml.etree.ElementTree as ET

    w_ns = "http://schemas.openxmlformats.org/wordprocessingml/2006/main"
    p_tag = f"{{{w_ns}}}p"
    t_tag = f"{{{w_ns}}}t"
    tab_tag = f"{{{w_ns}}}tab"
    br_tag = f"{{{w_ns}}}br"
    cr_tag = f"{{{w_ns}}}cr"

    with zipfile.ZipFile(path, "r") as zf:
        if "word/document.xml" not in zf.namelist():
            raise ValueError("Invalid Word document: missing word/document.xml")
        xml_bytes = zf.read("word/document.xml")

    root = ET.fromstring(xml_bytes)
    paragraphs = []
    for p in root.iter(p_tag):
        parts = []
        for elem in p.iter():
            if elem.tag == t_tag and elem.text:
                parts.append(elem.text)
            elif elem.tag == tab_tag:
                parts.append("\t")
            elif elem.tag in (br_tag, cr_tag):
                parts.append("\n")
        para = "".join(parts).strip()
        if para:
            paragraphs.append(para)

    sys.stdout.write("\n\n".join(paragraphs))


def main():
    parser = argparse.ArgumentParser(description="ParloCue TTS bridge")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("check")
    sub.add_parser("voices")

    synth = sub.add_parser("synthesize")
    synth.add_argument("--text-file", required=True)
    synth.add_argument("--voice", required=True)
    synth.add_argument("--rate", default="+0%")
    synth.add_argument("--pitch", default="+0Hz")
    synth.add_argument("--out", required=True)

    extract_docx = sub.add_parser("extract-docx")
    extract_docx.add_argument("--file", required=True)

    args = parser.parse_args()

    if args.command == "extract-docx":
        try:
            cmd_extract_docx(args.file)
        except Exception as exc:
            print(str(exc), file=sys.stderr)
            return 1
        return 0

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
