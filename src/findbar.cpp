#include "findbar.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTextCursor>

FindBar::FindBar(QPlainTextEdit *editor, QWidget *parent)
    : QWidget(parent)
    , m_editor(editor)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_findEdit = new QLineEdit(this);
    m_findEdit->setPlaceholderText(QStringLiteral("Word to find"));
    m_findEdit->setClearButtonEnabled(true);
    auto *btnPrevious = new QPushButton(QStringLiteral("Previous"), this);
    auto *btnNext = new QPushButton(QStringLiteral("Next"), this);
    m_replaceEdit = new QLineEdit(this);
    m_replaceEdit->setPlaceholderText(QStringLiteral("Replace with"));
    m_replaceEdit->setClearButtonEnabled(true);
    auto *btnReplace = new QPushButton(QStringLiteral("Replace"), this);
    auto *btnReplaceAll = new QPushButton(QStringLiteral("Replace all"), this);
    m_matchCase = new QCheckBox(QStringLiteral("Match case"), this);
    m_info = new QLabel(this);
    auto *btnClose = new QPushButton(QStringLiteral("Close"), this);
    btnPrevious->setToolTip(QStringLiteral("Find the previous match (Shift+F3)"));
    btnNext->setToolTip(QStringLiteral("Find the next match (Enter or F3)"));
    btnClose->setToolTip(QStringLiteral("Hide the find bar (Esc)"));
    layout->addWidget(new QLabel(QStringLiteral("Find:"), this));
    layout->addWidget(m_findEdit, 1);
    layout->addWidget(btnPrevious);
    layout->addWidget(btnNext);
    layout->addSpacing(12);
    layout->addWidget(m_replaceEdit, 1);
    layout->addWidget(btnReplace);
    layout->addWidget(btnReplaceAll);
    layout->addSpacing(12);
    layout->addWidget(m_matchCase);
    layout->addWidget(m_info);
    layout->addWidget(btnClose);
    hide();

    connect(btnClose, &QPushButton::clicked, this, [this] { closeBar(); });
    connect(btnNext, &QPushButton::clicked, this, [this] { findNext(false); });
    connect(btnPrevious, &QPushButton::clicked, this, [this] { findNext(true); });
    connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { findNext(false); });
    connect(m_findEdit, &QLineEdit::textEdited, this, [this] { m_info->clear(); });
    connect(m_replaceEdit, &QLineEdit::returnPressed, this, [this] { replaceOne(); });
    connect(btnReplace, &QPushButton::clicked, this, [this] { replaceOne(); });
    connect(btnReplaceAll, &QPushButton::clicked, this, [this] { replaceAll(); });
}

QString FindBar::info() const
{
    return m_info->text();
}

void FindBar::open(QLineEdit *field)
{
    if (beforeSearch)
        beforeSearch();
    const QTextCursor cursor = m_editor->textCursor();
    // Start from the selected word, unless it spans lines.
    if (cursor.hasSelection() && !cursor.selectedText().contains(QChar::ParagraphSeparator))
        m_findEdit->setText(cursor.selectedText());
    m_info->clear();
    show();
    field->setFocus();
    field->selectAll();
}

void FindBar::closeBar()
{
    hide();
    m_editor->setFocus();
}

QTextDocument::FindFlags FindBar::flags(bool backward) const
{
    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    if (m_matchCase->isChecked())
        flags |= QTextDocument::FindCaseSensitively;
    return flags;
}

bool FindBar::findNext(bool backward)
{
    const QString text = m_findEdit->text();
    if (text.isEmpty()) {
        openFind();
        return false;
    }
    if (beforeSearch)
        beforeSearch();
    m_info->clear();
    const QTextDocument::FindFlags findFlags = flags(backward);
    if (m_editor->find(text, findFlags)) {
        if (matchFound)
            matchFound();
        return true;
    }
    // Nothing more in this direction: continue from the other end of the document.
    const QTextCursor saved = m_editor->textCursor();
    QTextCursor cursor = saved;
    cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
    m_editor->setTextCursor(cursor);
    if (m_editor->find(text, findFlags)) {
        m_info->setText(backward ? QStringLiteral("Continued from the end") : QStringLiteral("Continued from the top"));
        if (matchFound)
            matchFound();
        return true;
    }
    m_editor->setTextCursor(saved);
    m_info->setText(QStringLiteral("Not found"));
    return false;
}

void FindBar::replaceOne()
{
    const QString text = m_findEdit->text();
    if (text.isEmpty())
        return;
    QTextCursor cursor = m_editor->textCursor();
    const Qt::CaseSensitivity cs = m_matchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (cursor.hasSelection() && cursor.selectedText().compare(text, cs) == 0) {
        cursor.insertText(m_replaceEdit->text());
        m_editor->setTextCursor(cursor);
    }
    findNext(false);
}

int FindBar::replaceAll()
{
    const QString text = m_findEdit->text();
    if (text.isEmpty())
        return 0;
    QTextDocument *doc = m_editor->document();
    const QTextDocument::FindFlags findFlags = flags(false);
    int count = 0;
    QTextCursor edit(doc);
    edit.beginEditBlock();
    // Searching from after each replacement avoids looping when the new text contains the old one.
    for (QTextCursor found = doc->find(text, 0, findFlags); !found.isNull(); found = doc->find(text, found, findFlags)) {
        found.insertText(m_replaceEdit->text());
        ++count;
    }
    edit.endEditBlock();
    m_info->setText(count == 0 ? QStringLiteral("Not found")
                    : count == 1 ? QStringLiteral("1 replacement")
                                 : QStringLiteral("%1 replacements").arg(count));
    return count;
}
