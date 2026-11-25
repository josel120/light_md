#include "formatting.h"

#include <QCoreApplication>
#include <QPlainTextEdit>
#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextDocument>

namespace Formatting {

namespace {

// Visits the lines touched by the selection (or the cursor's line).
void forEachSelectedLine(QPlainTextEdit *editor, const std::function<void(const QTextBlock &)> &fn)
{
    QTextCursor cursor = editor->textCursor();
    QTextDocument *doc = editor->document();
    QTextBlock first = doc->findBlock(cursor.selectionStart());
    QTextBlock last = doc->findBlock(cursor.selectionEnd());
    // A selection that ends right at the start of a line does not include that line.
    if (cursor.hasSelection() && last != first && cursor.selectionEnd() == last.position())
        last = last.previous();
    const bool single = first == last;

    cursor.beginEditBlock();
    for (QTextBlock block = first; block.isValid(); block = block.next()) {
        if (single || !block.text().trimmed().isEmpty())
            fn(block);
        if (block == last)
            break;
    }
    cursor.endEditBlock();
}

} // namespace

void prefixLines(QPlainTextEdit *editor, const std::function<QString(int)> &prefixFor)
{
    int n = 0;
    forEachSelectedLine(editor, [&](const QTextBlock &block) {
        QTextCursor c(block);
        c.insertText(prefixFor(++n));
    });
}

void setHeading(QPlainTextEdit *editor, const QString &marks)
{
    static const QRegularExpression existing(QStringLiteral("^#{1,6}\\s*"));
    forEachSelectedLine(editor, [&](const QTextBlock &block) {
        QTextCursor c(block);
        const QRegularExpressionMatch match = existing.match(block.text());
        if (match.hasMatch())
            c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, match.capturedLength());
        c.insertText(marks);
    });
}

void wrapSelection(QPlainTextEdit *editor, const QString &marks)
{
    QTextCursor cursor = editor->textCursor();
    cursor.beginEditBlock();
    if (cursor.hasSelection()) {
        const QString selected = cursor.selectedText();
        cursor.insertText(marks + selected + marks);
    } else {
        // Leave the sample word selected so the user can type over it.
        const QString sample = QCoreApplication::translate("Formatting", "text");
        cursor.insertText(marks + sample + marks);
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, marks.size());
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, sample.size());
    }
    cursor.endEditBlock();
    editor->setTextCursor(cursor);
}

QString linkAddress(const QString &typed)
{
    QString address = typed.trimmed();
    if (address.isEmpty() || address == QStringLiteral("https://"))
        return QString();
    if (!address.contains(QStringLiteral("://")) && !address.startsWith(QStringLiteral("mailto:")))
        address.prepend(QStringLiteral("https://"));
    return address;
}

void insertLink(QPlainTextEdit *editor, const QString &address)
{
    QTextCursor cursor = editor->textCursor();
    const bool hadSelection = cursor.hasSelection();
    const QString label = hadSelection ? cursor.selectedText() : QCoreApplication::translate("Formatting", "link text");
    cursor.beginEditBlock();
    cursor.insertText(QStringLiteral("[%1](%2)").arg(label, address));
    cursor.endEditBlock();
    if (!hadSelection) {
        cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, address.size() + 3);
        cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, label.size());
    }
    editor->setTextCursor(cursor);
}

// Blank lines around the block, because otherwise "---" under a line of text would turn that
// line into a title.
void insertBlock(QPlainTextEdit *editor, const QString &text, const QString &selectWord)
{
    QTextCursor cursor = editor->textCursor();
    cursor.clearSelection();
    // Go below the whole paragraph, so a table or list the cursor is in is not split.
    QTextBlock block = cursor.block();
    while (!block.text().trimmed().isEmpty() && block.next().isValid()
           && !block.next().text().trimmed().isEmpty())
        block = block.next();
    cursor.setPosition(block.position() + block.length() - 1);
    QString before;
    if (!block.text().trimmed().isEmpty())
        before = QStringLiteral("\n\n");
    else if (block.previous().isValid() && !block.previous().text().trimmed().isEmpty())
        before = QStringLiteral("\n");
    // End on a fresh line, with a blank line before any text that follows.
    const QTextBlock next = block.next();
    const QString after = !next.isValid() || !next.text().trimmed().isEmpty() ? QStringLiteral("\n") : QString();

    const int start = cursor.position() + before.size();
    cursor.beginEditBlock();
    cursor.insertText(before + text + after);
    cursor.endEditBlock();
    if (!selectWord.isEmpty()) {
        // Leave the sample word selected so the user can type over it.
        cursor.setPosition(start + text.indexOf(selectWord));
        cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selectWord.size());
    }
    editor->setTextCursor(cursor);
}

void insertCodeBlock(QPlainTextEdit *editor)
{
    QTextCursor cursor = editor->textCursor();
    if (!cursor.hasSelection()) {
        insertBlock(editor, QStringLiteral("```\ncode\n```"), QStringLiteral("code"));
        return;
    }
    // Fence the selected lines without changing them.
    QTextDocument *doc = editor->document();
    const QTextBlock first = doc->findBlock(cursor.selectionStart());
    QTextBlock last = doc->findBlock(cursor.selectionEnd());
    if (last != first && cursor.selectionEnd() == last.position())
        last = last.previous();
    cursor.beginEditBlock();
    QTextCursor end(last);
    end.movePosition(QTextCursor::EndOfBlock);
    end.insertText(QStringLiteral("\n```"));
    QTextCursor begin(first);
    begin.insertText(QStringLiteral("```\n"));
    cursor.endEditBlock();
}

} // namespace Formatting
