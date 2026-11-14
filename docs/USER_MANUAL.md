# Recite User Manual

Recite turns lecture and dialogue scripts into spoken `.mp3` audio using Microsoft Neural Voices (`edge-tts`)—the same neural voices available in Clipchamp—without copying lines one by one or stitching clips manually.

---

## Table of Contents

1. [Getting Started](#getting-started)
2. [Workspace Overview](#workspace-overview)
3. [Writing & Importing Scripts](#writing--importing-scripts)
4. [Speakers, Voices, Speed & Pitch](#speakers-voices-speed--pitch)
5. [Previewing & Caching](#previewing--caching)
6. [Saving & Exporting Audio](#saving--exporting-audio)
7. [Keyboard Shortcuts](#keyboard-shortcuts)
8. [CLI & Agent Usage](#cli--agent-usage)
9. [Files & Troubleshooting](#files--troubleshooting)

---

## Getting Started

### Installing Recite

On Omarchy OS / Arch-based Linux, clone the repository and run the installer:

```bash
git clone https://github.com/bnleez1/recite.git
cd recite
./bin/install
```

This builds the binary, installs `/usr/bin/recite`, and registers the desktop launcher entry so **Recite** appears in your application menu.

### First-Time Voice Engine Setup

Recite requires:
- **`edge-tts`** (Python package) to synthesize Microsoft Neural Voices (internet connection required for synthesis and voice list refresh).
- **`ffmpeg`** (with `ffprobe` and `libmp3lame`) to stitch lines, insert pauses, normalize loudness, and export `.mp3` files.

If `edge-tts` is not detected when you launch Recite, a banner appears at the top of the window with an **Install Voices Engine** button. Clicking it creates a dedicated virtual environment at `~/.local/share/recite/venv` and installs `edge-tts` automatically.

### Launching Recite

- Open **Recite** from the Omarchy app launcher, or
- Launch from a terminal:
  ```bash
  recite
  recite path/to/lesson.recite
  recite path/to/script.txt
  ```

---

## Workspace Overview

Recite uses a two-pane dark studio layout:

1. **Top Toolbar**
   - **Script title**: Click to rename your project. A dot (`●`) next to the title indicates unsaved changes.
   - **Paste Script**, **New**, **Open**, **Save**: Manage `.recite` projects and `.txt` script imports.
   - **Play Full / Stop**: Synthesize (if needed) and preview the entire script from start to finish.
   - **Export MP3…**: Open the audio export dialog.
   - **`?`**: Open the keyboard shortcuts overlay.

2. **Left Pane — Script Timeline**
   - **Sub-bar**:
     - **+ Add Line**: Insert a new blank line after the currently selected line.
     - **Default pause**: Set the baseline pause (`0.0 s` to `15.0 s`, in `0.2 s` steps). Click **Apply to All** to update the pause on every existing line.
     - **Counter**: Live line and word count.
   - **Line Cards**: Each line shows:
     - Line number and a color-coded left strip matching its speaker.
     - **Speaker pill (`▾`)**: Click to reassign the line to any speaker.
     - **Play / Stop (`▶` / `⏹`)**: Preview that individual line.
     - **Cache status dot**:
       - **Gray**: Not yet synthesized/cached.
       - **Accent color**: Currently synthesizing.
       - **Green**: Cached and ready for instant playback/export.
       - **Red**: Synthesis error (error details appear below the line text).
     - **`Custom` badge**: Appears when the line overrides its speaker's speed or pitch.
     - **Pause (`−` / `+`)**: Silence after this line (`0.0 s` to `30.0 s`, in `0.2 s` steps).
     - **Reorder / Delete (`↑`, `↓`, `×`)**: Move the line up/down or remove it.

3. **Right Pane — Speakers & Voices**
   - **Speakers & Voices list**: Select a speaker to edit, click **+ Add** to add another speaker, or click `×` to remove a speaker (at least one speaker is always kept).
   - **Speaker settings card**: Edit the speaker's name, neural voice, speed, pitch, and per-speaker default pause.
   - **Line Override card**: Override speed or pitch solely for the currently selected line.

4. **Bottom Transport / Progress Bar**
   - Appears during synthesis/export (showing progress and a **Cancel** button) or during playback (showing **Play/Stop**, current line/sample label, scrub slider, and elapsed/total time).

---

## Writing & Importing Scripts

You can type lines directly in the timeline or import formatted text via **Paste Script** (`Ctrl+I`) or **Open** (`Ctrl+O` with a `.txt` file).

### Script Format Rules

- **Speaker prefixes**: Start a line with `Name: text` (Markdown bold like `**Narrator:** text` is also supported):
  ```text
  Narrator: Listen to a conversation between a student and an advisor.
  Student: Hi, do you have a minute?
  Advisor: Sure, come on in.
  ```
  *Note: Prefixes such as `http:`, `https:`, `ftp:`, and `note:` are ignored so URLs and notes are not mistaken for speakers.*
- **Multi-line paragraphs & blank lines**:
  - Consecutive lines without a new speaker prefix are joined into the current line.
  - Blank lines split text into separate line cards.
- **Custom pauses**: Put a pause tag on its own line to set the pause duration after the preceding line:
  ```text
  [pause 1.5s]
  ```
  Supported forms include `[pause 2s]`, `(pause: 1.5s)`, `[pause=500ms]`, `[pause 3 sec]`, and `[pause 2.5 seconds]` (clamped between `0` and `30 s`).

### Import Options (**Paste Script** Dialog)

- **Auto-detect speakers (`Name: text`)**: Automatically creates and maps speakers found in the text.
- **Default speaker**: Speaker assigned to lines that have no prefix (or all lines if auto-detection is turned off).
- **Replace current script**: Checked replaces the existing timeline; unchecked appends the imported lines (`Append Lines`).
- A live preview pill shows how many lines and speakers were detected before importing.

---

## Speakers, Voices, Speed & Pitch

### Choosing a Neural Voice

1. Select a speaker in the **Speakers & Voices** list.
2. Click **Change Voice…** to open the **Choose Neural Voice** dialog.
3. Filter voices using:
   - **Search**: Voice name, accent, or personality/style (e.g., `Jenny`, `Guy`, `British`, `Friendly`).
   - **Language / Locale**: `English`, `en-US`, `en-GB`, `en-AU`, or `All Languages`.
   - **Gender**: `All`, `Female`, or `Male`.
   - **Favorites (`★`)**: Star voices to filter down to your go-to cast later.
4. Click **Sample** to audition a voice, or **Use** to assign it to the speaker.
5. Click **Refresh** to fetch the full online Microsoft Neural Voice catalog (Recite includes 18 English voices built in and caches refreshed lists locally).

### Adjusting Delivery

- **Test**: Click **Test** next to **Change Voice…** to hear the speaker's voice with their current speed and pitch.
- **Speed**: `-50%` (`0.50x`) to `+100%` (`2.00x`) in `5%` increments, plus quick buttons for **`-15%`**, **`0%`**, and **`+15%`**.
- **Pitch**: `-50 Hz` to `+50 Hz` in `2 Hz` steps, with a **Reset** button when non-zero.
- **Default pause for speaker**: `0.0 s` to `5.0 s` in `0.1 s` steps (applied when adding new lines for that speaker).
- **Line Override**: Enable **Override speed for this line** or **Override pitch for this line** in the **Line Override** card to fine-tune emphasis on a single line without changing the speaker's global settings.

---

## Previewing & Caching

- **Single line**: Click **`▶`** on a line card or press **`Ctrl+Enter`** to preview the selected line.
- **Full script**: Click **Play Full** or press **`Space`** / **`Ctrl+Space`** to synthesize all uncached lines, stitch them with pauses, and play the full audio while highlighting and scrolling to the active line.
- **Cancel / Stop**: Press **`Esc`** at any time to cancel active synthesis/export or stop playback.
- **Instant caching**: Synthesized clips are cached by `(text, voice, rate, pitch)` under `~/.cache/recite/clips/`. Editing one line only re-synthesizes that line; unchanged lines stay cached (green dot).

---

## Saving & Exporting Audio

### Saving Projects (`.recite`)

- Press **`Ctrl+S`** (or click **Save**) to save your script, speakers, voice settings, pauses, and overrides as a `.recite` JSON project file.
- Press **`Ctrl+Shift+S`** for **Save As**.
- If you try to start a **New** project, **Open** another file, or **Quit** with unsaved changes (`●`), Recite prompts you to save or discard.

### Exporting MP3s

Click **Export MP3…** (or press **`Ctrl+E`**) to open **Export Audio**:

1. **Export Mode**
   - **Combined final `.mp3` (with all pauses)**: Exports a single continuous `.mp3` file with exact silence gaps between lines.
   - **Individual `.mp3` clips per line (for Kdenlive)**: Exports numbered per-line files into a folder (`001_Narrator.mp3`, `002_Student.mp3`, …).
   - **Both combined `.mp3` and individual clips**: Prompts for both output locations and exports both in one pass.
2. **MP3 Bitrate**: `128 kbps`, `192 kbps` (default), or `320 kbps` (encoded at 44.1 kHz stereo via `libmp3lame`).
3. **Normalize loudness (`-14 LUFS`)**: Available when exporting a combined `.mp3`; applies an `ffmpeg` `loudnorm` pass (`I=-14:TP=-1.5:LRA=11`), optimal for YouTube and consistent playback volume.

---

## Keyboard Shortcuts

Press **`?`** or **`F1`** at any time in Recite to view the in-app shortcut overlay.

| Shortcut | Action |
| --- | --- |
| `Space` / `Ctrl+Space` | Play or stop full script preview |
| `Ctrl+Enter` | Play selected line |
| `Ctrl+I` | Paste or import a script |
| `Ctrl+L` | Add a new line after selection |
| `Ctrl+N` | New project |
| `Ctrl+O` | Open `.recite` project or `.txt` script |
| `Ctrl+S` / `Ctrl+Shift+S` | Save / Save As |
| `Ctrl+E` | Export combined MP3 or clips |
| `Ctrl+Z` / `Ctrl+Shift+Z` | Undo / Redo |
| `Esc` | Cancel active audio generation or stop playback |
| `?` / `F1` | Show or hide keyboard shortcuts |
| `Q` / `Ctrl+Q` | Quit Recite |

*(Single-key shortcuts such as `Space`, `?`, `Q`, and timeline `Undo`/`Redo` are automatically paused while typing inside a text field.)*

---

## CLI & Agent Usage

Recite also runs headlessly from the terminal for scripting, batch exports, and AI agent workflows:

```bash
# Verify edge-tts, ffmpeg, and (optionally) validate a script
recite check [script.recite|script.txt] [--json]

# List available Microsoft Neural Voices (optionally filter by locale prefix)
recite voices [--locale en-US] [--json]

# Synthesize and export combined MP3 and/or per-line clips
recite export <file> [--output out.mp3] [--clips-dir dir] [--bitrate 192] [--normalize] [--json]

# Install the bundled agent skill to ~/.agents/skills/recite/SKILL.md
recite skill install

# Open the desktop GUI (optionally loading a file)
recite [open] [file.recite|script.txt]

# Show CLI help or version
recite --help
recite --version
```

---

## Files & Troubleshooting

### Local Data & Cache Paths

- **`~/.cache/recite/clips/`**: Cached per-line synthesized audio clips.
- **`~/.cache/recite/voices.json`**: Cached voice catalog from **Refresh**.
- **`~/.cache/recite/favorites.json`**: Starred favorite voices.
- **`~/.local/share/recite/venv/`**: Managed Python virtual environment for `edge-tts` (if installed via the in-app button).

### Common Issues

- **"edge-tts is not installed yet" banner**: Click **Install Voices Engine** in the banner, or ensure `edge-tts` is installed in Python (`~/.local/share/recite/venv`, `pipx`, or system Python).
- **"ffmpeg / ffprobe not found" banner**: Install `ffmpeg` via your package manager (`sudo pacman -S ffmpeg` on Arch/Omarchy).
- **Line shows a red status dot / synthesis error**: Check your internet connection (`edge-tts` queries Microsoft's online TTS service) and confirm the line text is not empty.
- **Line has no sound when clicking `▶`**: Make sure the line contains spoken text; whitespace-only lines are skipped during playback and export.
