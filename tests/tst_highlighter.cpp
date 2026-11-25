#include "highlighter.h"

#include <QFont>
#include <QPlainTextEdit>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>

// Checks which characters of a line end up bold, italic, bigger or in the code font.
class TestHighlighter : public QObject
{
    Q_OBJECT

private:
    QPlainTextEdit editor;
    MarkdownHighlighter *highlighter = nullptr;

    QTextCharFormat formatAt(int line, int column) const
    {
        const QTextBlock block = editor.document()->findBlockByNumber(line);
        for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
            if (column >= range.start && column < range.start + range.length)
                return range.format;
        }
        return QTextCharFormat();
    }

    bool bold(int line, int column) const { return formatAt(line, column).fontWeight() >= QFont::Bold; }
    bool italic(int line, int column) const { return formatAt(line, column).fontItalic(); }
    bool code(int line, int column) const { return formatAt(line, column).background().style() != Qt::NoBrush; }

private slots:
    void initTestCase()
    {
        highlighter = new MarkdownHighlighter(editor.document());
        highlighter->setStyle(editor.palette(), 10);
    }

    void titlesAreBoldAndBigger()
    {
        editor.setPlainText(QStringLiteral("# Big\n## Sub\nplain"));
        QVERIFY(bold(0, 2));
        QCOMPARE(formatAt(0, 2).fontPointSize(), 16.0);
        QCOMPARE(formatAt(1, 3).fontPointSize(), 13.5);
        QVERIFY(!bold(2, 0));
    }

    void emphasis()
    {
        editor.setPlainText(QStringLiteral("a **bold** and *it* and _it_"));
        QVERIFY(bold(0, 5));
        QVERIFY(!bold(0, 0));
        QVERIFY(italic(0, 16));
        QVERIFY(italic(0, 25));
    }

    void multiplicationIsNotItalic()
    {
        editor.setPlainText(QStringLiteral("2 * 3 * 4"));
        QVERIFY(!italic(0, 4));
    }

    void inlineCodeIsNotFormattedInside()
    {
        editor.setPlainText(QStringLiteral("run `a **b**` now"));
        QVERIFY(code(0, 6));
        QVERIFY(!bold(0, 9));
        QVERIFY(!code(0, 15));
    }

    void codeBlockSpansLines()
    {
        editor.setPlainText(QStringLiteral("```\n**not bold**\n```\n**bold**"));
        QVERIFY(code(1, 3));
        QVERIFY(!bold(1, 3));
        QVERIFY(bold(3, 3));
    }

    void openingAFenceRecolorsBelow()
    {
        editor.setPlainText(QStringLiteral("text\n**bold**"));
        QVERIFY(bold(1, 3));
        QTextCursor cursor(editor.document());
        cursor.insertText(QStringLiteral("```\n"));
        QVERIFY(code(2, 3));
        QVERIFY(!bold(2, 3));
    }

    void shorterFenceInsideIsCode()
    {
        editor.setPlainText(QStringLiteral("````\n```\n**a**\n```\n````\n**bold**"));
        QVERIFY(code(2, 2));
        QVERIFY(!bold(2, 2));
        QVERIFY(bold(5, 3));
    }

    void fenceWithTextAfterDoesNotClose()
    {
        editor.setPlainText(QStringLiteral("```\n``` not a fence\n**a**\n```\n**bold**"));
        QVERIFY(!bold(2, 2));
        QVERIFY(bold(4, 3));
    }

    void inlineCodeAtLineStartIsNotAFence()
    {
        editor.setPlainText(QStringLiteral("```x``` here\n**bold**"));
        QVERIFY(bold(1, 3));
    }

    void lengtheningTheOpenerRecolorsBelow()
    {
        editor.setPlainText(QStringLiteral("```\n**a**\n```\n**b**"));
        QVERIFY(bold(3, 2));
        QTextCursor cursor(editor.document());
        cursor.insertText(QStringLiteral("`"));
        QVERIFY(!bold(3, 2));
    }

    void quotedCodeBlock()
    {
        editor.setPlainText(QStringLiteral("> ```\n> **a**\n> ```\n**bold**"));
        QVERIFY(code(1, 4));
        QVERIFY(!bold(1, 4));
        QVERIFY(bold(3, 3));
    }

    void quotedCodeBlockEndsWithTheQuote()
    {
        editor.setPlainText(QStringLiteral("> ```\n> **a**\n**bold**"));
        QVERIFY(!bold(1, 4));
        QVERIFY(bold(2, 3));
    }

    void codeInsideBoldAndLinks()
    {
        editor.setPlainText(QStringLiteral("**Run `git status` first**\n[see `x` here](u)"));
        QVERIFY(bold(0, 3));
        QVERIFY(bold(0, 20));
        QVERIFY(code(0, 9));
        QVERIFY(formatAt(1, 2).fontUnderline());
        QVERIFY(formatAt(1, 10).fontUnderline());
    }

    void tickedChecklistIsCrossedOut()
    {
        editor.setPlainText(QStringLiteral("- [x] done\n- [ ] open"));
        QVERIFY(formatAt(0, 7).fontStrikeOut());
        QVERIFY(!formatAt(1, 7).fontStrikeOut());
    }

    void linksAreUnderlined()
    {
        editor.setPlainText(QStringLiteral("see [site](https://example.com)"));
        QVERIFY(formatAt(0, 6).fontUnderline());
        QVERIFY(!formatAt(0, 15).fontUnderline());
    }

    void textSizeChangesTitles()
    {
        editor.setPlainText(QStringLiteral("# Big"));
        highlighter->setStyle(editor.palette(), 20);
        QCOMPARE(formatAt(0, 2).fontPointSize(), 32.0);
        highlighter->setStyle(editor.palette(), 10);
    }
};

QTEST_MAIN(TestHighlighter)
#include "tst_highlighter.moc"
