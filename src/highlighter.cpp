#include "highlighter.h"

#include <QFont>
#include <QFontDatabase>
#include <QRegularExpression>

namespace {

QColor mix(const QColor &a, const QColor &b, qreal amount)
{
    return QColor::fromRgbF(a.redF() + (b.redF() - a.redF()) * amount,
                            a.greenF() + (b.greenF() - a.greenF()) * amount,
                            a.blueF() + (b.blueF() - a.blueF()) * amount);
}

// Skips up to `maxDepth` quote markers ("> ") at the start of a line. Returns where the
// rest of the line starts and puts the number of markers skipped in `depth`.
int skipQuotes(const QString &text, int maxDepth, int *depth)
{
    int pos = 0;
    *depth = 0;
    while (*depth < maxDepth) {
        int i = pos;
        while (i < text.size() && i - pos < 3 && text.at(i) == QLatin1Char(' '))
            ++i;
        if (i >= text.size() || text.at(i) != QLatin1Char('>'))
            break;
        ++i;
        if (i < text.size() && text.at(i).isSpace())
            ++i;
        pos = i;
        ++*depth;
    }
    return pos;
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

int MarkdownHighlighter::fenceState(QChar marker, int length, int quoteDepth)
{
    return (qMin(quoteDepth, 255) << 16) | (qMin(length, 0x7fff) << 1)
        | (marker == QLatin1Char('~') ? 1 : 0);
}

void MarkdownHighlighter::highlightBlock(const QString &text)
{
    static const QRegularExpression openFenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})(.*)$"));
    static const QRegularExpression closeFenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})\\s*$"));
    static const QRegularExpression headingRe(QStringLiteral("^\\s{0,3}(#{1,6})(?:\\s+|$)"));
    static const QRegularExpression ruleRe(QStringLiteral("^\\s{0,3}([-*_])(?:\\s*\\1){2,}\\s*$"));
    static const QRegularExpression quoteRe(QStringLiteral("^\\s{0,3}(?:>\\s?)+"));
    static const QRegularExpression listRe(
        QStringLiteral("^\\s*([-*+]|\\d{1,9}[.)])\\s+(\\[[ xX]\\](?=\\s|$))?"));

    // Code blocks: everything between ``` (or ~~~) lines is code, nothing inside is formatted.
    const int previous = previousBlockState();
    if (previous != Normal) {
        const QChar marker = (previous & 1) ? QLatin1Char('~') : QLatin1Char('`');
        const int length = (previous >> 1) & 0x7fff;
        const int quoteDepth = previous >> 16;
        int depth = 0;
        const int contentStart = skipQuotes(text, quoteDepth, &depth);
        // A code block inside a quote ends with the quote; then this is an ordinary line.
        if (depth == quoteDepth) {
            setFormat(0, contentStart, m_quote);
            setFormat(contentStart, text.size() - contentStart, m_code);
            // Closes only with the same symbol, at least as many of them, and nothing after.
            const QRegularExpressionMatch close = closeFenceRe.match(text.mid(contentStart));
            const bool closes = close.hasMatch() && close.captured(1).at(0) == marker
                && close.capturedLength(1) >= length;
            if (closes)
                mergeFormat(contentStart, text.size() - contentStart, m_marker);
            setCurrentBlockState(closes ? Normal : previous);
            return;
        }
    }

    int quoteDepth = 0;
    const int contentStart = skipQuotes(text, 255, &quoteDepth);
    const QRegularExpressionMatch fence = openFenceRe.match(text.mid(contentStart));
    const QChar marker = fence.hasMatch() ? fence.captured(1).at(0) : QChar();
    // ```code``` at the start of a line is inline code, not a fence.
    if (fence.hasMatch() && !(marker == QLatin1Char('`') && fence.captured(2).contains(marker))) {
        setFormat(0, contentStart, m_quote);
        setFormat(contentStart, text.size() - contentStart, m_code);
        mergeFormat(contentStart, text.size() - contentStart, m_marker);
        setCurrentBlockState(fenceState(marker, fence.capturedLength(1), quoteDepth));
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

    // Symbols inside `code` don't count, but code can sit inside bold or a link. So the
    // other rules look at a copy where each code span is blanked out with a neutral
    // character: they can't start or end in it, yet can still wrap around it.
    QString masked = text;
    auto codeIt = codeRe.globalMatch(text, from);
    while (codeIt.hasNext()) {
        const QRegularExpressionMatch m = codeIt.next();
        mergeFormat(m.capturedStart(0), m.capturedLength(0), m_code);
        mergeFormat(m.capturedStart(1), m.capturedLength(1), m_marker);
        mergeFormat(m.capturedEnd(2), m.capturedEnd(0) - m.capturedEnd(2), m_marker);
        for (int i = m.capturedStart(0); i < m.capturedEnd(0); ++i)
            masked[i] = QChar(0xFFFC);
    }

    auto emphasis = [&](const QRegularExpression &re, const QTextCharFormat &format, int markerLength) {
        auto it = re.globalMatch(masked, from);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
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
    auto linkIt = linkRe.globalMatch(masked, from);
    while (linkIt.hasNext()) {
        const QRegularExpressionMatch m = linkIt.next();
        mergeFormat(m.capturedStart(0), m.capturedLength(0), m_marker);
        mergeFormat(m.capturedStart(1), m.capturedLength(1), m_linkText);
    }
    auto autoIt = autoLinkRe.globalMatch(masked, from);
    while (autoIt.hasNext()) {
        const QRegularExpressionMatch m = autoIt.next();
        mergeFormat(m.capturedStart(0), m.capturedLength(0), m_linkText);
    }
}
