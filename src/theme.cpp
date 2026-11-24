// Theme watching follows Omacut (MIT, David Heinemeier Hansson).
#include "theme.h"

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <QStyle>
#include <QStyleFactory>
#include <cmath>

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    m_systemPalette = qApp->palette();
    m_systemStyle = qApp->styleSheet();
    if (QStyle *style = qApp->style())
        m_systemStyleName = style->objectName().isEmpty() ? style->name() : style->objectName();

    // Debounce theme-file churn; Omarchy replaces several paths at once.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(120);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &ThemeManager::reloadOmarchy);
}

void ThemeManager::setKind(Kind kind)
{
    const bool changedKind = m_kind != kind;
    m_kind = kind;
    apply();
    if (changedKind)
        emit changed();
}

ThemeManager::Kind ThemeManager::kindFromString(const QString &name)
{
    if (name == QLatin1String("omarchy"))
        return Kind::Omarchy;
    if (name == QLatin1String("minimal"))
        return Kind::Minimal;
    return Kind::System;
}

QString ThemeManager::kindToString(Kind kind)
{
    switch (kind) {
    case Kind::Omarchy:
        return QStringLiteral("omarchy");
    case Kind::Minimal:
        return QStringLiteral("minimal");
    case Kind::System:
    default:
        return QStringLiteral("system");
    }
}

QString ThemeManager::kindLabel(Kind kind)
{
    switch (kind) {
    case Kind::Omarchy:
        return QStringLiteral("Omarchy");
    case Kind::Minimal:
        return QStringLiteral("Minimal");
    case Kind::System:
    default:
        return QStringLiteral("System");
    }
}

bool ThemeManager::omarchyAvailable()
{
    // Single exists() check — cheap on Windows/macOS where the path is absent.
    return QFileInfo::exists(omarchyCurrentDir() + QStringLiteral("/theme/colors.toml"));
}

QString ThemeManager::omarchyCurrentDir()
{
    return QDir::homePath() + QStringLiteral("/.local/state/omarchy/current");
}

void ThemeManager::apply()
{
    switch (m_kind) {
    case Kind::Omarchy:
        armOmarchyWatcher();
        reloadOmarchy();
        break;
    case Kind::Minimal: {
        clearOmarchyWatcher();
        const auto colors = minimalColors();
        applyPalette(paletteFromColors(colors), styleSheetFromColors(colors), fingerprintOf(colors));
        break;
    }
    case Kind::System:
    default:
        clearOmarchyWatcher();
        applySystem();
        break;
    }
}

void ThemeManager::clearOmarchyWatcher()
{
    const QStringList watched = m_watcher.files() + m_watcher.directories();
    if (!watched.isEmpty())
        m_watcher.removePaths(watched);
    m_debounce.stop();
}

void ThemeManager::applySystem()
{
    if (m_appliedKind == Kind::System && m_appliedFingerprint == QLatin1String("system"))
        return;

    if (!m_systemStyleName.isEmpty()) {
        if (QStyle *style = QStyleFactory::create(m_systemStyleName))
            qApp->setStyle(style);
    }
    qApp->setPalette(m_systemPalette);
    qApp->setStyleSheet(m_systemStyle);
    m_appliedKind = Kind::System;
    m_appliedFingerprint = QStringLiteral("system");
}

void ThemeManager::applyPalette(const QPalette &palette, const QString &styleSheet, const QString &fingerprint)
{
    if (m_appliedKind == m_kind && m_appliedFingerprint == fingerprint)
        return;

    // Fusion keeps custom palettes consistent across Windows, macOS, and Linux.
    if (qApp->style() == nullptr || qApp->style()->objectName().compare(QLatin1String("fusion"), Qt::CaseInsensitive) != 0) {
        if (QStyle *fusion = QStyleFactory::create(QStringLiteral("Fusion")))
            qApp->setStyle(fusion);
    }
    qApp->setPalette(palette);
    qApp->setStyleSheet(styleSheet);
    m_appliedKind = m_kind;
    m_appliedFingerprint = fingerprint;
}

void ThemeManager::armOmarchyWatcher()
{
    clearOmarchyWatcher();

    // A theme switch replaces symlinks and files, so watch parents too and
    // re-arm after every change. Watching the nearest existing ancestor means
    // installing Omarchy later works without restarting the app.
    const QString directory = omarchyCurrentDir();
    QString ancestor = directory;
    while (!QFileInfo::exists(ancestor) && ancestor != QLatin1String("/"))
        ancestor = QFileInfo(ancestor).absolutePath();

    QStringList candidates{ancestor, directory, directory + QStringLiteral("/theme"),
                           directory + QStringLiteral("/theme/colors.toml")};
    candidates.removeDuplicates();
    for (const QString &path : candidates) {
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
    }
}

void ThemeManager::reloadOmarchy()
{
    if (m_kind != Kind::Omarchy)
        return;

    armOmarchyWatcher();

    const QString path = omarchyCurrentDir() + QStringLiteral("/theme/colors.toml");
    QHash<QString, QString> colors = readColorsToml(path);
    if (colors.isEmpty())
        colors = omarchyFallbackColors();

    const QString fingerprint = fingerprintOf(colors);
    if (m_appliedKind == Kind::Omarchy && m_appliedFingerprint == fingerprint)
        return;

    applyPalette(paletteFromColors(colors), styleSheetFromColors(colors), fingerprint);
    emit changed();
}

QString ThemeManager::fingerprintOf(const QHash<QString, QString> &colors)
{
    // Include every key used by paletteFromColors / styleSheetFromColors so a
    // change to any of them triggers a repaint.
    return colors.value(QStringLiteral("background")) + QLatin1Char('|')
        + colors.value(QStringLiteral("dark_background")) + QLatin1Char('|')
        + colors.value(QStringLiteral("lighter_background")) + QLatin1Char('|')
        + colors.value(QStringLiteral("foreground")) + QLatin1Char('|')
        + colors.value(QStringLiteral("dark_foreground")) + QLatin1Char('|')
        + colors.value(QStringLiteral("light_foreground")) + QLatin1Char('|')
        + colors.value(QStringLiteral("bright_foreground")) + QLatin1Char('|')
        + colors.value(QStringLiteral("accent")) + QLatin1Char('|')
        + colors.value(QStringLiteral("selection")) + QLatin1Char('|')
        + colors.value(QStringLiteral("muted"));
}

QHash<QString, QString> ThemeManager::readColorsToml(const QString &path)
{
    QHash<QString, QString> colors;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return colors;

    static const QRegularExpression lineRe(
        QStringLiteral("^\\s*([A-Za-z0-9_]+)\\s*=\\s*[\"']?(#[0-9A-Fa-f]{6}|dark|light)[\"']?\\s*$"),
        QRegularExpression::MultilineOption);

    const QString text = QString::fromUtf8(file.readAll());
    auto it = lineRe.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        colors.insert(match.captured(1), match.captured(2));
    }
    return colors;
}

QHash<QString, QString> ThemeManager::minimalColors()
{
    // Quiet light writing surface: near-white paper, near-black ink, one gray accent.
    return {
        {QStringLiteral("mode"), QStringLiteral("light")},
        {QStringLiteral("background"), QStringLiteral("#f7f7f5")},
        {QStringLiteral("dark_background"), QStringLiteral("#efefec")},
        {QStringLiteral("lighter_background"), QStringLiteral("#ffffff")},
        {QStringLiteral("foreground"), QStringLiteral("#1c1c1a")},
        {QStringLiteral("dark_foreground"), QStringLiteral("#6b6b66")},
        {QStringLiteral("light_foreground"), QStringLiteral("#3a3a36")},
        {QStringLiteral("bright_foreground"), QStringLiteral("#111110")},
        {QStringLiteral("accent"), QStringLiteral("#2f2f2b")},
        {QStringLiteral("selection"), QStringLiteral("#e4e4df")},
        {QStringLiteral("muted"), QStringLiteral("#c8c8c2")},
    };
}

QHash<QString, QString> ThemeManager::omarchyFallbackColors()
{
    // Same defaults Omarchy apps use when colors.toml is missing.
    return {
        {QStringLiteral("mode"), QStringLiteral("dark")},
        {QStringLiteral("background"), QStringLiteral("#1a1a1a")},
        {QStringLiteral("dark_background"), QStringLiteral("#121212")},
        {QStringLiteral("lighter_background"), QStringLiteral("#242424")},
        {QStringLiteral("foreground"), QStringLiteral("#e6e6e6")},
        {QStringLiteral("dark_foreground"), QStringLiteral("#888888")},
        {QStringLiteral("light_foreground"), QStringLiteral("#cccccc")},
        {QStringLiteral("bright_foreground"), QStringLiteral("#ffffff")},
        {QStringLiteral("accent"), QStringLiteral("#FFD60A")},
        // Distinct from the pane background so selected text stays readable.
        {QStringLiteral("selection"), QStringLiteral("#3D5A80")},
        {QStringLiteral("muted"), QStringLiteral("#444444")},
    };
}

QString ThemeManager::contrastForeground(const QColor &background)
{
    auto linear = [](double v) {
        return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4);
    };
    const double luminance = 0.2126 * linear(background.redF()) + 0.7152 * linear(background.greenF())
        + 0.0722 * linear(background.blueF());
    return luminance > 0.179 ? QStringLiteral("#111111") : QStringLiteral("#ffffff");
}

QPalette ThemeManager::paletteFromColors(const QHash<QString, QString> &colors) const
{
    const QColor background(colors.value(QStringLiteral("background"), QStringLiteral("#1a1a1a")));
    const QColor foreground(colors.value(QStringLiteral("foreground"), QStringLiteral("#e6e6e6")));
    const QColor base(colors.value(QStringLiteral("lighter_background"), background.name()));
    const QColor alternate(colors.value(QStringLiteral("dark_background"), background.darker(110).name()));
    const QColor accent(colors.value(QStringLiteral("accent"), QStringLiteral("#FFD60A")));
    const QColor selection(colors.value(QStringLiteral("selection"), QStringLiteral("#3D5A80")));
    const QColor muted(colors.value(QStringLiteral("dark_foreground"),
                                    colors.value(QStringLiteral("muted"), QStringLiteral("#888888"))));
    const QColor button(colors.value(QStringLiteral("lighter_background"), background.lighter(115).name()));
    const QColor selectedText(contrastForeground(selection));

    QPalette palette;
    palette.setColor(QPalette::Window, background);
    palette.setColor(QPalette::WindowText, foreground);
    palette.setColor(QPalette::Base, base);
    palette.setColor(QPalette::AlternateBase, alternate);
    palette.setColor(QPalette::Text, foreground);
    palette.setColor(QPalette::Button, button);
    palette.setColor(QPalette::ButtonText, foreground);
    palette.setColor(QPalette::BrightText, colors.value(QStringLiteral("bright_foreground"), foreground.name()));
    palette.setColor(QPalette::Highlight, selection);
    palette.setColor(QPalette::HighlightedText, selectedText);
    palette.setColor(QPalette::Link, accent);
    palette.setColor(QPalette::LinkVisited, accent.darker(110));
    palette.setColor(QPalette::ToolTipBase, alternate);
    palette.setColor(QPalette::ToolTipText, foreground);
    palette.setColor(QPalette::PlaceholderText, muted);
    palette.setColor(QPalette::Disabled, QPalette::WindowText, muted);
    palette.setColor(QPalette::Disabled, QPalette::Text, muted);
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    return palette;
}

QString ThemeManager::styleSheetFromColors(const QHash<QString, QString> &colors) const
{
    // Lean sheet: menus + reading/writing panes. Palette covers the rest.
    const QString background = colors.value(QStringLiteral("background"), QStringLiteral("#1a1a1a"));
    const QString base = colors.value(QStringLiteral("lighter_background"), background);
    const QString foreground = colors.value(QStringLiteral("foreground"), QStringLiteral("#e6e6e6"));
    const QString muted = colors.value(QStringLiteral("dark_foreground"),
                                       colors.value(QStringLiteral("muted"), QStringLiteral("#888888")));
    const QString selection = colors.value(QStringLiteral("selection"), QStringLiteral("#3D5A80"));
    const QString border = colors.value(QStringLiteral("muted"), muted);
    const QString selectedText = contrastForeground(QColor(selection));

    return QStringLiteral(
               "QPlainTextEdit, QTextBrowser {"
               "  background-color: %1; color: %2; border: 1px solid %3;"
               "  selection-background-color: %4; selection-color: %5; padding: 8px;"
               "}"
               "QMenuBar, QMenu { background-color: %6; color: %2; }"
               "QMenu::item:selected, QMenuBar::item:selected {"
               "  background-color: %4; color: %5;"
               "}"
               "QMenu { border: 1px solid %3; }"
               "QLabel { color: %2; }"
               "QPushButton {"
               "  background-color: %1; color: %2; border: 1px solid %3; padding: 4px 10px;"
               "}"
               "QPushButton:hover { border-color: %4; }"
               "QPushButton:disabled { color: %7; }"
               "QSplitter::handle { background: %3; width: 1px; height: 1px; }"
               "QScrollBar:vertical { background: %6; width: 10px; margin: 0; }"
               "QScrollBar::handle:vertical { background: %3; min-height: 24px; }"
               "QScrollBar:horizontal { background: %6; height: 10px; margin: 0; }"
               "QScrollBar::handle:horizontal { background: %3; min-width: 24px; }")
        .arg(base, foreground, border, selection, selectedText, background, muted);
}
