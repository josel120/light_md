#include "editor.h"

#include "spellcheck.h"

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFont>
#include <QMenu>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextLayout>
#include <memory>

void MarkdownEditor::setCentered(bool centered)
{
    if (m_centered == centered)
        return;
    m_centered = centered;
    // No border around the page either.
    setFrameShape(centered ? QFrame::NoFrame : QFrame::StyledPanel);
    updateMargins();
}

void MarkdownEditor::resizeEvent(QResizeEvent *event)
{
    QPlainTextEdit::resizeEvent(event);
    updateMargins();
}

void MarkdownEditor::changeEvent(QEvent *event)
{
    QPlainTextEdit::changeEvent(event);
    // Bigger text means a wider column.
    if (event->type() == QEvent::FontChange)
        updateMargins();
}

// About 70 characters a line, the width books and newspapers use, centered; the space on the
// sides stays empty paper (the margins are part of the viewport's background).
void MarkdownEditor::updateMargins()
{
    if (!m_centered) {
        setViewportMargins(0, 0, 0, 0);
        return;
    }
    const int column = fontMetrics().horizontalAdvance(QStringLiteral("n")) * 70;
    const int scrollBar = verticalScrollBar()->isVisible() ? verticalScrollBar()->width() : 0;
    const int side = qMax(16, (width() - scrollBar - column) / 2);
    const int top = qMin(64, height() / 12);
    setViewportMargins(side, top, side, 0);
}

// Right-clicking a word with the red wave puts its corrections, and "Add to dictionary", above
// the usual Cut / Copy / Paste.
void MarkdownEditor::contextMenuEvent(QContextMenuEvent *event)
{
    std::unique_ptr<QMenu> menu(createStandardContextMenu(event->pos()));
    const QTextCursor clicked = cursorForPosition(event->pos());
    const QTextBlock block = clicked.block();
    const int column = clicked.positionInBlock();

    // The highlighter marked exactly the misspelled word, so its range is the word.
    int start = -1;
    int length = 0;
    if (speller && speller->isActive() && block.layout()) {
        for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
            if (range.format.underlineStyle() == QTextCharFormat::SpellCheckUnderline
                && column >= range.start && column <= range.start + range.length) {
                start = range.start;
                length = range.length;
                break;
            }
        }
    }

    if (start >= 0) {
        const QString word = block.text().mid(start, length);
        QAction *first = menu->actions().value(0);
        const QStringList suggestions = speller->suggestions(word);
        for (const QString &suggestion : suggestions) {
            auto *action = new QAction(suggestion, menu.get());
            QFont bold = action->font();
            bold.setBold(true);
            action->setFont(bold);
            connect(action, &QAction::triggered, this, [this, block, start, length, suggestion] {
                // Through a cursor, so Ctrl+Z brings the old word back.
                QTextCursor cursor(document());
                cursor.setPosition(block.position() + start);
                cursor.setPosition(block.position() + start + length, QTextCursor::KeepAnchor);
                cursor.insertText(suggestion);
            });
            menu->insertAction(first, action);
        }
        if (suggestions.isEmpty()) {
            auto *none = new QAction(QCoreApplication::translate("MarkdownEditor", "No suggestions"), menu.get());
            none->setEnabled(false);
            menu->insertAction(first, none);
        }
        if (addWord) {
            auto *add = new QAction(
                QCoreApplication::translate("MarkdownEditor", "Add “%1” to the dictionary").arg(word),
                menu.get());
            connect(add, &QAction::triggered, this, [this, word] { addWord(word); });
            menu->insertAction(first, add);
        }
        menu->insertSeparator(first);
    }
    menu->exec(event->globalPos());
}
