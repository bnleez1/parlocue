# Recite

![Recite](screenshot.png)

Turn audio scripts for **Listening I** and **TOEFL** practice into spoken `.mp3` files using Clipchamp's Microsoft Neural Voices (`edge-tts`) — without copying lines one by one or stitching clips manually in Kdenlive.

## Features

- **Same Clipchamp Voices**: Browse, filter, favorite, and audition Microsoft Neural Voices (`Jenny`, `Guy`, `Aria`, `Davis`, `Sonia`, `Ryan`, etc.) across locales and accents.
- **Lectures & Multi-Speaker Dialogues**: Paste raw scripts (`Narrator:`, `Student:`, `Advisor:`, `[pause 1.5s]`) or write line-by-line with automatic speaker voice assignment.
- **Voice, Speed, Pitch & Pauses**: Adjust rate (`-50%` to `+100%`), pitch (`-50Hz` to `+50Hz`), and pause duration per speaker or per line.
- **Instant Caching & Preview**: Preview individual lines or the full stitched script with live line highlighting.
- **Export for Direct Use or Kdenlive**: Export a single combined `.mp3` with exact pauses (plus optional `-16 LUFS` loudness normalization) and/or numbered per-line `.mp3` clips (`001_Narrator.mp3`, `002_Student.mp3`, …).
- **Save & Reopen Projects**: Save script projects as `.recite` files or import `.txt` scripts.

## Keyboard Shortcuts

| Shortcut | Action |
| --- | --- |
| `Space` / `Ctrl+Space` | Play or stop full script preview |
| `Ctrl+Enter` | Play selected line |
| `Ctrl+I` | Paste or import a script |
| `Ctrl+L` | Add a new line after selection |
| `Ctrl+N` | New project |
| `Ctrl+O` | Open `.recite` project or `.txt` script |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save As |
| `Ctrl+E` | Export combined `.mp3` or individual clips |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / Redo |
| `Esc` | Cancel active generation or stop playback |
| `?` / `F1` | Show keyboard shortcuts |
| `Q` / `Ctrl+Q` | Quit |

## Build, Test & Install

```sh
./bin/build
./bin/test
./bin/install
```

## CLI & Agent Usage

```sh
recite check [script.recite] --json
recite voices --locale en-US --json
recite export lesson.recite --output lesson.mp3 --clips-dir ./clips --json
recite skill install
```
