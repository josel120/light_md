#pragma once

#include <QList>
#include <QString>
#include <QTreeWidget>
#include <functional>

// The document's titles and subtitles, for the outline panel.
namespace Outline {

struct Heading
{
    int level = 1;    // 1 for "# Title", 2 for "## Subtitle", ... up to 6
    QString text;     // as shown: without the # marks or the bold/italic/link symbols
    int line = 0;     // where it is in the text (0 = first line)

    bool operator==(const Heading &other) const
    {
        return level == other.level && text == other.text && line == other.line;
    }
};

// "# Title" lines and underlined titles (=== or --- below a line), leaving out code blocks.
QList<Heading> headings(const QString &markdown);

} // namespace Outline

// The side panel: titles indented under the title they belong to. Clicking one calls `jump`
// with its place in the list and its line.
class OutlinePanel : public QTreeWidget
{
public:
    explicit OutlinePanel(QWidget *parent = nullptr);

    // Rebuilds the list only when the titles changed, so typing in a paragraph costs nothing.
    void setHeadings(const QList<Outline::Heading> &headings);
    const QList<Outline::Heading> &headings() const { return m_headings; }
    // Marks the section the cursor is in: the last title at or above `line`.
    void setCurrentLine(int line);

    std::function<void(int index, int line)> jump;

private:
    QList<Outline::Heading> m_headings;
    QList<QTreeWidgetItem *> m_items;
};
