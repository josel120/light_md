#include "formatting.h"

#include <QPlainTextEdit>
#include <QTest>
#include <QTextCursor>

// The formatting buttons, on a writing pane that is never shown.
class TestFormatting : public QObject
{
    Q_OBJECT

private:
    QPlainTextEdit editor;

    // Puts `text` in the pane and selects from `start` to `end` (or just places the cursor).
    void setText(const QString &text, int start = 0, int end = -1)
    {
        editor.setPlainText(text);
        QTextCursor cursor(editor.document());
        cursor.setPosition(start);
        if (end >= 0)
            cursor.setPosition(end, QTextCursor::KeepAnchor);
        editor.setTextCursor(cursor);
    }

    QString text() const { return editor.toPlainText(); }
    QString selected() const { return editor.textCursor().selectedText(); }

    static QString bullet(int) { return QStringLiteral("- "); }

private slots:
    void headingOnCursorLine()
    {
        setText(QStringLiteral("first\nsecond"), 8);
        Formatting::setHeading(&editor, QStringLiteral("# "));
        QCOMPARE(text(), QStringLiteral("first\n# second"));
    }

    void headingReplacesExistingMarks()
    {
        setText(QStringLiteral("### Old title"), 3);
        Formatting::setHeading(&editor, QStringLiteral("## "));
        QCOMPARE(text(), QStringLiteral("## Old title"));
    }

    void headingOnSeveralLines()
    {
        setText(QStringLiteral("one\ntwo"), 0, 7);
        Formatting::setHeading(&editor, QStringLiteral("# "));
        QCOMPARE(text(), QStringLiteral("# one\n# two"));
    }

    void bulletsSkipBlankLinesInSelection()
    {
        setText(QStringLiteral("a\n\nb"), 0, 4);
        Formatting::prefixLines(&editor, bullet);
        QCOMPARE(text(), QStringLiteral("- a\n\n- b"));
    }

    void bulletOnEmptyLine()
    {
        setText(QString());
        Formatting::prefixLines(&editor, bullet);
        QCOMPARE(text(), QStringLiteral("- "));
    }

    void numberedListCounts()
    {
        setText(QStringLiteral("a\nb\nc"), 0, 5);
        Formatting::prefixLines(&editor, [](int n) { return QStringLiteral("%1. ").arg(n); });
        QCOMPARE(text(), QStringLiteral("1. a\n2. b\n3. c"));
    }

    void numberedListCountsOnlyListedLines()
    {
        setText(QStringLiteral("a\n\nb"), 0, 4);
        Formatting::prefixLines(&editor, [](int n) { return QStringLiteral("%1. ").arg(n); });
        QCOMPARE(text(), QStringLiteral("1. a\n\n2. b"));
    }

    void checklistAndQuote()
    {
        setText(QStringLiteral("buy milk"));
        Formatting::prefixLines(&editor, [](int) { return QStringLiteral("- [ ] "); });
        QCOMPARE(text(), QStringLiteral("- [ ] buy milk"));
        setText(QStringLiteral("wise words\nmore words"), 0, 21);
        Formatting::prefixLines(&editor, [](int) { return QStringLiteral("> "); });
        QCOMPARE(text(), QStringLiteral("> wise words\n> more words"));
    }

    void selectionEndingAtLineStartLeavesThatLine()
    {
        // Selecting "a\n" by dragging to the start of the next line only means line "a".
        setText(QStringLiteral("a\nb"), 0, 2);
        Formatting::prefixLines(&editor, bullet);
        QCOMPARE(text(), QStringLiteral("- a\nb"));
    }

    void prefixingIsOneUndoStep()
    {
        setText(QStringLiteral("a\nb\nc"), 0, 5);
        Formatting::prefixLines(&editor, bullet);
        editor.undo();
        QCOMPARE(text(), QStringLiteral("a\nb\nc"));
    }

    void boldWrapsSelection()
    {
        setText(QStringLiteral("hello world"), 6, 11);
        Formatting::wrapSelection(&editor, QStringLiteral("**"));
        QCOMPARE(text(), QStringLiteral("hello **world**"));
    }

    void italicWithoutSelectionSelectsSample()
    {
        setText(QStringLiteral("say "), 4);
        Formatting::wrapSelection(&editor, QStringLiteral("*"));
        QCOMPARE(text(), QStringLiteral("say *text*"));
        QCOMPARE(selected(), QStringLiteral("text"));
    }

    void linkAddress_data()
    {
        QTest::addColumn<QString>("typed");
        QTest::addColumn<QString>("address");
        QTest::newRow("adds https") << "example.com" << "https://example.com";
        QTest::newRow("keeps scheme") << "http://example.com" << "http://example.com";
        QTest::newRow("keeps other schemes") << "ftp://files.example.com" << "ftp://files.example.com";
        QTest::newRow("mail") << "mailto:me@example.com" << "mailto:me@example.com";
        QTest::newRow("trims") << "  example.com  " << "https://example.com";
        QTest::newRow("untouched default") << "https://" << "";
        QTest::newRow("empty") << "   " << "";
    }

    void linkAddress()
    {
        QFETCH(QString, typed);
        QFETCH(QString, address);
        QCOMPARE(Formatting::linkAddress(typed), address);
    }

    void linkOnSelection()
    {
        setText(QStringLiteral("see the site"), 8, 12);
        Formatting::insertLink(&editor, QStringLiteral("https://example.com"));
        QCOMPARE(text(), QStringLiteral("see the [site](https://example.com)"));
    }

    void linkWithoutSelectionSelectsLabel()
    {
        setText(QString());
        Formatting::insertLink(&editor, QStringLiteral("https://example.com"));
        QCOMPARE(text(), QStringLiteral("[link text](https://example.com)"));
        QCOMPARE(selected(), QStringLiteral("link text"));
    }

    void blockGoesBelowParagraphWithBlankLines()
    {
        setText(QStringLiteral("line one\nline two\n\nnext"), 2);
        Formatting::insertBlock(&editor, QStringLiteral("---"));
        QCOMPARE(text(), QStringLiteral("line one\nline two\n\n---\n\nnext"));
    }

    void blockAtEndOfDocument()
    {
        setText(QStringLiteral("text"), 4);
        Formatting::insertBlock(&editor, QStringLiteral("---"));
        QCOMPARE(text(), QStringLiteral("text\n\n---\n"));
    }

    void tableSelectsFirstHeader()
    {
        setText(QString());
        Formatting::insertBlock(&editor, QStringLiteral("| Column 1 | Column 2 |\n| --- | --- |\n| Text | Text |"),
                                QStringLiteral("Column 1"));
        QCOMPARE(selected(), QStringLiteral("Column 1"));
    }

    void codeBlockFencesSelectedLines()
    {
        setText(QStringLiteral("intro\nls -la\ncd ..\nend"), 6, 18);
        Formatting::insertCodeBlock(&editor);
        QCOMPARE(text(), QStringLiteral("intro\n```\nls -la\ncd ..\n```\nend"));
    }

    void emptyCodeBlockSelectsSample()
    {
        setText(QString());
        Formatting::insertCodeBlock(&editor);
        QCOMPARE(text(), QStringLiteral("```\ncode\n```\n"));
        QCOMPARE(selected(), QStringLiteral("code"));
    }
};

QTEST_MAIN(TestFormatting)
#include "tst_formatting.moc"
