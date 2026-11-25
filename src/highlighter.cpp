#include "highlighter.h"

#include <QFont>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QVector>

namespace {

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount,
                            a.greenF() + (b.greenF() - a.greenF()) * amount,
                            a.blueF() + (b.blueF() - a.blueF()) * amount);
}

} // namespace

MarkdownHighlighter::MarkdownHighlighter(QTextDocument *document)
    : QSyntaxHighlighter(document)
{
}

void MarkdownHighlighter::setStyle(const QPalette &palette, qreal pointSize)
{
    const QColor text = palette.color(QPalette::Text);
    const QColor link = palette.color(QPalette::Link);
    const QColor muted = palette.color(QPalette::PlaceholderText);
    // A faint tint of the ink over the paper: readable in light and dark themes alike.
    const QColor codeBackground = mix(palette.color(QPalette::Base), text, 0.09);
    if (text == m_text && link == m_link && muted == m_muted && codeBackground == m_codeBackground
        && qFuzzyCompare(pointSize, m_pointSize))
        return;

    m_text = text;
    m_link = link;
    m_muted = muted;
    m_codeBackground = codeBackground;
    m_pointSize = pointSize;
    buildFormats();
    rehighlight();
}

void MarkdownHighlighter::buildFormats()
{
    // Big title, Subtitle, then smaller steps; levels 5 and 6 are just bold.
    static const qreal headingScale[6] = {1.6, 1.35, 1.15, 1.05, 1.0, 1.0};
    for (int level = 0; level < 6; ++level) {
        QTextCharFormat heading;
        heading.setFontWeight(QFont::Bold);
        if (m_pointSize > 0)
            heading.setFontPointSize(m_pointSize * headingScale[level]);
        m_headings[level] = heading;
    }

    m_marker = QTextCharFormat();
    m_marker.setForeground(m_muted);

    m_listMarker = QTextCharFormat();
    m_listMarker.setForeground(m_link);
    m_listMarker.setFontWeight(QFont::Bold);

    m_quote = QTextCharFormat();
    m_quote.setForeground(m_muted);

    m_done = QTextCharFormat();
    m_done.setForeground(m_muted);
    m_done.setFontStrikeOut(true);

    m_code = QTextCharFormat();
    m_code.setFontFamilies(QFontDatabase::systemFont(QFontDatabase::FixedFont).families());
    m_code.setBackground(m_codeBackground);

    // Underlined as well, so links stand out even where the theme's link color is close to the ink.
    m_linkText = QTextCharFormat();
    m_linkText.setForeground(m_link);
    m_linkText.setFontUnderline(true);

    m_bold = QTextCharFormat();
    m_bold.setFontWeight(QFont::Bold);

    m_italic = QTextCharFormat();
    m_italic.setFontItalic(true);

    m_strike = QTextCharFormat();
    m_strike.setFontStrikeOut(true);
}

// Adds `format` on top of what is already there (a bold word inside a title stays big).
void MarkdownHighlighter::mergeFormat(int start, int count, const QTextCharFormat &format)
{
    const int end = start + count;
    int i = start;
    while (i < end) {
        QTextCharFormat run = this->format(i);
        int j = i + 1;
        while (j < end && this->format(j) == run)
            ++j;
        run.merge(format);
        setFormat(i, j - i, run);
        i = j;
    }
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    static const QRegularExpression fenceRe(QStringLiteral("^\\s{0,3}(`{3,}|~{3,})"));
    static const QRegularExpression headingRe(QStringLiteral("^\\s{0,3}(#{1,6})(?:\\s+|$)"));
    static const QRegularExpression ruleRe(QStringLiteral("^\\s{0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
    static const QRegularExpression quoteRe(QStringLiteral("^\\s{0,3}(?:>\\s?)+"));
    static const QRegularExpression listRe(
        QStringLiteral("^\\s*([-*+]|\\d{1,9}[.)])\\s+(\\[[ xX]\\](?=\\s|$))?"));

    // Code blocks: everything between ``` (or ~~~) lines is code, nothing inside is formatted.
    const int previous = previousBlockState();
    const QRegularExpressionMatch fence = fenceRe.match(text);
    if (previous == InBacktickFence || previous == InTildeFence) {
        const bool closes = fence.hasMatch()
            && fence.captured(1).at(0) == (previous == InBacktickFence ? QLatin1Char('`') : QLatin1Char('~'));
        setFormat(0, text.size(), m_code);
        if (closes)
            mergeFormat(0, text.size(), m_marker);
        setCurrentBlockState(closes ? Normal : previous);
        return;
    }
    if (fence.hasMatch()) {
        setFormat(0, text.size(), m_code);
        mergeFormat(0, text.size(), m_marker);
        setCurrentBlockState(fence.captured(1).at(0) == QLatin1Char('`') ? InBacktickFence : InTildeFence);
        return;
    }
    setCurrentBlockState(Normal);

    if (ruleRe.match(text).hasMatch()) {
        setFormat(0, text.size(), m_marker);
        return;
    }

    int inlineFrom = 0;
    const QRegularExpressionMatch heading = headingRe.match(text);
    if (heading.hasMatch()) {
        setFormat(0, text.size(), m_headings[heading.capturedLength(1) - 1]);
        mergeFormat(0, heading.capturedEnd(1), m_marker);
        inlineFrom = heading.capturedEnd(0);
    } else {
        const QRegularExpressionMatch quote = quoteRe.match(text);
        if (quote.hasMatch()) {
            setFormat(0, text.size(), m_quote);
            inlineFrom = quote.capturedEnd(0);
        }
        const QRegularExpressionMatch list = listRe.match(text, inlineFrom);
        if (list.hasMatch() && list.capturedStart(0) == inlineFrom) {
            mergeFormat(list.capturedStart(1), list.capturedLength(1), m_listMarker);
            if (list.capturedLength(2) > 0) {
                mergeFormat(list.capturedStart(2), list.capturedLength(2), m_listMarker);
                // A ticked box: the rest of the line is crossed out and dimmed.
                if (list.captured(2).at(1) != QLatin1Char(' '))
                    mergeFormat(list.capturedEnd(0), text.size() - list.capturedEnd(0), m_done);
            }
            inlineFrom = list.capturedEnd(0);
        }
    }

    highlightInline(text, inlineFrom);
}

void MarkdownHighlighter::highlightInline(const QString &text, int from)
{
    static const QRegularExpression codeRe(QStringLiteral("(`+)(?!`)(.+?)(?<!`)\\1(?!`)"));
    static const QRegularExpression linkRe(QStringLiteral("!?\\[([^\\]]*)\\]\\(([^)]*)\\)"));
    static const QRegularExpression autoLinkRe(QStringLiteral("<(?:https?://|mailto:)[^>\\s]+>"));
    static const QRegularExpression boldRe(QStringLiteral("(\\*\\*|__)(?=\\S)(.+?)(?<=\\S)\\1"));
    static const QRegularExpression italicStarRe(
        QStringLiteral("(?<![*\\\\])\\*(?![\\s*])(.+?)(?<![\\s*\\\\])\\*(?!\\*)"));
    static const QRegularExpression italicUnderscoreRe(
        QStringLiteral("(?<![\\w_])_(?![\\s_])(.+?)(?<![\\s_])_(?![\\w_])"));
    static const QRegularExpression strikeRe(QStringLiteral("~~(?=\\S)(.+?)(?<=\\S)~~"));

    // Fast path: plain sentences have none of the inline symbols.
    bool hasSymbols = false;
    for (int i = from; i < text.size() && !hasSymbols; ++i) {
        const QChar c = text.at(i);
        hasSymbols = c == QLatin1Char('`') || c == QLatin1Char('[') || c == QLatin1Char('<')
            || c == QLatin1Char('*') || c == QLatin1Char('_') || c == QLatin1Char('~');
    }
    if (!hasSymbols)
        return;

    // Inside `code` nothing else counts, so remember where the code is.
    QVector<QPair<int, int>> code;
    auto insideCode = [&code](int start, int end) {
        for (const auto &range : code) {
            if (start < range.second && end > range.first)
                return true;
        }
        return false;
    };

    auto codeIt = codeRe.globalMatch(text, from);
    while (codeIt.hasNext()) {
        const QRegularExpressionMatch m = codeIt.next();
        mergeFormat(m.capturedStart(0), m.capturedLength(0), m_code);
        mergeFormat(m.capturedStart(1), m.capturedLength(1), m_marker);
        mergeFormat(m.capturedEnd(2), m.capturedEnd(0) - m.capturedEnd(2), m_marker);
        code.append({m.capturedStart(0), m.capturedEnd(0)});
    }

    auto emphasis = [&](const QRegularExpression &re, const QTextCharFormat &format, int markerLength) {
        auto it = re.globalMatch(text, from);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (insideCode(m.capturedStart(0), m.capturedEnd(0)))
                continue;
            const int length = markerLength > 0 ? markerLength : m.capturedLength(1);
            mergeFormat(m.capturedStart(0), m.capturedLength(0), format);
            mergeFormat(m.capturedStart(0), length, m_marker);
            mergeFormat(m.capturedEnd(0) - length, length, m_marker);
        }
    };
    emphasis(boldRe, m_bold, 0);
    emphasis(italicStarRe, m_italic, 1);
    emphasis(italicUnderscoreRe, m_italic, 1);
    emphasis(strikeRe, m_strike, 2);

    // Links last, so their color wins over bold or italic around them.
    auto linkIt = linkRe.globalMatch(text, from);
    while (linkIt.hasNext()) {
        const QRegularExpressionMatch m = linkIt.next();
        if (insideCode(m.capturedStart(0), m.capturedEnd(0)))
            continue;
        mergeFormat(m.capturedStart(0), m.capturedLength(0), m_marker);
        mergeFormat(m.capturedStart(1), m.capturedLength(1), m_linkText);
    }
    auto autoIt = autoLinkRe.globalMatch(text, from);
    while (autoIt.hasNext()) {
        const QRegularExpressionMatch m = autoIt.next();
        if (!insideCode(m.capturedStart(0), m.capturedEnd(0)))
            mergeFormat(m.capturedStart(0), m.capturedLength(0), m_linkText);
    }
}
