#include "export.h"

#include <QBuffer>
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
        // Into memory first, as exportOdt does, so a failed write never truncates the old file.
        QBuffer buffer;
        QVERIFY(buffer.open(QIODevice::WriteOnly));
        QVERIFY(QTextDocumentWriter(&buffer, "odf").write(&document));
        QVERIFY(buffer.data().size() > 100);
        QCOMPARE(buffer.data().left(2), QByteArray("PK"));
    }

    void pictureWithoutDescriptionIsKept()
    {
        const QString html =
            Export::standaloneHtml(QStringLiteral("![](images/dot.png)"), QStringLiteral("x"), m_dir.path());
        QVERIFY(html.contains(QStringLiteral("src=\"data:image/png;base64,")));
    }

    void codeIsNotTouchedWhenFillingDescriptions()
    {
        const QString markdown = QStringLiteral("`![](a.png)` and ![](b.png)\n```\n![](c.png)\n```");
        QCOMPARE(Export::renderableMarkdown(markdown),
                 QStringLiteral("`![](a.png)` and ![picture](b.png)\n```\n![](c.png)\n```"));
    }

    void picturesOutsideTheFolderAreNotEmbedded()
    {
        QTemporaryDir other;
        QImage picture(4, 4, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        QVERIFY(picture.save(other.filePath(QStringLiteral("private.png"))));
        const QString link = QUrl::fromLocalFile(other.filePath(QStringLiteral("private.png"))).toString();
        const QString html = Export::standaloneHtml(
            QStringLiteral("![a](%1)\n\n![b](../%2/private.png)").arg(link, QDir(other.path()).dirName()),
            QStringLiteral("x"), m_dir.path());
        QVERIFY(!html.contains(QStringLiteral("data:")));
    }

    void scriptLinksAreMadeInactive()
    {
        const QString html = Export::standaloneHtml(
            QStringLiteral("[bad](javascript:alert(1)) [ok](https://example.com) [here](#top)"),
            QStringLiteral("x"), m_dir.path());
        QVERIFY(!html.contains(QStringLiteral("javascript:")));
        QVERIFY(html.contains(QStringLiteral("href=\"https://example.com\"")));
        QVERIFY(html.contains(QStringLiteral("href=\"#top\"")));
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
