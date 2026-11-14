---
name: parlocue
description: Generate multi-voice spoken MP3 audio and individual speaker clips from scripts using ParloCue and Microsoft Neural Voices (edge-tts).
---

# ParloCue CLI

Use `parlocue` headlessly to validate scripts, list voices, and export combined MP3 files or per-line clips for Kdenlive.

## Commands

- `parlocue check [script.parlocue|script.recite|script.txt] --json`
  Verify `edge-tts` and `ffmpeg` availability and optionally validate a script file.
- `parlocue voices [--locale en-US] --json`
  List available Microsoft Neural Voices (same voices as Clipchamp).
- `parlocue export <script.parlocue|script.recite|script.txt> --output <out.mp3> [--clips-dir <dir>] [--bitrate 192] [--normalize] --json`
  Synthesize all lines, insert configured pauses, and export a combined `.mp3` and/or numbered per-line `.mp3` clips.
- `parlocue open [file]`
  Open the desktop GUI.
