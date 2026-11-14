---
name: recite
description: Generate multi-voice spoken MP3 audio and individual speaker clips from scripts using Recite and Microsoft Neural Voices (edge-tts).
---

# Recite CLI

Use `recite` headlessly to validate scripts, list voices, and export combined MP3 files or per-line clips for Kdenlive.

## Commands

- `recite check [script.recite|script.txt] --json`
  Verify `edge-tts` and `ffmpeg` availability and optionally validate a script file.
- `recite voices [--locale en-US] --json`
  List available Microsoft Neural Voices (same voices as Clipchamp).
- `recite export <script.recite|script.txt> --output <out.mp3> [--clips-dir <dir>] [--bitrate 192] [--normalize] --json`
  Synthesize all lines, insert configured pauses, and export a combined `.mp3` and/or numbered per-line `.mp3` clips.
- `recite open [file]`
  Open the desktop GUI.
