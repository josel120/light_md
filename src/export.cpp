#include "export.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QHash>
#include <QRegularExpression>
#include <QTextDocument>
#include <QUrl>

namespace Export {

namespace {

// The picture at `source` as a data: URL, or an empty string when it is not a readable file.
QString embeddedPicture(const QString &source, const QString &folder)
{
    static const QHash<QString, QString> types{
        {QStringLiteral("png"), QStringLiteral("image/png")},   {QStringLiteral("jpg"), QStringLiteral("image/jpeg")},
        {QStringLiteral("jpeg"), QStringLiteral("image/jpeg")}, {QStringLiteral("gif"), QStringLiteral("image/gif")},
        {QStringLiteral("bmp"), QStringLiteral("image/bmp")},   {QStringLiteral("webp"), QStringLiteral("image/webp")},
        {QStringLiteral("svg"), QStringLiteral("image/svg+xml")},
    };
    const QUrl url(source);
    QString path;
    if (url.isLocalFile())
        path = url.toLocalFile();
    else if (url.scheme().isEmpty() || (url.scheme().size() == 1 && QDir::isAbsolutePath(source)))
        // Relative ("images/a.png") or a Windows path ("C:/pictures/a.png", whose drive looks like a scheme).
        path = QDir::isAbsolutePath(source) ? source : QDir(folder).filePath(QUrl::fromPercentEncoding(source.toUtf8()));
    else
        return QString(); // On the web: leave the link as it is.

    // Only from the document's own folder: a document from someone else could name any
    // picture on this computer, and the page is meant to be sent.
    if (!folder.isEmpty()) {
        const QString inside = QFileInfo(path).canonicalFilePath();
        const QString root = QFileInfo(folder).canonicalFilePath();
        if (inside.isEmpty() || root.isEmpty()
            || !inside.startsWith(root.endsWith(QLatin1Char('/')) ? root : root + QLatin1Char('/'),
#ifdef Q_OS_WIN
                                  Qt::CaseInsensitive))
#else
                                  Qt::CaseSensitive))
#endif
            return QString();
    }

    const QString type = types.value(QFileInfo(path).suffix().toLower());
    QFile file(path);
    if (type.isEmpty() || !file.open(QIODevice::ReadOnly))
        return QString();
    return QStringLiteral("data:%1;base64,%2").arg(type, QString::fromLatin1(file.readAll().toBase64()));
}

// Links that open web pages, mail or another part of the page are fine; javascript:, data:
// and the like could run code with the page's contents, so they lose their target.
bool safeLink(QString target)
{
    static const QRegularExpression schemeRe(QStringLiteral("^([A-Za-z][A-Za-z0-9+.-]*):"));
    target.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    target.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    target.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    target.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    // Browsers ignore spaces and control characters in a scheme ("java\tscript:").
    QString compact;
    for (const QChar c : target) {
        if (c.unicode() > 0x20)
            compact += c;
    }
    const QRegularExpressionMatch m = schemeRe.match(compact);
    if (!m.hasMatch())
        return true; // Relative link or #anchor.
    static const QStringList allowed{QStringLiteral("http"), QStringLiteral("https"), QStringLiteral("mailto"),
                                     QStringLiteral("ftp"), QStringLiteral("tel")};
    return allowed.contains(m.captured(1).toLower());
}

} // namespace

QString renderableMarkdown(const QString &markdown)
{
    static const QRegularExpression fenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})"));
    QStringList lines = markdown.split(QLatin1Char('\n'));
    QString fence;
    for (QString &line : lines) {
        const QRegularExpressionMatch m = fenceRe.match(line);
        if (!fence.isEmpty()) {
            if (m.hasMatch() && m.captured(1).at(0) == fence.at(0) && m.capturedLength(1) >= fence.size())
                fence.clear();
            continue;
        }
        if (m.hasMatch()) {
            fence = m.captured(1);
            continue;
        }
        if (!line.contains(QStringLiteral("![](")))
            continue;
        // Outside `inline code` only: an odd number of backticks before means inside.
        QString out;
        int ticks = 0;
        for (int i = 0; i < line.size(); ++i) {
            if (line.at(i) == QLatin1Char('`'))
                ++ticks;
            if (ticks % 2 == 0 && QStringView(line).mid(i).startsWith(QStringLiteral("![]("))) {
                out += QStringLiteral("![picture](");
                i += 3;
                continue;
            }
            out += line.at(i);
        }
        line = out;
    }
    return lines.join(QLatin1Char('\n'));
}

QString standaloneHtml(const QString &markdown, const QString &title, const QString &folder)
{
    QTextDocument document;
    QFont font = QGuiApplication::font();
    font.setPointSize(11);
    document.setDefaultFont(font);
    document.setMarkdown(renderableMarkdown(markdown));
    // Qt writes the title as is, so escape it here.
    document.setMetaInformation(QTextDocument::DocumentTitle, title.toHtmlEscaped());
    QString html = document.toHtml();
    static const QRegularExpression doctypeRe(QStringLiteral("^<!DOCTYPE[^>]*>"));
    html.replace(doctypeRe, QStringLiteral("<!DOCTYPE html>"));

    static const QRegularExpression imageRe(QStringLiteral("(<img[^>]*\\ssrc=\")([^\"]*)(\")"));
    QString embedded;
    qsizetype last = 0;
    auto it = imageRe.globalMatch(html);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        // The source is HTML-escaped in the page; unescape it to find the file.
        QString source = m.captured(2);
        source.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        const QString data = embeddedPicture(source, folder);
        embedded += html.mid(last, m.capturedStart(2) - last);
        embedded += data.isEmpty() ? m.captured(2) : data;
        last = m.capturedEnd(2);
    }
    embedded += html.mid(last);

    static const QRegularExpression linkRe(QStringLiteral("(<a[^>]*?)\\shref=\"([^\"]*)\""));
    QString safe;
    last = 0;
    auto linkIt = linkRe.globalMatch(embedded);
    while (linkIt.hasNext()) {
        const QRegularExpressionMatch m = linkIt.next();
        if (safeLink(m.captured(2)))
            continue;
        safe += embedded.mid(last, m.capturedEnd(1) - last);
        last = m.capturedEnd(0);
    }
    safe += embedded.mid(last);
    embedded = safe;

    // Readable on a phone or a wide screen: a comfortable column, centered.
    const QString head = QStringLiteral(
        "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />\n"
        "<style>body { max-width: 46em; margin: 2em auto; padding: 0 1em; line-height: 1.5; }"
        " img { max-width: 100%; } table { border-collapse: collapse; }"
        " td, th { border: 1px solid #999; padding: 0.3em 0.6em; }</style>\n");
    const qsizetype headEnd = embedded.indexOf(QStringLiteral("</head>"));
    if (headEnd >= 0)
        embedded.insert(headEnd, head);
    return embedded;
}

} // namespace Export
