#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QIcon>
#include <QSettings>

static ThemeManager::Kind loadInitialTheme()
{
    const QSettings settings;
    if (settings.contains(QStringLiteral("theme"))) {
        const auto kind = ThemeManager::kindFromString(settings.value(QStringLiteral("theme")).toString());
        // Stale Omarchy choice on non-Omarchy platforms falls back to System.
        if (kind == ThemeManager::Kind::Omarchy && !ThemeManager::omarchyAvailable())
            return ThemeManager::Kind::System;
        return kind;
    }
    // First run on Omarchy: follow the desktop theme. Elsewhere keep System
    // so Windows/macOS behavior stays unchanged.
    if (ThemeManager::omarchyAvailable())
        return ThemeManager::Kind::Omarchy;
    return ThemeManager::Kind::System;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("light_md"));
    QApplication::setApplicationVersion(QStringLiteral(LIGHT_MD_VERSION));
    QApplication::setOrganizationName(QStringLiteral("light_md"));
    // Matches light_md.desktop so Wayland/Omarchy pick up the launcher icon.
    QApplication::setDesktopFileName(QStringLiteral("light_md"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/light_md.png")));

    ThemeManager themes;
    themes.setKind(loadInitialTheme());

    const QStringList args = QApplication::arguments();
    const QString initialFile = args.size() > 1 ? args.at(1) : QString();
    MainWindow window(&themes, initialFile);
    // Receives QFileOpenEvent, which is sent to the application object (see MainWindow::eventFilter).
    app.installEventFilter(&window);
    window.show();
    return app.exec();
}
