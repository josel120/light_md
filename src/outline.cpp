#include "outline.h"

#include <QCoreApplication>
#include <QHeaderView>
#include <QRegularExpression>
#include <QSignalBlocker>

namespace Outline {

namespace {

QString withoutCarriageReturn(QString line)
{
    if (line.endsWith(QLatin1Char('\r')))
        line.chop(1);
    return line;
}

} // namespace

QString plainText(QString text)
{
    // Only symbols that come in pairs are formatting; a lone * ("Pointers (T*)") is text.
    static const QRegularExpression imageOrLinkRe(QStringLiteral("!?\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression codeRe(QStringLiteral("`+([^`]*)`+"));
    static const QRegularExpression strongRe(QStringLiteral("(\\*\\*|__)(?=\\S)(.+?)(?<=\\S)\\1"));
    static const QRegularExpression emphasisRe(QStringLiteral("(?<![\\w*])([*_])(?=\\S)(.+?)(?<=\\S)\\1(?![\\w*])"));
    static const QRegularExpression strikeRe(QStringLiteral("~~(.+?)~~"));
    text.replace(imageOrLinkRe, QStringLiteral("\\1"));
    text.replace(codeRe, QStringLiteral("\\1"));
    text.replace(strongRe, QStringLiteral("\\2"));
    text.replace(emphasisRe, QStringLiteral("\\2"));
    text.replace(strikeRe, QStringLiteral("\\1"));
    return text.simplified();
}

QList<Heading> headings(const QString &markdown)
{
    static const QRegularExpression fenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})(.*)$"));
    static const QRegularExpression quoteRe(QStringLiteral("^ {0,3}(?:> ?)+"));
    // "# Title", optionally closed with #s ("## Title ##").
    static const QRegularExpression atxRe(QStringLiteral("^ {0,3}(#{1,6})(?:[ \\t]+(.*?))?(?:[ \\t]+#+)?[ \\t]*$"));
    static const QRegularExpression setextRe(QStringLiteral("^ {0,3}(=+|-+)[ \\t]*$"));
    // A separator line (*** or - - -), which is never the text of a title.
    static const QRegularExpression ruleRe(QStringLiteral("^ {0,3}([-*_])(?:[ \\t]*\\1){2,}[ \\t]*$"));
    // Lines that can't be the text of an underlined title: lists, quotes, tables, indented code.
    static const QRegularExpression notParagraphRe(
        QStringLiteral("^(?: {4}|\\t| {0,3}(?:[-*+]|\\d{1,9}[.)])(?:[ \\t]|$)| {0,3}>| {0,3}#|\\s*\\|)"));

    // A line that opens a code block: ``` or ~~~, and after ``` no other backtick
    // (```code``` at the start of a line is inline code).
    const auto opensFence = [](const QRegularExpressionMatch &m) {
        return m.hasMatch() && !(m.captured(1).at(0) == QLatin1Char('`') && m.captured(2).contains(QLatin1Char('`')));
    };
    // Lines that can be part of the paragraph an underline turns into a title.
    const auto paragraphLine = [&](const QString &line) {
        return !line.trimmed().isEmpty() && !notParagraphRe.match(line).hasMatch() && !opensFence(fenceRe.match(line))
            && !setextRe.match(line).hasMatch() && !ruleRe.match(line).hasMatch();
    };

    QList<Heading> result;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QString fence;
    for (int i = 0; i < lines.size(); ++i) {
        const QString line = withoutCarriageReturn(lines.at(i));
        const QRegularExpressionMatch fenceMatch = fenceRe.match(line);
        if (!fence.isEmpty()) {
            if (fenceMatch.hasMatch() && fenceMatch.captured(1).at(0) == fence.at(0)
                && fenceMatch.capturedLength(1) >= fence.size() && fenceMatch.captured(2).trimmed().isEmpty())
                fence.clear();
            continue;
        }
        if (opensFence(fenceMatch)) {
            fence = fenceMatch.captured(1);
            continue;
        }
        // Titles inside quotes ("> # Notes") are titles in the preview too.
        const QRegularExpressionMatch quote = quoteRe.match(line);
        const QString content = quote.hasMatch() ? line.mid(quote.capturedEnd(0)) : line;
        const QRegularExpressionMatch atx = atxRe.match(content);
        if (atx.hasMatch()) {
            result.append({int(atx.capturedLength(1)), plainText(atx.captured(2)), i});
            continue;
        }
        // An underline turns the paragraph above (one or more lines) into a title.
        const QRegularExpressionMatch setext = setextRe.match(line);
        // ("---" under text is an underline, not a separator line.)
        if (!setext.hasMatch() || i == 0)
            continue;
        const bool alreadyTitle = !result.isEmpty() && result.constLast().line == i - 1;
        if (alreadyTitle || !paragraphLine(withoutCarriageReturn(lines.at(i - 1))))
            continue;
        int first = i - 1;
        while (first > 0 && paragraphLine(withoutCarriageReturn(lines.at(first - 1))))
            --first;
        QStringList text;
        for (int j = first; j < i; ++j)
            text.append(withoutCarriageReturn(lines.at(j)));
        result.append({setext.captured(1).at(0) == QLatin1Char('=') ? 1 : 2,
                       plainText(text.join(QLatin1Char(' '))), first});
    }
    return result;
}

} // namespace Outline

OutlinePanel::OutlinePanel(QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(1);
    setHeaderLabel(QCoreApplication::translate("OutlinePanel", "Outline"));
    header()->setSectionResizeMode(QHeaderView::Stretch);
    setIndentation(14);
    setRootIsDecorated(false);
    setUniformRowHeights(true);
    setFocusPolicy(Qt::NoFocus);
    setTextElideMode(Qt::ElideRight);
    setToolTip(QCoreApplication::translate("OutlinePanel", "Click a title to go to it"));
    connect(this, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *item) {
        const int index = int(m_items.indexOf(item));
        if (index >= 0 && jump)
            jump(index, m_headings.at(index).line);
    });
}

void OutlinePanel::setHeadings(const QList<Outline::Heading> &headings)
{
    if (headings == m_headings)
        return;
    m_headings = headings;
    clear();
    m_items.clear();
    // Each title goes under the nearest title above it with a smaller level.
    QList<QPair<int, QTreeWidgetItem *>> parents;
    for (const Outline::Heading &heading : headings) {
        while (!parents.isEmpty() && parents.constLast().first >= heading.level)
            parents.removeLast();
        auto *item = parents.isEmpty() ? new QTreeWidgetItem(this) : new QTreeWidgetItem(parents.constLast().second);
        item->setText(0, heading.text.isEmpty() ? QStringLiteral("…") : heading.text);
        item->setToolTip(0, heading.text);
        if (heading.level == 1) {
            QFont bold = item->font(0);
            bold.setBold(true);
            item->setFont(0, bold);
        }
        parents.append({heading.level, item});
        m_items.append(item);
    }
    expandAll();
}

void OutlinePanel::setCurrentLine(int line)
{
    int current = -1;
    for (int i = 0; i < m_headings.size() && m_headings.at(i).line <= line; ++i)
        current = i;
    QSignalBlocker blocker(this);
    if (current < 0) {
        clearSelection();
        setCurrentItem(nullptr);
        return;
    }
    setCurrentItem(m_items.at(current));
    scrollToItem(m_items.at(current));
}
