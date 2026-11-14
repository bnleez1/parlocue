# Recite

![Recite](screenshot.png)

Turn audio scripts for **Listening I** and **TOEFL** practice into spoken `.mp3` files using Clipchamp's Microsoft Neural Voices (`edge-tts`) — without copying lines one by one or stitching clips manually in Kdenlive.

Built using Vibe Code on Omarchy OS.

---

## Features

- **Same Clipchamp Voices**: Browse, filter, favorite, and audition Microsoft Neural Voices (`Jenny`, `Guy`, `Aria`, `Davis`, `Sonia`, `Ryan`, etc.) across locales and accents.
- **Lectures & Multi-Speaker Dialogues**: Paste raw scripts (`Narrator:`, `Student:`, `Advisor:`, `[pause 1.5s]`) or write line-by-line with automatic speaker voice assignment.
- **Voice, Speed, Pitch & Pauses**: Adjust rate (`-50%` to `+100%`), pitch (`-50Hz` to `+50Hz`), and pause duration per speaker or per line.
- **Instant Caching & Preview**: Preview individual lines or the full stitched script with live line highlighting.
- **Export for Direct Use or Kdenlive**: Export a single combined `.mp3` with exact pauses (plus optional `-16 LUFS` loudness normalization) and/or numbered per-line `.mp3` clips (`001_Narrator.mp3`, `002_Student.mp3`, …).
- **Save & Reopen Projects**: Save script projects as `.recite` files or import `.txt` scripts.

---

## Prerequisites

Before installing and running this application, ensure your system meets the following requirements:

- **Operating System:** Omarchy OS (or compatible Arch-based Linux distribution)
- **Runtime Dependencies:** `qt6-base`, `qt6-declarative`, `qt6-multimedia`, `qt6-wayland`, `ffmpeg`, `python`, `xdg-desktop-portal` (plus the `edge-tts` Python package, which can also be installed from within the app)
- **Build Dependencies:** `gcc`, `make`, `qmake6` (included in `qt6-base`)
- **Git**

---

## Installation

1. Clone the repository:

```bash
git clone https://github.com/bnleez1/recite.git
cd recite
```

2. Build and run tests:

```bash
./bin/build
./bin/test
```

3. Install the Arch package (`pkgbuild/PKGBUILD`):

```bash
./bin/install
```

---

## Usage

Launch the desktop application from your app launcher or terminal (`recite`), or run the built binary directly (`./build/recite`).

See the full **[User Manual](docs/USER_MANUAL.md)** for a step-by-step guide to script formatting, speaker/voice controls, line overrides, MP3 exporting, and troubleshooting.

### Keyboard Shortcuts

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
| `?` / `F1` | Show keyboard shortcuts |
| `Q` / `Ctrl+Q` | Quit |

### CLI & Agent Usage

```sh
recite check [script.recite] --json
recite voices --locale en-US --json
recite export lesson.recite --output lesson.mp3 --clips-dir ./clips --json
recite skill install
```

---

## Contributing

Contributions are welcome! To keep the project aligned with its core goals, direct push access to the `main` branch is restricted. Please follow the pull request workflow:

1. Fork the repository.
2. Create a new feature branch (`git checkout -b feature/your-feature-name`).
3. Commit your changes (`git commit -m "Add description of your changes"`).
4. Push to your branch (`git push origin feature/your-feature-name`).
5. Open a Pull Request for review and testing.

---

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
