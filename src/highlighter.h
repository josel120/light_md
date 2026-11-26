#pragma once

#include <QPalette>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

class SpellChecker;

// Colors the writing pane: titles bigger, bold in bold, italic in italic, links and code
// set apart, quotes dimmed. The Markdown symbols stay visible, just quieter.
//
// Colors come from the editor's palette, so every theme (System, Minimal, Dark, Omarchy)
// works without its own color list. Only changed lines are re-colored while typing.
class MarkdownHighlighter : public QSyntaxHighlighter
{
public:
    explicit MarkdownHighlighter(QTextDocument *document);

    // Call after a theme or text size change; re-colors only when something differs.
    void setStyle(const QPalette &palette, qreal pointSize);

    // Misspelled words get a wavy underline (not in code, links' addresses or web addresses).
    // Null, or a checker that is off, underlines nothing. Call rehighlight() after a change.
    void setSpellChecker(const SpellChecker *speller) { m_speller = speller; }

protected:
    void highlightBlock(const QString &text) override;

private:
    // Outside a code block the state is Normal. Inside one it records the fence: ` or ~,
    // how many of them opened it (a closing fence needs at least as many) and how many
    // quote levels it sits in (the block ends when the quote does). Keeping all of that in
    // the state means editing the opening fence re-colors the lines after it.
    enum State { Normal = -1 };
    static int fenceState(QChar marker, int length, int quoteDepth);

    void buildFormats();
    void mergeFormat(int start, int count, const QTextCharFormat &format);
    void highlightInline(const QString &text, int from);
    void underlineMisspelled(const QString &text, int from);

    QColor m_text;
    QColor m_link;
    QColor m_muted;
    QColor m_codeBackground;
    qreal m_pointSize = 0;

    QTextCharFormat m_headings[6];
    QTextCharFormat m_marker;
    QTextCharFormat m_listMarker;
    QTextCharFormat m_quote;
    QTextCharFormat m_done;
    QTextCharFormat m_code;
    QTextCharFormat m_linkText;
    QTextCharFormat m_bold;
    QTextCharFormat m_italic;
    QTextCharFormat m_strike;
    QTextCharFormat m_misspelled;
    const SpellChecker *m_speller = nullptr;
};
