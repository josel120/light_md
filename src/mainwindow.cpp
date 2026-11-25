#include "mainwindow.h"

#include "editor.h"
#include "findbar.h"
#include "formatting.h"
#include "highlighter.h"
#include "theme.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileOpenEvent>
#include <QHBoxLayout>
#include <QImage>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWheelEvent>

MainWindow::MainWindow(ThemeManager *themes, const QString &initialFile)
    : m_themes(themes)
{
    resize(1100, 700);

    auto *central = new QWidget(this);
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(8, 8, 8, 8);

    m_fileBar = new QWidget(central);
    m_fileBarLayout = new QHBoxLayout(m_fileBar);
    m_fileBarLayout->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_fileBar);

    m_formatBar = new QWidget(central);
    m_formatBarLayout = new QHBoxLayout(m_formatBar);
    m_formatBarLayout->setContentsMargins(0, 0, 0, 0);
    root->addWidget(m_formatBar);

    // One tab per open document; hidden while there is only one.
    m_tabBar = new QTabBar(central);
    m_tabBar->setDocumentMode(true);
    m_tabBar->setTabsClosable(true);
    m_tabBar->setMovable(true);
    m_tabBar->setExpanding(false);
    m_tabBar->setAutoHide(true);
    m_tabBar->setElideMode(Qt::ElideMiddle);
    m_tabBar->setFocusPolicy(Qt::NoFocus);
    root->addWidget(m_tabBar);

    m_splitter = new QSplitter(Qt::Horizontal, central);
    // The writing panes of all tabs; only the current one shows.
    m_editors = new QStackedWidget(m_splitter);
    m_preview = new QTextBrowser(m_splitter);
    m_preview->setOpenExternalLinks(true);
    // Base sizes; the user's bigger/smaller text setting is added on top (see applyTextSize).
    m_previewBaseSize = m_preview->font().pointSizeF() + 2;
    m_editorBaseSize = MarkdownEditor().font().pointSizeF() + 1;
    m_preview->viewport()->installEventFilter(this);
    m_splitter->addWidget(m_editors);
    m_splitter->addWidget(m_preview);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({550, 550});
    root->addWidget(m_splitter, 1);
    m_findBar = new FindBar(nullptr, central);
    // Matches are selected in the writing pane, so it has to be visible.
    m_findBar->beforeSearch = [this] {
        if (!m_editMode)
            setEditMode(true);
    };
    // A match moves the writing pane; the preview should follow it there, even if the user
    // last scrolled the preview.
    m_findBar->matchFound = [this] {
        m_previewLeads = false;
        syncFromLeader();
    };
    root->addWidget(m_findBar);

    m_status = new QLabel(central);
    root->addWidget(m_status);

    setCentralWidget(central);
    setAcceptDrops(true);
    buildMenus();

    // Preview updates quickly; word count is cheaper on a slower timer.
    m_previewTimer.setSingleShot(true);
    m_previewTimer.setInterval(120);
    connect(&m_previewTimer, &QTimer::timeout, this, [this] { updatePreview(); });
    m_statusTimer.setSingleShot(true);
    m_statusTimer.setInterval(350);
    connect(&m_statusTimer, &QTimer::timeout, this, [this] { updateStatus(); });
    // A few seconds after a change, keep a recovery copy in case the app or the computer crashes.
    m_autosaveTimer.setSingleShot(true);
    m_autosaveTimer.setInterval(5000);
    connect(&m_autosaveTimer, &QTimer::timeout, this, [this] {
        for (const auto &tab : m_tabs)
            writeRecovery(tab.get());
    });

    // Scrolling either pane moves the other to the same place, proportionally (the writing
    // pane's side is connected for each tab, in addTab).
    QScrollBar *previewBar = m_preview->verticalScrollBar();
    connect(previewBar, &QScrollBar::valueChanged, this, [this, previewBar] {
        if (m_editor)
            syncScroll(previewBar, m_editor->verticalScrollBar());
    });
    // The preview's height changes after a re-render, a resize or a new text size; realign the
    // other pane to whichever one the user is working in.
    connect(previewBar, &QScrollBar::rangeChanged, this, [this] { syncFromLeader(); });

    connect(m_tabBar, &QTabBar::currentChanged, this, [this](int index) { switchToTab(index); });
    connect(m_tabBar, &QTabBar::tabCloseRequested, this, [this](int index) { closeTab(index); });
    connect(m_tabBar, &QTabBar::tabMoved, this, [this](int from, int to) {
        auto tab = std::move(m_tabs[from]);
        m_tabs.erase(m_tabs.begin() + from);
        m_tabs.insert(m_tabs.begin() + to, std::move(tab));
    });

    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);

    // Classic defaults on first run: buttons visible, two panes (edit mode).
    const bool showButtons = settings.value(QStringLiteral("ui/showButtons"), true).toBool();
    const bool editMode = settings.value(QStringLiteral("ui/editMode"), true).toBool();
    m_syncScroll = settings.value(QStringLiteral("ui/syncScroll"), true).toBool();
    m_syncScrollAction->setChecked(m_syncScroll);
    m_textSize = qBound(kMinTextSize, settings.value(QStringLiteral("ui/textSize"), 0).toInt(), kMaxTextSize);
    applyTextSize();

    // Start with the welcome text; the first document opened below takes its place.
    switchToTab(indexOf(addTab()));
    m_editor->setPlainText(QStringLiteral(
        "# Welcome to light_md\n\n"
        "Write on the **left** and see the result on the *right*.\n\n"
        "## Getting started\n\n"
        "- Use the buttons at the top to format your text\n"
        "- Hide the buttons anytime with **Options → Show buttons** (Ctrl+Shift+B)\n"
        "- **Ctrl+E** toggles the writing pane; **Ctrl+R** or **Esc** is Read only\n"
        "- **Ctrl+N** opens a new tab; every action also lives under **Options** with a shortcut\n"));
    m_editor->document()->setModified(false);
    updatePreview();

    // Reopen the tabs that were open last time, then a file given on the command line (or by
    // "Open with"), which becomes the current tab.
    const QString lastFile = settings.value(QStringLiteral("session/lastFile")).toString();
    QStringList openFiles = settings.value(QStringLiteral("session/openFiles")).toStringList();
    if (openFiles.isEmpty() && !lastFile.isEmpty())
        openFiles.append(lastFile);
    for (const QString &file : openFiles) {
        if (QFileInfo::exists(file))
            loadFile(file);
    }
    if (const int last = indexOfFile(lastFile); last >= 0)
        switchToTab(last);
    if (!initialFile.isEmpty())
        loadFile(initialFile);

    // Apply after load so a failed open still gets remembered chrome.
    setShowButtons(showButtons);
    setEditMode(editMode);

    // After the window is on screen, so the question appears in front of it.
    QTimer::singleShot(0, this, [this] { offerRecovery(); });
}

bool MainWindow::loadFile(const QString &path)
{
    if (const int open = indexOfFile(path); open >= 0) {
        switchToTab(open);
        return true;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Could not open"),
                             QStringLiteral("Could not read the file:\n%1").arg(path));
        return false;
    }
    if (!isSpare(currentTab()))
        switchToTab(indexOf(addTab()));
    m_editor->setPlainText(QString::fromUtf8(file.readAll()));
    m_editor->document()->setModified(false);
    clearRecovery(currentTab());
    setFilePath(path);
    addRecentFile(path);
    m_preview->verticalScrollBar()->setValue(0);
    updatePreview();
    restorePosition(path);
    // Keep the user's current Edit/Reader choice (remembered in QSettings).
    return true;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    // Every tab with unsaved changes gets asked, in turn, in front.
    DocumentTab *current = currentTab();
    for (int i = 0; i < int(m_tabs.size()); ++i) {
        if (!m_tabs[i]->editor->document()->isModified())
            continue;
        switchToTab(i);
        if (!askSaveIfModified(m_tabs[i].get())) {
            event->ignore();
            return;
        }
    }
    // Back to the tab the user was in, so that is the one reopened in front next time.
    switchToTab(indexOf(current));
    for (const auto &tab : m_tabs)
        rememberPosition(tab.get());
    rememberOpenTabs();
    QSettings().setValue(QStringLiteral("geometry"), saveGeometry());
    // A normal close (saved, or "Don't save") leaves nothing to recover.
    for (const auto &tab : m_tabs)
        clearRecovery(tab.get());
    event->accept();
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    // Esc first closes the find bar; otherwise it returns to Reader. Dialogs are modal,
    // so they keep their own Esc.
    if (event->key() == Qt::Key_Escape && m_findBar->isVisible()) {
        m_findBar->closeBar();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Escape && m_editMode) {
        setEditMode(false);
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

// Dropping a document on the window opens it instead of pasting its path.
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (droppedFile(event->mimeData()).isEmpty())
        return;
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QString path = droppedFile(event->mimeData());
    if (path.isEmpty())
        return;
    event->acceptProposedAction();
    loadFile(path);
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // On macOS, documents opened from Finder arrive as an event instead of a command-line argument.
    if (event->type() == QEvent::FileOpen) {
        const QString path = static_cast<QFileOpenEvent *>(event)->file();
        if (!path.isEmpty())
            loadFile(path);
        return true;
    }
    // Before the first tab exists, nothing below applies.
    if (!m_editor)
        return QMainWindow::eventFilter(watched, event);
    // Remember which pane the user last scrolled, clicked or typed in. Only that pane drives the
    // other one, so the preview moving by itself (after a refresh or a resize) never drags the editor.
    if (event->type() == QEvent::Wheel || event->type() == QEvent::MouseButtonPress
        || event->type() == QEvent::KeyPress) {
        if (auto *widget = qobject_cast<QWidget *>(watched)) {
            if (widget == m_preview || m_preview->isAncestorOf(widget))
                m_previewLeads = true;
            else if (widget == m_editor || m_editor->isAncestorOf(widget))
                m_previewLeads = false;
        }
    }
    // Ctrl + mouse wheel changes the text size of both panes together (and remembers it),
    // instead of Qt's built-in zoom, which only touches one pane and is forgotten.
    if (event->type() == QEvent::Wheel
        && (watched == m_editor->viewport() || watched == m_preview->viewport())) {
        auto *wheel = static_cast<QWheelEvent *>(event);
        if (wheel->modifiers() & Qt::ControlModifier) {
            // Touchpads send many small steps; add them up to whole wheel notches.
            m_wheelZoom += wheel->angleDelta().y();
            const int steps = m_wheelZoom / 120;
            m_wheelZoom -= steps * 120;
            if (steps != 0)
                changeTextSize(steps);
            return true;
        }
    }
    // A new theme (or the desktop switching light/dark under System) repaints the editor;
    // re-color once the palette has settled.
    if (watched == m_editor && event->type() == QEvent::PaletteChange)
        QTimer::singleShot(0, this, [this] { updateHighlighter(); });
    if (watched == m_editor->viewport()) {
        if (event->type() == QEvent::DragEnter || event->type() == QEvent::DragMove) {
            auto *drag = static_cast<QDragMoveEvent *>(event);
            if (!droppedFile(drag->mimeData()).isEmpty()) {
                drag->acceptProposedAction();
                return true;
            }
        } else if (event->type() == QEvent::Drop) {
            auto *drop = static_cast<QDropEvent *>(event);
            if (!droppedFile(drop->mimeData()).isEmpty()) {
                dropEvent(drop);
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

QAction *MainWindow::addOption(QMenu *menu, const QString &text, const QString &tip, const QKeySequence &keys)
{
    auto *action = menu->addAction(text);
    action->setStatusTip(tip);
    action->setToolTip(tip);
    action->setShortcutContext(Qt::ApplicationShortcut);
    if (!keys.isEmpty())
        action->setShortcut(keys);
    return action;
}

QPushButton *MainWindow::addBarButton(QHBoxLayout *bar, QAction *action)
{
    // One QAction owns the shortcut; the button only triggers it.
    auto *button = new QPushButton(action->text().remove(QLatin1Char('&')), this);
    const auto tipWithShortcut = [action] {
        const QString tip = action->toolTip();
        const QKeySequence keys = action->shortcut();
        if (keys.isEmpty())
            return tip;
        return QStringLiteral("%1 (%2)").arg(tip, keys.toString(QKeySequence::NativeText));
    };
    button->setToolTip(tipWithShortcut());
    button->setStatusTip(action->statusTip());
    button->setFocusPolicy(Qt::NoFocus);
    connect(button, &QPushButton::clicked, action, &QAction::trigger);
    connect(action, &QAction::changed, button, [button, action, tipWithShortcut] {
        button->setEnabled(action->isEnabled());
        button->setVisible(action->isVisible());
        button->setToolTip(tipWithShortcut());
    });
    button->setEnabled(action->isEnabled());
    bar->addWidget(button);
    return button;
}

void MainWindow::buildMenus()
{
    auto *options = menuBar()->addMenu(QStringLiteral("&Options"));

    auto *actNew = addOption(options, QStringLiteral("&New"), QStringLiteral("Start a blank document in a new tab"),
                             QKeySequence::New);
    auto *actOpen = addOption(options, QStringLiteral("&Open..."), QStringLiteral("Open a saved document"),
                              QKeySequence::Open);
    m_recentMenu = options->addMenu(QStringLiteral("Open &recent"));
    m_recentMenu->setToolTipsVisible(true);
    connect(m_recentMenu, &QMenu::aboutToShow, this, [this] { rebuildRecentMenu(); });
    auto *actSave = addOption(options, QStringLiteral("&Save"), QStringLiteral("Save your changes"),
                              QKeySequence::Save);
    auto *actSaveAs = addOption(options, QStringLiteral("Save &as..."),
                                QStringLiteral("Save a copy with a different name"),
                                QKeySequence::SaveAs);
    auto *actCloseTab = addOption(options, QStringLiteral("&Close tab"), QStringLiteral("Close this document"),
                                  QKeySequence(QStringLiteral("Ctrl+W")));
    auto *actNextTab = addOption(options, QStringLiteral("Next ta&b"), QStringLiteral("Go to the next open document"),
                                 QKeySequence(QStringLiteral("Ctrl+Tab")));
    auto *actPreviousTab = addOption(options, QStringLiteral("Previous tab"),
                                     QStringLiteral("Go to the previous open document"),
                                     QKeySequence(QStringLiteral("Ctrl+Shift+Tab")));
    options->addSeparator();
    auto *actPdf = addOption(options, QStringLiteral("Export to &PDF..."),
                             QStringLiteral("Create a nicely formatted PDF to send or share"),
                             QKeySequence(QStringLiteral("Ctrl+Shift+E")));
    auto *actPrint = addOption(options, QStringLiteral("&Print..."), QStringLiteral("Print the document"),
                               QKeySequence::Print);
    options->addSeparator();

    auto *actFind = addOption(options, QStringLiteral("&Find..."),
                              QStringLiteral("Find and replace words in the text"), QKeySequence::Find);
    auto *actReplace = addOption(options, QStringLiteral("Re&place..."),
                                 QStringLiteral("Replace a word with another one"), QKeySequence::Replace);
    auto *actFindNext = addOption(options, QStringLiteral("Find ne&xt"),
                                  QStringLiteral("Go to the next match"), QKeySequence::FindNext);
    auto *actFindPrevious = addOption(options, QStringLiteral("Find pre&vious"),
                                      QStringLiteral("Go to the previous match"), QKeySequence::FindPrevious);
    options->addSeparator();

    m_editAction = addOption(options, QStringLiteral("&Edit document"),
                             QStringLiteral("Show the writing pane"),
                             QKeySequence(QStringLiteral("Ctrl+E")));
    m_editAction->setCheckable(true);

    auto *actRead = addOption(options, QStringLiteral("&Read only"),
                              QStringLiteral("Hide the writing pane and just read"),
                              QKeySequence(QStringLiteral("Ctrl+R")));

    m_showButtonsAction = addOption(options, QStringLiteral("Show &buttons"),
                                    QStringLiteral("Show or hide the file and format button bars"),
                                    QKeySequence(QStringLiteral("Ctrl+Shift+B")));
    m_showButtonsAction->setCheckable(true);
    m_showButtonsAction->setChecked(true);

    m_syncScrollAction = addOption(options, QStringLiteral("S&ync scrolling"),
                                   QStringLiteral("Scroll the writing pane and the preview together"),
                                   QKeySequence());
    m_syncScrollAction->setCheckable(true);
    m_syncScrollAction->setChecked(true);

    auto *textSizeMenu = options->addMenu(QStringLiteral("&Text size"));
    auto *actBigger = addOption(textSizeMenu, QStringLiteral("&Bigger text"),
                                QStringLiteral("Make the text bigger in both panes"), QKeySequence::ZoomIn);
    // Ctrl+= too, so it works without Shift on keyboards where + needs Shift.
    actBigger->setShortcuts({QKeySequence(QKeySequence::ZoomIn), QKeySequence(QStringLiteral("Ctrl+="))});
    auto *actSmaller = addOption(textSizeMenu, QStringLiteral("&Smaller text"),
                                 QStringLiteral("Make the text smaller in both panes"), QKeySequence::ZoomOut);
    auto *actNormalSize = addOption(textSizeMenu, QStringLiteral("&Normal size"),
                                    QStringLiteral("Go back to the normal text size"),
                                    QKeySequence(QStringLiteral("Ctrl+0")));
    connect(actBigger, &QAction::triggered, this, [this] { changeTextSize(1); });
    connect(actSmaller, &QAction::triggered, this, [this] { changeTextSize(-1); });
    connect(actNormalSize, &QAction::triggered, this, [this] { changeTextSize(-m_textSize); });

    options->addSeparator();
    m_formatMenu = options->addMenu(QStringLiteral("&Format"));
    auto *actUndo = addOption(m_formatMenu, QStringLiteral("&Undo"),
                              QStringLiteral("Take back the last change"), QKeySequence::Undo);
    auto *actRedo = addOption(m_formatMenu, QStringLiteral("&Redo"),
                              QStringLiteral("Bring back the change you undid"), QKeySequence::Redo);
    m_formatMenu->addSeparator();
    auto *actTitle = addOption(m_formatMenu, QStringLiteral("&Big title"),
                               QStringLiteral("Turn the line into a big title"),
                               QKeySequence(QStringLiteral("Ctrl+1")));
    auto *actSubtitle = addOption(m_formatMenu, QStringLiteral("&Subtitle"),
                                  QStringLiteral("Turn the line into a subtitle"),
                                  QKeySequence(QStringLiteral("Ctrl+2")));
    auto *actBold = addOption(m_formatMenu, QStringLiteral("B&old"),
                              QStringLiteral("Make the selected text bold"), QKeySequence::Bold);
    auto *actItalic = addOption(m_formatMenu, QStringLiteral("&Italic"),
                                QStringLiteral("Make the selected text italic"), QKeySequence::Italic);
    auto *actBullets = addOption(m_formatMenu, QStringLiteral("Bullet &list"),
                                 QStringLiteral("Put a bullet in front of each selected line"),
                                 QKeySequence(QStringLiteral("Ctrl+Shift+8")));
    auto *actNumbered = addOption(m_formatMenu, QStringLiteral("&Numbered list"),
                                  QStringLiteral("Number the selected lines: 1, 2, 3..."),
                                  QKeySequence(QStringLiteral("Ctrl+Shift+7")));
    auto *actChecklist = addOption(m_formatMenu, QStringLiteral("&Checklist"),
                                   QStringLiteral("Put a box to tick in front of each selected line"),
                                   QKeySequence(QStringLiteral("Ctrl+Shift+9")));
    // Ctrl+Shift+Q often means Quit on Linux desktops; use Period instead.
    auto *actQuote = addOption(m_formatMenu, QStringLiteral("&Quote"),
                               QStringLiteral("Mark the lines as a quote"),
                               QKeySequence(QStringLiteral("Ctrl+Shift+.")));
    auto *actLink = addOption(m_formatMenu, QStringLiteral("Lin&k"),
                              QStringLiteral("Add a link to a web page"),
                              QKeySequence(QStringLiteral("Ctrl+K")));
    auto *insertMenu = m_formatMenu->addMenu(QStringLiteral("I&nsert"));
    auto *actPicture = addOption(insertMenu, QStringLiteral("&Picture..."),
                                 QStringLiteral("Add a picture from your computer"),
                                 QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    auto *actTable = addOption(insertMenu, QStringLiteral("&Table"), QStringLiteral("Add a small table"),
                               QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    auto *actCode = addOption(insertMenu, QStringLiteral("&Code block"),
                              QStringLiteral("Show text exactly as typed, for code or commands"),
                              QKeySequence(QStringLiteral("Ctrl+Shift+C")));
    auto *actSeparator = addOption(insertMenu, QStringLiteral("&Separator line"),
                                   QStringLiteral("Add a horizontal line between sections"),
                                   QKeySequence(QStringLiteral("Ctrl+Shift+L")));

    options->addSeparator();
    auto *themeMenu = options->addMenu(QStringLiteral("&Theme"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);
    struct ThemeShortcut {
        ThemeManager::Kind kind;
        const char *keys;
    };
    const ThemeShortcut themeShortcuts[] = {
        {ThemeManager::Kind::System, "Ctrl+Shift+1"},
        {ThemeManager::Kind::Minimal, "Ctrl+Shift+2"},
        {ThemeManager::Kind::Omarchy, "Ctrl+Shift+3"},
        {ThemeManager::Kind::Dark, "Ctrl+Shift+4"},
    };
    for (const ThemeShortcut &entry : themeShortcuts) {
        auto *action = addOption(themeMenu, ThemeManager::kindLabel(entry.kind),
                                 QStringLiteral("Use the %1 theme").arg(ThemeManager::kindLabel(entry.kind)),
                                 QKeySequence(QString::fromLatin1(entry.keys)));
        action->setCheckable(true);
        action->setData(ThemeManager::kindToString(entry.kind));
        themeGroup->addAction(action);
        if (entry.kind == ThemeManager::Kind::Omarchy && !ThemeManager::omarchyAvailable()) {
            action->setEnabled(false);
            action->setToolTip(QStringLiteral("Omarchy theme is only available on Omarchy Linux"));
            action->setShortcut(QKeySequence());
        }
        if (entry.kind == m_themes->kind())
            action->setChecked(true);
        connect(action, &QAction::triggered, this, [this, action] {
            const auto kind = ThemeManager::kindFromString(action->data().toString());
            m_themes->setKind(kind);
            QSettings().setValue(QStringLiteral("theme"), ThemeManager::kindToString(kind));
        });
    }

    auto *actUpdates = addOption(options, QStringLiteral("Check for &updates..."),
                                 QStringLiteral("See if a newer version of light_md is available"),
                                 QKeySequence());
    auto *actHelp = addOption(options, QStringLiteral("&Help"), QStringLiteral("How to use light_md"),
                              QKeySequence::HelpContents);

    // Classic file bar (same order as origin/main).
    addBarButton(m_fileBarLayout, actNew);
    addBarButton(m_fileBarLayout, actOpen);
    addMenuButton(m_fileBarLayout, QStringLiteral("Recent"), QStringLiteral("Open a document you used recently"),
                  m_recentMenu);
    addBarButton(m_fileBarLayout, actSave);
    addBarButton(m_fileBarLayout, actSaveAs);
    m_fileBarLayout->addSpacing(12);
    addBarButton(m_fileBarLayout, actPdf);
    addBarButton(m_fileBarLayout, actPrint);
    m_fileBarLayout->addStretch();
    addBarButton(m_fileBarLayout, actFind);
    addBarButton(m_fileBarLayout, actUpdates);
    addBarButton(m_fileBarLayout, actHelp);

    // Classic format bar.
    addBarButton(m_formatBarLayout, actUndo);
    addBarButton(m_formatBarLayout, actRedo);
    m_formatBarLayout->addSpacing(12);
    addBarButton(m_formatBarLayout, actTitle);
    addBarButton(m_formatBarLayout, actSubtitle);
    addBarButton(m_formatBarLayout, actBold);
    addBarButton(m_formatBarLayout, actItalic);
    addBarButton(m_formatBarLayout, actBullets);
    addBarButton(m_formatBarLayout, actNumbered);
    addBarButton(m_formatBarLayout, actChecklist);
    addBarButton(m_formatBarLayout, actQuote);
    addBarButton(m_formatBarLayout, actLink);
    addMenuButton(m_formatBarLayout, QStringLiteral("Insert"),
                  QStringLiteral("Add a picture, a table, a code block or a separator line"), insertMenu);
    m_formatBarLayout->addStretch();

    connect(actNew, &QAction::triggered, this, [this] { newDocument(); });
    connect(actCloseTab, &QAction::triggered, this, [this] { closeTab(indexOf(currentTab())); });
    connect(actNextTab, &QAction::triggered, this, [this] {
        m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + 1) % m_tabBar->count());
    });
    connect(actPreviousTab, &QAction::triggered, this, [this] {
        m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + m_tabBar->count() - 1) % m_tabBar->count());
    });
    connect(actOpen, &QAction::triggered, this, [this] { openDocument(); });
    connect(actSave, &QAction::triggered, this, [this] { saveDocument(); });
    connect(actSaveAs, &QAction::triggered, this, [this] { saveDocumentAs(); });
    connect(actPdf, &QAction::triggered, this, [this] { exportPdf(); });
    connect(actPrint, &QAction::triggered, this, [this] { printDocument(); });
    connect(actHelp, &QAction::triggered, this, [this] { showHelp(); });
    connect(actUpdates, &QAction::triggered, this, [this] { checkForUpdates(); });
    connect(m_editAction, &QAction::toggled, this, [this](bool on) { setEditMode(on); });
    connect(actRead, &QAction::triggered, this, [this] { setEditMode(false); });
    connect(m_showButtonsAction, &QAction::toggled, this, [this](bool on) { setShowButtons(on); });
    connect(m_syncScrollAction, &QAction::toggled, this, [this](bool on) {
        m_syncScroll = on;
        QSettings().setValue(QStringLiteral("ui/syncScroll"), on);
        syncFromLeader();
    });

    // Each tab has its own history; addTab and switchToTab keep these two up to date.
    m_undoAction = actUndo;
    m_redoAction = actRedo;
    connect(actUndo, &QAction::triggered, this, [this] { m_editor->undo(); });
    connect(actRedo, &QAction::triggered, this, [this] { m_editor->redo(); });
    actUndo->setEnabled(false);
    actRedo->setEnabled(false);

    connect(actTitle, &QAction::triggered, this, [this] { setHeading(QStringLiteral("# ")); });
    connect(actSubtitle, &QAction::triggered, this, [this] { setHeading(QStringLiteral("## ")); });
    connect(actQuote, &QAction::triggered, this, [this] { prefixLines([](int) { return QStringLiteral("> "); }); });
    connect(actBullets, &QAction::triggered, this, [this] { prefixLines([](int) { return QStringLiteral("- "); }); });
    connect(actNumbered, &QAction::triggered, this,
            [this] { prefixLines([](int n) { return QStringLiteral("%1. ").arg(n); }); });
    connect(actBold, &QAction::triggered, this, [this] { wrapSelection(QStringLiteral("**")); });
    connect(actItalic, &QAction::triggered, this, [this] { wrapSelection(QStringLiteral("*")); });
    connect(actLink, &QAction::triggered, this, [this] { insertLink(); });
    connect(actChecklist, &QAction::triggered, this,
            [this] { prefixLines([](int) { return QStringLiteral("- [ ] "); }); });
    connect(actPicture, &QAction::triggered, this, [this] { insertImage(); });
    connect(actTable, &QAction::triggered, this, [this] {
        insertBlock(QStringLiteral("| Column 1 | Column 2 |\n"
                                   "| --- | --- |\n"
                                   "| Text | Text |"),
                    QStringLiteral("Column 1"));
    });
    connect(actCode, &QAction::triggered, this, [this] { insertCodeBlock(); });
    connect(actSeparator, &QAction::triggered, this, [this] { insertBlock(QStringLiteral("---")); });

    connect(actFind, &QAction::triggered, this, [this] { m_findBar->openFind(); });
    connect(actReplace, &QAction::triggered, this, [this] { m_findBar->openReplace(); });
    connect(actFindNext, &QAction::triggered, this, [this] { m_findBar->findNext(false); });
    connect(actFindPrevious, &QAction::triggered, this, [this] { m_findBar->findNext(true); });
}

QPushButton *MainWindow::addMenuButton(QHBoxLayout *bar, const QString &text, const QString &tip, QMenu *menu)
{
    auto *button = new QPushButton(text, this);
    button->setToolTip(tip);
    button->setFocusPolicy(Qt::NoFocus);
    button->setMenu(menu);
    bar->addWidget(button);
    return button;
}

void MainWindow::setShowButtons(bool show)
{
    m_showButtons = show;
    if (m_fileBar)
        m_fileBar->setVisible(show);
    if (m_formatBar)
        m_formatBar->setVisible(show && m_editMode);
    if (m_showButtonsAction && m_showButtonsAction->isChecked() != show) {
        QSignalBlocker blocker(m_showButtonsAction);
        m_showButtonsAction->setChecked(show);
    }
    QSettings().setValue(QStringLiteral("ui/showButtons"), show);
    updateStatus();
}

void MainWindow::setEditMode(bool editing)
{
    if (m_editMode == editing && m_editor->isVisible() == editing) {
        if (m_editAction && m_editAction->isChecked() != editing) {
            QSignalBlocker blocker(m_editAction);
            m_editAction->setChecked(editing);
        }
        if (m_formatBar)
            m_formatBar->setVisible(m_showButtons && editing);
        return;
    }
    // Coming back from Reader, keep the part the user was reading: the preview leads.
    // (Not at startup, before the window shows, where restorePosition decides.)
    const bool fromReader = editing && isVisible() && !m_editor->isVisible();
    const double readingRatio = scrollRatio(m_preview->verticalScrollBar());
    if (fromReader)
        m_previewLeads = true;
    m_editMode = editing;
    m_editors->setVisible(editing);
    if (m_formatMenu)
        m_formatMenu->setEnabled(editing);
    if (m_formatBar)
        m_formatBar->setVisible(m_showButtons && editing);
    if (m_editAction && m_editAction->isChecked() != editing) {
        QSignalBlocker blocker(m_editAction);
        m_editAction->setChecked(editing);
    }
    if (editing) {
        if (m_splitter->sizes().value(0, 0) < 80)
            m_splitter->setSizes({550, 550});
        m_editor->setFocus(Qt::ShortcutFocusReason);
        if (fromReader) {
            // Showing the editor narrows the preview and re-wraps it; realign once the layout settles.
            QTimer::singleShot(0, this, [this, readingRatio] {
                if (!m_editMode)
                    return;
                setScrollRatio(m_preview->verticalScrollBar(), readingRatio);
                syncFromLeader();
            });
        } else {
            syncFromLeader();
        }
    } else {
        m_preview->setFocus(Qt::OtherFocusReason);
    }
    QSettings().setValue(QStringLiteral("ui/editMode"), editing);
    updateStatus();
}

void MainWindow::changeTextSize(int steps)
{
    const int size = qBound(kMinTextSize, m_textSize + steps, kMaxTextSize);
    if (size == m_textSize)
        return;
    m_textSize = size;
    QSettings().setValue(QStringLiteral("ui/textSize"), m_textSize);
    applyTextSize();
}

// Each step is one point on top of the base size; titles in the preview grow with it.
void MainWindow::applyTextSize()
{
    for (const auto &tab : m_tabs) {
        QFont writeFont = tab->editor->font();
        writeFont.setPointSizeF(m_editorBaseSize + m_textSize);
        tab->editor->setFont(writeFont);
    }
    QFont readFont = m_preview->font();
    readFont.setPointSizeF(m_previewBaseSize + m_textSize);
    m_preview->setFont(readFont);
    updateHighlighter();
    updateStatus();
}

void MainWindow::updateHighlighter()
{
    for (const auto &tab : m_tabs)
        tab->highlighter->setStyle(tab->editor->palette(), m_editorBaseSize + m_textSize);
}

QString MainWindow::droppedFile(const QMimeData *mime)
{
    if (!mime->hasUrls())
        return QString();
    for (const QUrl &url : mime->urls()) {
        if (url.isLocalFile())
            return url.toLocalFile();
    }
    return QString();
}

double MainWindow::scrollRatio(const QScrollBar *bar)
{
    return bar->maximum() > bar->minimum()
               ? double(bar->value() - bar->minimum()) / (bar->maximum() - bar->minimum())
               : 0.0;
}

void MainWindow::setScrollRatio(QScrollBar *bar, double ratio)
{
    bar->setValue(bar->minimum() + qRound(ratio * (bar->maximum() - bar->minimum())));
}

// Moves `to` to the same relative position as `from`. Only while both panes are showing, and
// only from the pane the user is working in (see m_previewLeads).
void MainWindow::syncScroll(QScrollBar *from, QScrollBar *to)
{
    if (!m_syncScroll || m_syncingScroll || !m_editor || !m_editor->isVisible())
        return;
    if ((from == m_preview->verticalScrollBar()) != m_previewLeads)
        return;
    // The guard stops `to` from scrolling `from` back and making the panes jitter.
    m_syncingScroll = true;
    setScrollRatio(to, scrollRatio(from));
    m_syncingScroll = false;
}

void MainWindow::syncFromLeader()
{
    if (!m_editor)
        return;
    QScrollBar *editorBar = m_editor->verticalScrollBar();
    QScrollBar *previewBar = m_preview->verticalScrollBar();
    if (m_previewLeads)
        syncScroll(previewBar, editorBar);
    else
        syncScroll(editorBar, previewBar);
}

void MainWindow::updatePreview()
{
    if (!m_editor)
        return;
    // Keep the reading position; otherwise the pane jumps to the top on every keystroke.
    QScrollBar *bar = m_preview->verticalScrollBar();
    const int scroll = bar->value();
    m_preview->setMarkdown(m_editor->toPlainText());
    bar->setValue(scroll);
    syncFromLeader();
}

void MainWindow::updateStatus()
{
    if (!m_editor)
        return;
    static const QRegularExpression spaces(QStringLiteral("\\s+"));
    const int words = m_editor->toPlainText().split(spaces, Qt::SkipEmptyParts).size();
    const bool modified = m_editor->document()->isModified();
    const QString mode = m_editMode ? QStringLiteral("Editing") : QStringLiteral("Reading");
    const QString chrome = m_showButtons ? QStringLiteral("Buttons") : QStringLiteral("Minimal");
    // Only mentioned when it differs from normal, so the status line stays short.
    const QString textSize =
        m_textSize == 0 ? QString()
                        : QStringLiteral(" · Text %1%").arg(qRound(100.0 * (m_editorBaseSize + m_textSize)
                                                                   / m_editorBaseSize));
    m_status->setText(QStringLiteral("%1 · %2%3    Words: %4    File: %5%6")
                          .arg(mode, chrome, textSize)
                          .arg(words)
                          .arg(m_filePath.isEmpty() ? QStringLiteral("(not saved)")
                                                    : QDir::toNativeSeparators(m_filePath))
                          .arg(modified ? QStringLiteral("    (unsaved changes)") : QString()));
}

void MainWindow::updateTitle()
{
    const QString name = m_filePath.isEmpty() ? QStringLiteral("New document")
                                              : QFileInfo(m_filePath).fileName();
    setWindowTitle(QStringLiteral("%1[*] - light_md").arg(name));
}

void MainWindow::setFilePath(const QString &path)
{
    // Always absolute: a file given as "notes.md" on the command line must still mean the same
    // file later, whatever folder light_md is started from (Save, Recent, crash recovery).
    m_filePath = path.isEmpty() ? QString() : cleanPath(path);
    if (DocumentTab *tab = currentTab()) {
        tab->filePath = m_filePath;
        updateTabText(tab);
    }
    // Pictures written as "photo.png" are looked up next to the document.
    m_preview->setSearchPaths(path.isEmpty() ? QStringList() : QStringList{QFileInfo(path).absolutePath()});
    if (!path.isEmpty())
        QSettings().setValue(QStringLiteral("lastFolder"), QFileInfo(path).absolutePath());
    updateTitle();
    updateStatus();
}

QString MainWindow::lastFolder() const
{
    return QSettings().value(QStringLiteral("lastFolder")).toString();
}

void MainWindow::prefixLines(const std::function<QString(int)> &prefixFor)
{
    if (!m_editMode)
        setEditMode(true);
    Formatting::prefixLines(m_editor, prefixFor);
    m_editor->setFocus();
}

void MainWindow::setHeading(const QString &marks)
{
    if (!m_editMode)
        setEditMode(true);
    Formatting::setHeading(m_editor, marks);
    m_editor->setFocus();
}

void MainWindow::wrapSelection(const QString &marks)
{
    if (!m_editMode)
        setEditMode(true);
    Formatting::wrapSelection(m_editor, marks);
    m_editor->setFocus();
}

void MainWindow::insertLink()
{
    if (!m_editMode)
        setEditMode(true);
    bool ok = false;
    const QString typed = QInputDialog::getText(this, QStringLiteral("Add link"), QStringLiteral("Web page address:"),
                                                QLineEdit::Normal, QStringLiteral("https://"), &ok);
    const QString address = Formatting::linkAddress(typed);
    if (ok && !address.isEmpty())
        Formatting::insertLink(m_editor, address);
    m_editor->setFocus();
}

void MainWindow::insertBlock(const QString &text, const QString &selectWord)
{
    if (!m_editMode)
        setEditMode(true);
    Formatting::insertBlock(m_editor, text, selectWord);
    m_editor->setFocus();
}

void MainWindow::insertCodeBlock()
{
    if (!m_editMode)
        setEditMode(true);
    Formatting::insertCodeBlock(m_editor);
    m_editor->setFocus();
}

void MainWindow::insertImage()
{
    const QString folder = m_filePath.isEmpty() ? lastFolder() : QFileInfo(m_filePath).absolutePath();
    const QString image = QFileDialog::getOpenFileName(
        this, QStringLiteral("Choose a picture"), folder,
        QStringLiteral("Pictures (*.png *.jpg *.jpeg *.gif *.bmp *.svg *.webp);;All files (*)"));
    if (image.isEmpty()) {
        m_editor->setFocus();
        return;
    }
    // Next to a saved document the path is relative, so the folder can be moved or shared as a whole.
    QString target;
    if (!m_filePath.isEmpty()) {
        const QString relative = QFileInfo(m_filePath).absoluteDir().relativeFilePath(image);
        if (!relative.startsWith(QStringLiteral("../")) && !QDir::isAbsolutePath(relative))
            target = relative;
    }
    if (target.isEmpty())
        target = QUrl::fromLocalFile(image).toString();
    if (target.contains(QLatin1Char(' ')))
        target = QStringLiteral("<%1>").arg(target);
    const QString description = QFileInfo(image).completeBaseName();
    insertBlock(QStringLiteral("![%1](%2)").arg(description, target), description);
}

// A picture pasted from the clipboard (a screenshot, say) is saved as a PNG in an "images"
// folder next to the document, and a link to it goes where the cursor is.
void MainWindow::pasteImage(const QImage &image)
{
    if (m_filePath.isEmpty()) {
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Save the document first"),
            QStringLiteral("Pasted pictures are kept in a folder next to the document, "
                           "so the document needs to be saved first.\n\nSave it now?"),
            QMessageBox::Save | QMessageBox::Cancel, QMessageBox::Save);
        if (answer != QMessageBox::Save || !saveDocumentAs()) {
            m_editor->setFocus();
            return;
        }
    }
    const QDir folder = QFileInfo(m_filePath).absoluteDir();
    const QString subfolder = QStringLiteral("images");
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString name = QStringLiteral("picture-%1.png").arg(stamp);
    for (int n = 2; QFileInfo::exists(folder.filePath(subfolder + QLatin1Char('/') + name)); ++n)
        name = QStringLiteral("picture-%1-%2.png").arg(stamp).arg(n);
    const QString relative = subfolder + QLatin1Char('/') + name;
    if (!folder.mkpath(subfolder) || !image.save(folder.filePath(relative), "PNG")) {
        QMessageBox::warning(this, QStringLiteral("Could not paste the picture"),
                             QStringLiteral("Could not save the picture in:\n%1")
                                 .arg(QDir::toNativeSeparators(folder.filePath(subfolder))));
        return;
    }
    if (!m_editMode)
        setEditMode(true);
    m_editor->textCursor().insertText(QStringLiteral("![Pasted picture](%1)").arg(relative));
    m_editor->setFocus();
    m_status->setText(QStringLiteral("Picture saved as %1").arg(QDir::toNativeSeparators(relative)));
}

// Asks about `tab`'s unsaved changes and saves it if the user says so. A document opened while
// the question is up (from Finder, say) switches tabs, so this goes back to `tab` before saving.
bool MainWindow::askSaveIfModified(DocumentTab *tab)
{
    if (!tab->editor->document()->isModified())
        return true;
    QMessageBox box(QMessageBox::Question, QStringLiteral("Save changes"),
                    QStringLiteral("You have unsaved changes. Do you want to save them?"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.button(QMessageBox::Save)->setText(QStringLiteral("Save"));
    box.button(QMessageBox::Discard)->setText(QStringLiteral("Don't save"));
    box.button(QMessageBox::Cancel)->setText(QStringLiteral("Cancel"));
    box.setDefaultButton(QMessageBox::Save);
    const int answer = box.exec();
    if (answer == QMessageBox::Cancel)
        return false;
    if (answer == QMessageBox::Save) {
        const int index = indexOf(tab);
        if (index < 0)
            return false;
        switchToTab(index);
        return saveDocument();
    }
    return true;
}

void MainWindow::newDocument()
{
    switchToTab(indexOf(addTab()));
    setEditMode(true);
}

// Closes a tab, asking about its unsaved changes first. The last tab is never left empty-handed:
// it becomes a blank document. Returns false when the user cancels.
bool MainWindow::closeTab(int index)
{
    DocumentTab *tab = tabAt(index);
    if (!tab)
        return false;
    if (tab->editor->document()->isModified()) {
        switchToTab(index);
        if (!askSaveIfModified(tab))
            return false;
        // Tabs opened meanwhile go at the end, but look it up again rather than trust that.
        index = indexOf(tab);
        if (index < 0)
            return false;
    }
    rememberPosition(tab);
    clearRecovery(tab);
    if (m_tabs.size() == 1)
        addTab();
    if (tab->editor == m_editor)
        m_editor = nullptr;
    MarkdownEditor *editor = tab->editor;
    // Out of the list before the tab bar picks the next current tab (see switchToTab).
    m_tabs.erase(m_tabs.begin() + index);
    m_tabBar->removeTab(index);
    m_editors->removeWidget(editor);
    editor->deleteLater();
    if (!m_editor)
        switchToTab(m_tabBar->currentIndex());
    return true;
}

void MainWindow::openDocument()
{
    const QStringList paths = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Open document"), lastFolder(),
        QStringLiteral("Markdown documents (*.md *.markdown *.txt);;All files (*)"));
    for (const QString &path : paths)
        loadFile(path);
}

bool MainWindow::writeTo(const QString &path)
{
    // QSaveFile uses a unique temp name and replaces the target only on
    // commit(), so a sibling like doc.md.tmp is never clobbered and a
    // failed rename cannot delete both the old and new document.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("Could not save"),
                             QStringLiteral("Could not write to the file:\n%1").arg(path));
        return false;
    }
    const QByteArray bytes = m_editor->toPlainText().toUtf8();
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        QMessageBox::warning(this, QStringLiteral("Could not save"),
                             QStringLiteral("Could not write to the file:\n%1").arg(path));
        return false;
    }
    if (!file.commit()) {
        QMessageBox::warning(this, QStringLiteral("Could not save"),
                             QStringLiteral("Could not finish saving:\n%1").arg(path));
        return false;
    }
    m_editor->document()->setModified(false);
    clearRecovery(currentTab());
    setFilePath(path);
    addRecentFile(path);
    return true;
}

bool MainWindow::saveDocument()
{
    if (m_filePath.isEmpty())
        return saveDocumentAs();
    return writeTo(m_filePath);
}

QString MainWindow::suggestedPath(const QString &extension) const
{
    const QString base = m_filePath.isEmpty() ? QStringLiteral("document")
                                              : QFileInfo(m_filePath).completeBaseName();
    const QString folder = m_filePath.isEmpty() ? lastFolder() : QFileInfo(m_filePath).absolutePath();
    return folder.isEmpty() ? base + extension : QDir(folder).filePath(base + extension);
}

bool MainWindow::saveDocumentAs()
{
    DocumentTab *tab = currentTab();
    QString start = suggestedPath(QStringLiteral(".md"));
    for (;;) {
        QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save as"), start,
            QStringLiteral("Markdown documents (*.md);;All files (*)"));
        if (path.isEmpty())
            return false;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".md");
        // The dialog lets documents open meanwhile (from Finder, say); save the tab that asked.
        const int index = indexOf(tab);
        if (index < 0)
            return false;
        switchToTab(index);
        // Two tabs on one file would overwrite each other's work on every save.
        if (const int owner = indexOfFile(path); owner >= 0 && tabAt(owner) != tab) {
            QMessageBox::warning(this, QStringLiteral("Document already open"),
                                 QStringLiteral("This document is open in another tab:\n%1\n\n"
                                                "Choose a different name, or close that tab first.")
                                     .arg(QDir::toNativeSeparators(path)));
            start = path;
            continue;
        }
        return writeTo(path);
    }
}

void MainWindow::exportPdf()
{
    QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Export to PDF"),
                                                suggestedPath(QStringLiteral(".pdf")),
                                                QStringLiteral("PDF document (*.pdf)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().compare(QStringLiteral("pdf"), Qt::CaseInsensitive) != 0)
        path += QStringLiteral(".pdf");
    updatePreview();
    QPrinter printer(QPrinter::HighResolution);
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(path);
    printer.setPageMargins(QMarginsF(20, 20, 20, 20), QPageLayout::Millimeter);
    m_preview->document()->print(&printer);
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Could not export"),
                             QStringLiteral("Could not create the PDF:\n%1").arg(path));
        return;
    }
    m_status->setText(QStringLiteral("PDF created: %1").arg(QDir::toNativeSeparators(path)));
}

void MainWindow::printDocument()
{
    updatePreview();
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    dialog.setWindowTitle(QStringLiteral("Print"));
    if (dialog.exec() == QDialog::Accepted)
        m_preview->document()->print(&printer);
}

void MainWindow::showHelp()
{
    QMessageBox::information(
        this, QStringLiteral("How to use light_md"),
        QStringLiteral(
            "<p>By default you get the <b>classic</b> layout: file and format buttons "
            "plus write + preview panes. Hide the buttons anytime for a minimal look. "
            "Your last button and Edit/Reader choices are remembered the next time you open "
            "the app or another file.</p>"
            "<p><b>Layout</b><br>"
            "Ctrl+Shift+B — Show / hide buttons<br>"
            "Ctrl+E — Edit document (show writing pane)<br>"
            "Ctrl+R or Esc — Read only<br>"
            "<i>Sync scrolling</i> (Options) — the preview follows the writing pane<br>"
            "Ctrl++ / Ctrl+- / Ctrl+0 — Bigger / smaller / normal text (or Ctrl + mouse wheel)</p>"
            "<p><b>File</b><br>"
            "Ctrl+N New · Ctrl+O Open · Ctrl+S Save · Ctrl+Shift+S Save as<br>"
            "Ctrl+Shift+E Export PDF · Ctrl+P Print</p>"
            "<p><b>Format</b> (while editing)<br>"
            "Ctrl+Z Undo · Ctrl+Shift+Z / Ctrl+Y Redo<br>"
            "Ctrl+B Bold · Ctrl+I Italic · Ctrl+K Link<br>"
            "Ctrl+1 Big title · Ctrl+2 Subtitle<br>"
            "Ctrl+Shift+8 Bullet list · Ctrl+Shift+7 Numbered list · Ctrl+Shift+9 Checklist · "
            "Ctrl+Shift+. Quote</p>"
            "<p><b>Insert</b> (button or Options → Format → Insert)<br>"
            "Ctrl+Shift+P Picture · Ctrl+Shift+T Table · Ctrl+Shift+C Code block · "
            "Ctrl+Shift+L Separator line<br>"
            "Ctrl+V with a picture on the clipboard (a screenshot, say) saves it in an <i>images</i> folder "
            "next to the document and adds it</p>"
            "<p><b>Tabs</b><br>"
            "Ctrl+N New tab · Ctrl+W Close tab · Ctrl+Tab / Ctrl+Shift+Tab Next / previous tab. "
            "Opening a document puts it in its own tab, and your tabs come back the next time.</p>"
            "<p><b>Find</b><br>"
            "Ctrl+F Find · Ctrl+H Replace · F3 / Shift+F3 Next / previous match · Esc closes the find bar</p>"
            "<p><b>Theme</b><br>"
            "Ctrl+Shift+1 System · Ctrl+Shift+2 Minimal · Ctrl+Shift+3 Omarchy "
            "(Omarchy only where available) · Ctrl+Shift+4 Dark</p>"
            "<p><b>Easy to read while you write</b><br>"
            "Titles show bigger, bold and italic look the part, links and code stand out and quotes are "
            "dimmed. The Markdown symbols stay, in a quieter color.</p>"
            "<p><b>Never lose your work</b><br>"
            "A few seconds after each change, light_md keeps a recovery copy. If it closes unexpectedly, "
            "it offers to restore your unsaved changes the next time you open it.</p>"
            "<p><b>Updates</b><br>"
            "<i>Check for updates</i> tells you when a new version is ready and downloads it.</p>"
            "<p>F1 — Help. You can also drag a document onto the window to open it, "
            "or reopen one from <i>Recent</i>.</p>"));
}
