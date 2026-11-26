#pragma once

#include <QFont>
#include <QString>
#include <QStringList>
#include <QTextDocument>
#include <memory>

// Exporting a document to other formats (PDF and printing go through the preview instead).
namespace Export {

// `markdown` ready for Qt's Markdown importer, which leaves out pictures without a
// description (![](a.png)): those get one. Code is left exactly as written.
QString renderableMarkdown(const QString &markdown);

// A complete web page for `markdown`, titled `title`. Pictures found on disk (relative to
// `folder`, or as file:// links) are embedded, so the page works on its own when sent or moved.
// Only pictures in `folder` or below are embedded, unless `includeOutside`: a document from
// someone else could name private pictures anywhere on this computer, so the user is asked
// first (see outsidePictures). Links that would run code (javascript: and the like) are made
// inactive.
QString standaloneHtml(const QString &markdown, const QString &title, const QString &folder,
                       bool includeOutside = false);

// The same rules for the .odt export: a document showing only the pictures that may be shared.
std::unique_ptr<QTextDocument> shareableDocument(const QString &markdown, const QString &folder,
                                                 bool includeOutside, const QFont &font);

// Pictures the document shows from this computer but outside `folder` (all of them for an
// untitled document, which has no folder), as paths to show the user.
QStringList outsidePictures(const QString &markdown, const QString &folder);

// Pictures given relative to `folder` that are inside it ("images/a.png"), for Save As to bring
// along to a new folder.
QStringList relativePictures(const QString &markdown, const QString &folder);

} // namespace Export
