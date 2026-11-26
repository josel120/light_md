#include "editor.h"

#include "spellcheck.h"

#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QFont>
#include <QMenu>
#include <QTextBlock>
#include <QTextLayout>
#include <memory>

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
