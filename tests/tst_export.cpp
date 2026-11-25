#include "export.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QTextDocument>
#include <QTextDocumentWriter>
#include <QUrl>

class TestExport : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

private slots:
    void initTestCase()
    {
        QVERIFY(m_dir.isValid());
        QDir(m_dir.path()).mkpath(QStringLiteral("images"));
        QImage picture(4, 4, QImage::Format_RGB32);
        picture.fill(Qt::red);
        QVERIFY(picture.save(m_dir.filePath(QStringLiteral("images/dot.png"))));
    }

    void titleAndText()
    {
        const QString html = Export::standaloneHtml(QStringLiteral("# Hello\n\nSome **bold** text."),
                                                    QStringLiteral("Notes & plans"), m_dir.path());
        QVERIFY(html.contains(QStringLiteral("<title>Notes &amp; plans</title>")));
        QVERIFY(html.contains(QStringLiteral("Hello")));
        QVERIFY(html.contains(QStringLiteral("bold")));
        QVERIFY(html.startsWith(QStringLiteral("<!DOCTYPE html>")));
        QVERIFY(html.contains(QStringLiteral("charset=\"utf-8\"")));
    }

    void picturesNextToTheDocumentAreEmbedded()
    {
        const QString html =
            Export::standaloneHtml(QStringLiteral("![dot](images/dot.png)"), QStringLiteral("x"), m_dir.path());
        QVERIFY(html.contains(QStringLiteral("src=\"data:image/png;base64,")));
        QVERIFY(!html.contains(QStringLiteral("images/dot.png")));
    }

    void pictureGivenAsFileLinkIsEmbedded()
    {
        const QString link = QUrl::fromLocalFile(m_dir.filePath(QStringLiteral("images/dot.png"))).toString();
        const QString html = Export::standaloneHtml(QStringLiteral("![dot](%1)").arg(link), QStringLiteral("x"), QString());
        QVERIFY(html.contains(QStringLiteral("data:image/png;base64,")));
    }

    // "Export for Word" uses Qt's OpenDocument writer; it must produce a real .odt (a zip file).
    void openDocumentFile()
    {
        QTextDocument document;
        document.setMarkdown(QStringLiteral("# Title\n\nText"));
        const QString path = m_dir.filePath(QStringLiteral("out.odt"));
        QVERIFY(QTextDocumentWriter(path, "odf").write(&document));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.read(2), QByteArray("PK"));
    }

    void webAndMissingPicturesAreLeftAlone()
    {
        const QString html = Export::standaloneHtml(
            QStringLiteral("![web](https://example.com/a.png)\n\n![gone](images/missing.png)"), QStringLiteral("x"),
            m_dir.path());
        QVERIFY(html.contains(QStringLiteral("https://example.com/a.png")));
        QVERIFY(html.contains(QStringLiteral("images/missing.png")));
        QVERIFY(!html.contains(QStringLiteral("data:")));
    }
};

QTEST_MAIN(TestExport)
#include "tst_export.moc"
