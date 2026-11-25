# Roadmap

What light_md already does, what is coming next, and ideas for later.
Tick a box (`[x]`) and move the line to **Done** when a feature is finished.
While working on something, move it to **In progress** so we always know where we left off.
Each open item links to its GitHub issue; write "Closes #N" in a pull request to close the issue when it is merged.

## In progress

Order agreed on 2026-11-25, one at a time, one pull request each:

1. [x] Syntax highlighting in the editor (#11)
2. [x] Paste pictures from the clipboard (#12)
3. [x] Split `src/main.cpp` into smaller files (#19)
4. [x] Automated tests for the formatting functions (#20)
5. [ ] Tabs to keep several documents open (#13)
6. [ ] Export to HTML (#14)
7. [ ] Interface in Spanish, Portuguese, French and German (#15)
8. [ ] Spell checker (#16)
9. [ ] Distraction-free mode (#17)
10. [ ] Outline panel to jump between titles (#18)
11. [ ] Windows installer that registers `.md` files (#21)

Need a person: testing on a real Mac (#22), signing with an Apple Developer ID (#23) and publishing releases (#9).

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

### Version 1.4 (in progress, release tracked in [#9](https://github.com/josel120/light_md/issues/9))
- [x] Synchronized scrolling between the editor and the preview (Options → Sync scrolling turns it off)
- [x] Dark theme for every system (Options → Theme → Dark, Ctrl+Shift+4); the title bar follows the theme too
- [x] Reopens the last document on start, at the same cursor and scroll position (also remembered for each recent document)
- [x] Bigger or smaller text in both panes (Ctrl + mouse wheel, Ctrl+Plus / Ctrl+Minus, Ctrl+0 for normal; Options → Text size), remembered

### Version 1.5 (in progress)
- [x] Autosave and recovery after a crash: a copy of unsaved changes is kept every few seconds, and offered back after a crash ([#10](https://github.com/josel120/light_md/issues/10))
- [x] Syntax highlighting in the editor: bigger titles, bold, italic, links, code, lists, checklists and dimmed quotes, in the colors of the current theme ([#11](https://github.com/josel120/light_md/issues/11))
- [x] Paste pictures from the clipboard (Ctrl+V): saved as PNG in an `images` folder next to the document; an unsaved document is saved first ([#12](https://github.com/josel120/light_md/issues/12))
- [x] `src/main.cpp` split into smaller files: window, formatting, find bar, session and updates ([#19](https://github.com/josel120/light_md/issues/19))
- [x] Unit tests (Qt Test) for formatting, find and replace, syntax highlighting and reopening where you left off, run by CI on every push ([#20](https://github.com/josel120/light_md/issues/20))

## Next

Nothing queued; the items below are being worked through in the order listed under **In progress**.

## Later

- [ ] Tabs to keep several documents open ([#13](https://github.com/josel120/light_md/issues/13))
- [ ] Export to HTML (and maybe `.docx`) ([#14](https://github.com/josel120/light_md/issues/14))
- [ ] Interface in Spanish, Portuguese, French and German (`QTranslator`) ([#15](https://github.com/josel120/light_md/issues/15))
- [ ] Spell checker (Hunspell) ([#16](https://github.com/josel120/light_md/issues/16))
- [ ] Distraction-free mode (full screen, editor only) ([#17](https://github.com/josel120/light_md/issues/17))
- [ ] Outline panel to jump between titles ([#18](https://github.com/josel120/light_md/issues/18))

## Engineering

- [ ] Windows installer (Inno Setup or MSIX) that registers `.md` files ([#21](https://github.com/josel120/light_md/issues/21))
- [ ] Test the macOS app on a real Mac (it is built in CI but nobody has tried it yet) ([#22](https://github.com/josel120/light_md/issues/22))
- [ ] Sign and notarize the macOS app with an Apple Developer ID, so it opens without the security warning ([#23](https://github.com/josel120/light_md/issues/23))
