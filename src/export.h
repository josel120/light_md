#pragma once

#include <QString>

// Exporting a document to other formats (PDF and printing go through the preview instead).
namespace Export {

// `markdown` ready for Qt's Markdown importer, which leaves out pictures without a
// description (![](a.png)): those get one. Code is left exactly as written.
QString renderableMarkdown(const QString &markdown);

// A complete web page for `markdown`, titled `title`. Pictures found on disk (relative to
// `folder`, or as file:// links) are embedded, so the page works on its own when sent or moved.
// Only pictures in `folder` or below are embedded (any picture when `folder` is empty), so a
// document from someone else can't pull private pictures into a page that gets shared. Links
// that would run code (javascript: and the like) are made inactive.
QString standaloneHtml(const QString &markdown, const QString &title, const QString &folder);

} // namespace Export
