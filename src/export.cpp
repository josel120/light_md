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

    const QString type = types.value(QFileInfo(path).suffix().toLower());
    QFile file(path);
    if (type.isEmpty() || !file.open(QIODevice::ReadOnly))
        return QString();
    return QStringLiteral("data:%1;base64,%2").arg(type, QString::fromLatin1(file.readAll().toBase64()));
}

} // namespace

QString standaloneHtml(const QString &markdown, const QString &title, const QString &folder)
{
    QTextDocument document;
    QFont font = QGuiApplication::font();
    font.setPointSize(11);
    document.setDefaultFont(font);
    document.setMarkdown(markdown);
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
