#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QLockFile>
#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QSettings>
#include <QTimer>
#include <functional>
#include <memory>
#include <vector>

class FindBar;
class MarkdownEditor;
class MarkdownHighlighter;
class QAction;
class QActionGroup;
class QHBoxLayout;
class QImage;
class QLabel;
class QMenu;
class QMimeData;
class QNetworkReply;
class QPushButton;
class QScrollBar;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QTextBrowser;
class SpellChecker;
class ThemeManager;

// One open document: its own writing pane (with its undo history), file and recovery copy.
struct DocumentTab
{
    MarkdownEditor *editor = nullptr;
    MarkdownHighlighter *highlighter = nullptr;
    QString filePath;
    // Crash recovery: "<id>.md" / "<id>.json" hold the unsaved text, "<id>.lock" is held while open.
    QString recoveryId;
    std::unique_ptr<QLockFile> recoveryLock;
    // Where the shared preview was when this tab was left, and which pane was leading.
    double previewScroll = 0;
    bool previewLeads = false;
    // Set when the tab was reopened in the background; applied the first time it is shown.
    double pendingEditorScroll = -1;
};

// The light_md window: buttons on top, tabs, writing pane on the left, preview on the right.
class MainWindow : public QMainWindow
{
    Q_DECLARE_TR_FUNCTIONS(MainWindow)
    // tests/tst_session.cpp checks where documents are reopened.
    friend class TestSession;

public:
    explicit MainWindow(ThemeManager *themes, const QString &initialFile = QString());
    ~MainWindow() override;
    // Opens `path` in a tab (switching to it if it is already open).
    bool loadFile(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QAction *addOption(QMenu *menu, const QString &text, const QString &tip, const QKeySequence &keys);
    QPushButton *addBarButton(QHBoxLayout *bar, QAction *action);
    void buildMenus();
    QPushButton *addMenuButton(QHBoxLayout *bar, const QString &text, const QString &tip, QMenu *menu);
    void setShowButtons(bool show);
    void setEditMode(bool editing);
    void changeTextSize(int steps);
    void applyTextSize();
    void updateHighlighter();
    static QString droppedFile(const QMimeData *mime);
    static double scrollRatio(const QScrollBar *bar);
    static void setScrollRatio(QScrollBar *bar, double ratio);
    void syncScroll(QScrollBar *from, QScrollBar *to);
    void syncFromLeader();
    void updatePreview();
    void updateStatus();
    void updateTitle();
    void setFilePath(const QString &path);
    QString lastFolder() const;

    // Formatting buttons: show the writing pane, apply the change (see formatting.h), focus the pane.
    void prefixLines(const std::function<QString(int)> &prefixFor);
    void setHeading(const QString &marks);
    void wrapSelection(const QString &marks);
    void insertLink();
    void insertBlock(const QString &text, const QString &selectWord = QString());
    void insertCodeBlock();

    void insertImage();
    void pasteImage(const QImage &image);

    // Recent documents, where each was left off, and crash recovery (session.cpp).
    static QStringList recentFiles() { return QSettings().value(QStringLiteral("recentFiles")).toStringList(); }
    static void setRecentFiles(const QStringList &files) { QSettings().setValue(QStringLiteral("recentFiles"), files); }
    static QString cleanPath(const QString &path) { return QDir::cleanPath(QFileInfo(path).absoluteFilePath()); }
    void rememberPosition(DocumentTab *tab);
    void restorePosition(const QString &path);
    void rememberOpenTabs();
    static QString recoveryFolder();
    static QString recoveryFile(const QString &id, const QString &extension);
    void startRecovery(DocumentTab *tab);
    bool writeRecovery(DocumentTab *tab);
    void clearRecovery(DocumentTab *tab);
    void offerRecovery();
    static void addRecentFile(const QString &path);
    void rebuildRecentMenu();
    void openRecent(const QString &file);

    // Spell checking (spellcheck.h): the language picked in Options -> Spelling, otherwise the
    // interface language. "off" switches it off.
    static QString spellingLanguage();
    void setSpellingLanguage(const QString &language);
    void addToDictionary(const QString &word);
    void rehighlightAll();

    // Check for updates (updates.cpp).
    static QString platformDownloadSuffix();
    static QString installSteps();
    void checkForUpdates();
    void showUpdateResult();

    // New, open, save, export and print.
    bool askSaveIfModified(DocumentTab *tab);
    void newDocument();
    bool closeTab(int index);
    void openDocument();
    bool writeTo(const QString &path);
    bool saveDocument();
    QString suggestedPath(const QString &extension) const;
    bool saveDocumentAs();
    void exportPdf();
    QString exportPath(const QString &title, const QString &extension, const QString &filter);
    void exportHtml();
    void exportOdt();
    void printDocument();
    void showHelp();

    ThemeManager *m_themes = nullptr;
    QWidget *m_fileBar = nullptr;
    QWidget *m_formatBar = nullptr;
    QHBoxLayout *m_fileBarLayout = nullptr;
    QHBoxLayout *m_formatBarLayout = nullptr;
    QSplitter *m_splitter = nullptr;
    // Tabs (tabs.cpp). m_editor and m_filePath always belong to the current tab.
    DocumentTab *addTab();
    void switchToTab(int index);
    DocumentTab *currentTab() const;
    DocumentTab *tabAt(int index) const;
    int indexOf(const DocumentTab *tab) const;
    int indexOfFile(const QString &path) const;
    void updateTabText(DocumentTab *tab);
    // An untitled tab nobody typed in (the welcome text, or a fresh New): opening a file replaces it.
    bool isSpare(const DocumentTab *tab) const;

    QTabBar *m_tabBar = nullptr;
    QStackedWidget *m_editors = nullptr;
    std::vector<std::unique_ptr<DocumentTab>> m_tabs;
    MarkdownEditor *m_editor = nullptr;
    QTextBrowser *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_showButtonsAction = nullptr;
    QAction *m_syncScrollAction = nullptr;
    QMenu *m_formatMenu = nullptr;
    QMenu *m_recentMenu = nullptr;
    FindBar *m_findBar = nullptr;
    std::unique_ptr<SpellChecker> m_speller;
    QActionGroup *m_spellingGroup = nullptr;
    QNetworkAccessManager m_network;
    QNetworkReply *m_updateReply = nullptr;
    QTimer m_previewTimer;
    QTimer m_statusTimer;
    QTimer m_autosaveTimer;
    QAction *m_undoAction = nullptr;
    QAction *m_redoAction = nullptr;
    QString m_filePath;
    bool m_editMode = true;
    bool m_showButtons = true;
    bool m_syncScroll = true;
    bool m_syncingScroll = false;
    // True when the user last scrolled, clicked or typed in the reading pane; that pane then
    // drives synchronized scrolling. Otherwise the writing pane does.
    bool m_previewLeads = false;
    static constexpr int kMinTextSize = -5;
    static constexpr int kMaxTextSize = 20;
    int m_textSize = 0;
    int m_wheelZoom = 0;
    qreal m_editorBaseSize = 10;
    qreal m_previewBaseSize = 11;
};
