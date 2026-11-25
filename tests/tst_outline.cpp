#include "outline.h"

#include <QTest>

using Outline::Heading;

// Which lines the outline panel lists as titles, and how it nests them.
class TestOutline : public QObject
{
    Q_OBJECT

private slots:
    void hashTitles()
    {
        const QList<Heading> found = Outline::headings(
            QStringLiteral("# Big\ntext\n## Sub ##\n###### Six\n#hashtag\n####### seven"));
        const QList<Heading> expected{{1, QStringLiteral("Big"), 0},
                                      {2, QStringLiteral("Sub"), 2},
                                      {6, QStringLiteral("Six"), 3}};
        QCOMPARE(found, expected);
    }

    void underlinedTitles()
    {
        const QList<Heading> found =
            Outline::headings(QStringLiteral("Title\n=====\n\nSubtitle\n---\n\n- item\n---\n\n---"));
        const QList<Heading> expected{{1, QStringLiteral("Title"), 0}, {2, QStringLiteral("Subtitle"), 3}};
        QCOMPARE(found, expected);
    }

    void codeBlocksAreLeftOut()
    {
        const QList<Heading> found = Outline::headings(
            QStringLiteral("```\n# not a title\n```\n~~~~\n## nor this\n~~~\n~~~~\n# Real"));
        QCOMPARE(found, QList<Heading>({{1, QStringLiteral("Real"), 7}}));
    }

    void symbolsAreRemoved()
    {
        const QList<Heading> found =
            Outline::headings(QStringLiteral("# **Bold** and *it* with `code` and [a link](https://x.y) snake_case"));
        QCOMPARE(found.value(0).text, QStringLiteral("Bold and it with code and a link snake_case"));
    }

    void windowsLineEndings()
    {
        QCOMPARE(Outline::headings(QStringLiteral("# One\r\ntext\r\nTwo\r\n===\r\n")).size(), 2);
    }

    void panelNestsSubtitles()
    {
        OutlinePanel panel;
        panel.setHeadings(Outline::headings(QStringLiteral("# A\n## A1\n### A1a\n## A2\n# B\n### B1")));
        QCOMPARE(panel.topLevelItemCount(), 2);
        QCOMPARE(panel.topLevelItem(0)->childCount(), 2);
        QCOMPARE(panel.topLevelItem(0)->child(0)->child(0)->text(0), QStringLiteral("A1a"));
        // A level skipped (# then ###) still nests under the title above.
        QCOMPARE(panel.topLevelItem(1)->child(0)->text(0), QStringLiteral("B1"));
    }

    void currentSectionFollowsTheCursor()
    {
        OutlinePanel panel;
        panel.setHeadings(Outline::headings(QStringLiteral("intro\n# A\ntext\n# B\ntext")));
        panel.setCurrentLine(0);
        QVERIFY(!panel.currentItem());
        panel.setCurrentLine(2);
        QCOMPARE(panel.currentItem()->text(0), QStringLiteral("A"));
        panel.setCurrentLine(4);
        QCOMPARE(panel.currentItem()->text(0), QStringLiteral("B"));
    }

    void clickingJumps()
    {
        OutlinePanel panel;
        panel.setHeadings(Outline::headings(QStringLiteral("# A\n\n# B")));
        int jumpedTo = -1;
        panel.jump = [&](int, int line) { jumpedTo = line; };
        emit panel.itemClicked(panel.topLevelItem(1), 0);
        QCOMPARE(jumpedTo, 2);
    }
};

QTEST_MAIN(TestOutline)
#include "tst_outline.moc"
