#include "outline.h"

#include <QCoreApplication>
#include <QHeaderView>
#include <QRegularExpression>
#include <QSignalBlocker>

namespace Outline {

namespace {

// The title as the reader sees it: no **, *, _, `, ~~, and [text](address) becomes text.
QString plainText(QString text)
{
    static const QRegularExpression imageOrLinkRe(QStringLiteral("!?\\[([^\\]]*)\\]\\([^)]*\\)"));
    static const QRegularExpression markRe(QStringLiteral("(\\*\\*|__|~~|`|\\*|(?<!\\w)_|_(?!\\w))"));
    text.replace(imageOrLinkRe, QStringLiteral("\\1"));
    text.remove(markRe);
    return text.simplified();
}

} // namespace

QList<Heading> headings(const QString &markdown)
{
    static const QRegularExpression fenceRe(QStringLiteral("^ {0,3}(`{3,}|~{3,})"));
    // "# Title", optionally closed with #s ("## Title ##").
    static const QRegularExpression atxRe(QStringLiteral("^ {0,3}(#{1,6})(?:[ \\t]+(.*?))?(?:[ \\t]+#+)?[ \\t]*$"));
    static const QRegularExpression setextRe(QStringLiteral("^ {0,3}(=+|-+)[ \\t]*$"));
    // Lines that can't be the text of an underlined title: lists, quotes, tables, indented code.
    static const QRegularExpression notParagraphRe(
        QStringLiteral("^(?: {4}|\\t| {0,3}(?:[-*+]|\\d{1,9}[.)])(?:[ \\t]|$)| {0,3}>| {0,3}#|\\s*\\|)"));

    QList<Heading> result;
    const QStringList lines = markdown.split(QLatin1Char('\n'));
    QString fence;
    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines.at(i);
        if (line.endsWith(QLatin1Char('\r')))
            line.chop(1);
        const QRegularExpressionMatch fenceMatch = fenceRe.match(line);
        if (!fence.isEmpty()) {
            if (fenceMatch.hasMatch() && fenceMatch.captured(1).at(0) == fence.at(0)
                && fenceMatch.capturedLength(1) >= fence.size()
                && line.trimmed().size() == fenceMatch.capturedLength(1))
                fence.clear();
            continue;
        }
        if (fenceMatch.hasMatch()) {
            fence = fenceMatch.captured(1);
            continue;
        }
        const QRegularExpressionMatch atx = atxRe.match(line);
        if (atx.hasMatch()) {
            result.append({int(atx.capturedLength(1)), plainText(atx.captured(2)), i});
            continue;
        }
        // An underline turns the line above into a title, if that line is ordinary text.
        const QRegularExpressionMatch setext = setextRe.match(line);
        if (setext.hasMatch() && i > 0) {
            QString above = lines.at(i - 1);
            if (above.endsWith(QLatin1Char('\r')))
                above.chop(1);
            const bool alreadyTitle = !result.isEmpty() && result.constLast().line == i - 1;
            if (!above.trimmed().isEmpty() && !alreadyTitle && !notParagraphRe.match(above).hasMatch()
                && !fenceRe.match(above).hasMatch() && !setextRe.match(above).hasMatch()) {
                result.append({setext.captured(1).at(0) == QLatin1Char('=') ? 1 : 2, plainText(above), i - 1});
            }
        }
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
