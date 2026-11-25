#pragma once

#include <QString>

// Exporting a document to other formats (PDF and printing go through the preview instead).
namespace Export {

// A complete web page for `markdown`, titled `title`. Pictures found on disk (relative to
// `folder`, or as file:// links) are embedded, so the page works on its own when sent or moved.
QString standaloneHtml(const QString &markdown, const QString &title, const QString &folder);

} // namespace Export
