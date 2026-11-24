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

### Version 1.1
- [x] Find and replace (Ctrl+F, Ctrl+H, F3 / Shift+F3, match case, replace all)
- [x] Recent documents menu (last 10, removes files that no longer exist)
- [x] App icon on the window, the taskbar and the `.exe`
- [x] Checklist button (`- [ ]`)
- [x] Insert menu: picture, table, code block, separator line
- [x] Pictures next to the document show up in the preview and the PDF

## Next

- [ ] Synchronized scrolling between the editor and the preview
- [ ] Dark mode (follow Windows, with a manual switch)
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
- [ ] GitHub Actions: build on every push and attach the zip to each release
- [ ] Windows installer (Inno Setup or MSIX) that registers `.md` files
- [ ] Linux and macOS builds
