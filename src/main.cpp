#include "mainwindow.h"
#include "theme.h"

#include <QApplication>
#include <QIcon>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

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

// The interface language: the one picked in Options → Language, otherwise the system's first choice.
static void installTranslations(QApplication &app)
{
    const QString chosen = QSettings().value(QStringLiteral("language")).toString();
    const QLocale locale(chosen.isEmpty() ? QLocale::system().uiLanguages().value(0) : chosen);
    auto *own = new QTranslator(&app);
    if (own->load(locale, QStringLiteral("light_md"), QStringLiteral("_"), QStringLiteral(":/i18n")))
        QApplication::installTranslator(own);
    // Qt's own texts, such as the buttons of the file dialogs.
    auto *qt = new QTranslator(&app);
    if (qt->load(locale, QStringLiteral("qt"), QStringLiteral("_"), QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
        QApplication::installTranslator(qt);
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

    installTranslations(app);

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
