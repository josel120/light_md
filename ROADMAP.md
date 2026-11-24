# Roadmap

What light_md already does, what is coming next, and ideas for later.
Tick a box (`[x]`) and move the line to **Done** when a feature is finished.

## Done

### Version 1.0
- [x] Two panes: writing on the left, instant preview on the right
- [x] Formatting buttons in everyday words (Big title, Subtitle, Bold, Italic, Bullet list, Numbered list, Quote, Link)
- [x] Open and save `.md` files, also by drag and drop and "Open with" from File Explorer
- [x] Export to PDF and print
- [x] Undo and redo, keyboard shortcuts
- [x] Word count and unsaved-changes warning (also when closing the window)
- [x] Help button with a short guide

### Version 1.1 (contributed by @debugsito)
- [x] Linux support: build instructions, `.desktop` entry, icon, Arch `PKGBUILD` template
- [x] Themes: System, Minimal and Omarchy (follows the Omarchy desktop theme live)
- [x] Options menu with every action and its shortcut
- [x] Optional Reader mode (Ctrl+E / Ctrl+R / Esc) and Show buttons toggle (Ctrl+Shift+B), both remembered
- [x] Safe saving with `QSaveFile` (the original is kept if saving fails)

### Version 1.2
- [x] Find and replace (Ctrl+F, Ctrl+H, F3 / Shift+F3, match case, replace all; Esc closes the bar)
- [x] Recent documents menu (last 10, removes files that no longer exist)
- [x] App icon on the window, the taskbar and the `.exe`
- [x] Checklist button (`- [ ]`, Ctrl+Shift+9)
- [x] Insert menu: picture, table, code block, separator line (Ctrl+Shift+P / T / C / L)
- [x] Pictures next to the document show up in the preview and the PDF

### Version 1.3
- [x] macOS app: universal (Apple Silicon and Intel), icon, opens `.md` files from Finder
- [x] GitHub Actions: builds Windows, macOS and Linux on every push and attaches the Windows and macOS packages to each release
- [x] Check for updates button (downloads the right file for your system)

## Next

- [ ] Synchronized scrolling between the editor and the preview
- [ ] Dark theme on Windows and macOS (Omarchy already brings one on Linux)
- [ ] Reopen the last document on start and remember the cursor position
- [ ] Autosave and recovery after a crash
- [ ] Syntax highlighting in the editor (titles bigger, bold in bold, links in color)
- [ ] Adjustable font size (Ctrl + mouse wheel, Ctrl+Plus / Ctrl+Minus)
- [ ] Paste pictures from the clipboard (saved next to the document)

## Later

- [ ] Tabs to keep several documents open
- [ ] Export to HTML (and maybe `.docx`)
- [ ] Interface in Spanish, Portuguese, French and German (`QTranslator`)
- [ ] Spell checker (Hunspell)
- [ ] Distraction-free mode (full screen, editor only)
- [ ] Outline panel to jump between titles

## Engineering

- [ ] Split `src/main.cpp` into smaller files (window, formatting actions, find bar)
- [ ] Automated tests for the formatting functions (Qt Test)
- [ ] Windows installer (Inno Setup or MSIX) that registers `.md` files
- [ ] Test the macOS app on a real Mac (it is built in CI but nobody has tried it yet)
- [ ] Sign and notarize the macOS app with an Apple Developer ID, so it opens without the security warning
