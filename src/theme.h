#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QPalette>
#include <QString>
#include <QTimer>

// Applies one of three looks: the desktop/Qt default, a quiet built-in
// Minimal palette, or the live Omarchy theme (colors.toml). Theme watching
// follows Omacut (MIT, David Heinemeier Hansson).
//
// Designed to stay cheap: System does almost nothing, Omarchy watching is
// armed only while that theme is selected, and identical color sets are not
// reapplied.
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum class Kind {
        System,
        Omarchy,
        Minimal,
    };
    Q_ENUM(Kind)

    explicit ThemeManager(QObject *parent = nullptr);

    Kind kind() const { return m_kind; }
    void setKind(Kind kind);

    static Kind kindFromString(const QString &name);
    static QString kindToString(Kind kind);
    static QString kindLabel(Kind kind);

    // True when Omarchy's live colors.toml is present (Linux/Omarchy only).
    static bool omarchyAvailable();

signals:
    void changed();

private:
    void apply();
    void applySystem();
    void applyPalette(const QPalette &palette, const QString &styleSheet, const QString &fingerprint);
    void clearOmarchyWatcher();
    void armOmarchyWatcher();
    void reloadOmarchy();
    QPalette paletteFromColors(const QHash<QString, QString> &colors) const;
    QString styleSheetFromColors(const QHash<QString, QString> &colors) const;
    static QHash<QString, QString> readColorsToml(const QString &path);
    static QHash<QString, QString> minimalColors();
    static QHash<QString, QString> omarchyFallbackColors();
    static QString omarchyCurrentDir();
    static QString contrastForeground(const QColor &background);
    static QString fingerprintOf(const QHash<QString, QString> &colors);

    Kind m_kind = Kind::System;
    Kind m_appliedKind = Kind::System;
    QString m_appliedFingerprint;
    QPalette m_systemPalette;
    QString m_systemStyle;
    QString m_systemStyleName;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
