#include "export.h"

#include <QBuffer>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QTemporaryDir>
#include <QTest>
#include <QTextBlock>
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
        const QString markdown = QStringLiteral("![dot](%1)").arg(link);
        // Inside the document's folder: always.
        QVERIFY(Export::standaloneHtml(markdown, QStringLiteral("x"), m_dir.path())
                    .contains(QStringLiteral("data:image/png;base64,")));
        // An untitled document has no folder: only once the user said yes.
        QVERIFY(!Export::standaloneHtml(markdown, QStringLiteral("x"), QString()).contains(QStringLiteral("data:")));
        QVERIFY(Export::standaloneHtml(markdown, QStringLiteral("x"), QString(), true)
                    .contains(QStringLiteral("data:image/png;base64,")));
    }

    void outsidePicturesAreListed()
    {
        QTemporaryDir other;
        QImage picture(4, 4, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        QVERIFY(picture.save(other.filePath(QStringLiteral("private.png"))));
        const QString link = QUrl::fromLocalFile(other.filePath(QStringLiteral("private.png"))).toString();
        const QString markdown = QStringLiteral("![a](images/dot.png) ![b](%1) ![](%1) ![c](https://x.y/a.png)").arg(link);
        const QStringList outside = Export::outsidePictures(markdown, m_dir.path());
        QCOMPARE(outside.size(), 1);
        QVERIFY(outside.first().endsWith(QStringLiteral("private.png")));
    }

    void wordExportLeavesOutOutsidePictures()
    {
        QTemporaryDir other;
        QImage picture(4, 4, QImage::Format_RGB32);
        picture.fill(Qt::blue);
        QVERIFY(picture.save(other.filePath(QStringLiteral("private.png"))));
        const QString link = QUrl::fromLocalFile(other.filePath(QStringLiteral("private.png"))).toString();
        const QString markdown = QStringLiteral("![a](images/dot.png)\n\n![b](%1)").arg(link);
        const auto countImages = [&](bool includeOutside) {
            const auto document = Export::shareableDocument(markdown, m_dir.path(), includeOutside, QFont());
            int images = 0;
            for (QTextBlock block = document->begin(); block.isValid(); block = block.next()) {
                for (auto it = block.begin(); !it.atEnd(); ++it) {
                    const QTextCharFormat format = it.fragment().charFormat();
                    if (format.isImageFormat()
                        && !document->resource(QTextDocument::ImageResource, QUrl(format.toImageFormat().name())).isNull())
                        ++images;
                }
            }
            return images;
        };
        QCOMPARE(countImages(false), 1);
        QCOMPARE(countImages(true), 2);
    }

    void saveAsBringsRelativePictures()
    {
        const QStringList found = Export::relativePictures(
            QStringLiteral("![a](images/dot.png) ![b](../outside.png) ![c](https://x.y/a.png) ![d](images/missing.png)"),
            m_dir.path());
        QCOMPARE(found, QStringList({QStringLiteral("images/dot.png")}));
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
