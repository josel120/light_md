#pragma once

#include <QTextDocument>
#include <QWidget>
#include <functional>

class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;

// The find and replace bar under the panes. Hidden until the user asks for it.
class FindBar : public QWidget
{
public:
    FindBar(QPlainTextEdit *editor, QWidget *parent);

    // Called before searching, so the window can show the writing pane (matches are selected there).
    std::function<void()> beforeSearch;
    // Called when a match moved the writing pane.
    std::function<void()> matchFound;

    void openFind() { open(m_findEdit); }
    void openReplace() { open(m_replaceEdit); }
    // Hides the bar and gives the writing pane its focus back.
    void closeBar();

    // Selects the next (or previous) match, continuing from the other end when needed.
    bool findNext(bool backward);
    // The first call finds the word; the next one replaces it and moves on.
    void replaceOne();
    // Returns how many were replaced, as one undo step.
    int replaceAll();

    QLineEdit *findField() const { return m_findEdit; }
    QLineEdit *replaceField() const { return m_replaceEdit; }
    QCheckBox *matchCaseBox() const { return m_matchCase; }
    QString info() const;

private:
    void open(QLineEdit *field);
    QTextDocument::FindFlags flags(bool backward) const;

    QPlainTextEdit *m_editor;
    QLineEdit *m_findEdit;
    QLineEdit *m_replaceEdit;
    QCheckBox *m_matchCase;
    QLabel *m_info;
};
