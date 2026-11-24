# light_md

**English** | [Español](README.es.md) | [Português](README.pt.md) | [Français](README.fr.md) | [Deutsch](README.de.md)

A lightweight Markdown editor in C++ and Qt 6, for everyone.

Write on the left, see the formatted result on the right. Format with the buttons at the top — no Markdown required. Prefer a quieter layout? Hide the buttons with **Ctrl+Shift+B**, or use **Options** (every action has a shortcut).

![light_md: the writing pane on the left and the formatted result on the right](docs/screenshot.png)

**[Download light_md for Windows](https://github.com/josel120/light_md/releases/latest)**: unzip it and open `bin\light_md.exe`. Nothing to install.

## What it does

- **Classic layout by default**: file and format button bars plus write + preview panes.
- Toggle **Show buttons** (**Ctrl+Shift+B**) for a minimal Options-only chrome.
- Optional **Reader** mode (**Ctrl+E** / **Ctrl+R** / **Esc**) hides the writing pane; independent of the button bars.
- Button visibility and the last Edit/Reader view are remembered across restarts and when opening another file.
- **Options** menu in everyday words: New, Open, Save, Format, Theme, Help — every item has a shortcut.
- Open and save `.md` documents, also by dragging them onto the window or using "Open with" from the file manager.
- Export to PDF and print: the result looks just like the reading pane.
- Word count and an unsaved-changes warning, including when closing the window.
- Themes: **System** (desktop default), **Minimal** (quiet light look), **Omarchy** (live desktop theme on Omarchy Linux only). Windows and macOS keep **System** by default.

## Keyboard shortcuts

| Action | Shortcut |
| --- | --- |
| Show / hide buttons | Ctrl+Shift+B |
| Edit document | Ctrl+E |
| Read only | Ctrl+R or Esc |
| New / Open / Save / Save as | Ctrl+N / Ctrl+O / Ctrl+S / Ctrl+Shift+S |
| Export PDF / Print | Ctrl+Shift+E / Ctrl+P |
| Undo / Redo | Ctrl+Z / Ctrl+Shift+Z (or Ctrl+Y) |
| Bold / Italic / Link | Ctrl+B / Ctrl+I / Ctrl+K |
| Big title / Subtitle | Ctrl+1 / Ctrl+2 |
| Bullet / Numbered / Quote | Ctrl+Shift+8 / Ctrl+Shift+7 / Ctrl+Shift+. |
| Theme System / Minimal / Omarchy | Ctrl+Shift+1 / Ctrl+Shift+2 / Ctrl+Shift+3 |
| Help | F1 |

On macOS, use **Cmd** instead of Ctrl where the platform expects it (standard Qt shortcuts).

## Build on Linux

Needs Qt 6 development packages (`Widgets` and `PrintSupport`). On Arch / [Omarchy](https://omarchy.org/):

```bash
sudo pacman -S --needed base-devel cmake ninja qt6-base qt6-wayland
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/light_md
```

Install for your user (binary, `.desktop` entry, and icon):

```bash
cmake --install build --prefix ~/.local
```

Or use `packaging/arch/PKGBUILD` as a release template (the `v1.1.0` tag and real `sha256sums` are still pending).

On first launch under Omarchy, the theme defaults to **Omarchy** and updates when you run `omarchy theme set`. Elsewhere it defaults to **System**. Your choice is remembered.

## Build on Windows (Qt 6 and MinGW)

You need Qt 6 installed (for example in `C:\Qt`) with its CMake and Ninja.

```powershell
$env:PATH = "C:\Qt\Tools\mingw1510_64\bin;C:\Qt\6.12.1\mingw_64\bin;$env:PATH"
C:\Qt\Tools\CMake_64\bin\cmake.exe -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=C:/Qt/6.12.1/mingw_64
C:\Qt\Tools\CMake_64\bin\cmake.exe --build build
.\build\light_md.exe
```

Adjust the paths if your Qt or MinGW version is different.

## Take it to another Windows computer

To create a folder that runs on any Windows PC without installing Qt:

```powershell
C:\Qt\Tools\CMake_64\bin\cmake.exe --install build --prefix dist
```

Copy the whole `dist` folder and open `dist\bin\light_md.exe`.

## License

MIT

Omarchy theme watching follows the approach used in Omacut (MIT, David Heinemeier Hansson).
