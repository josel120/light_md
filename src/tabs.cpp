// Tabs: several documents open at once, each with its own writing pane, undo history, unsaved
// marker and recovery copy. The preview is shared and shows the current tab.
#include "mainwindow.h"

#include "editor.h"
#include "findbar.h"
#include "highlighter.h"

#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/stat.h>
#endif

namespace {

// Which file on disk `path` is (its volume and file number), so two spellings of one file
// (Notes.md and notes.md on Windows, a link) compare equal. Empty when it can't be told.
// Asked of the system directly rather than through std::filesystem, which needs a newer C++
// runtime than some Windows machines (and CI) have first on their PATH.
QByteArray fileIdentity(const QString &path)
{
#ifdef Q_OS_WIN
    const HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(QDir::toNativeSeparators(path).utf16()), 0,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return QByteArray();
    BY_HANDLE_FILE_INFORMATION info;
    const bool known = GetFileInformationByHandle(file, &info);
    CloseHandle(file);
    if (!known)
        return QByteArray();
    return QByteArray::number(qulonglong(info.dwVolumeSerialNumber)) + ':'
        + QByteArray::number((qulonglong(info.nFileIndexHigh) << 32) | info.nFileIndexLow);
#else
    struct stat info;
    if (::stat(QFile::encodeName(path).constData(), &info) != 0)
        return QByteArray();
    return QByteArray::number(qulonglong(info.st_dev)) + ':' + QByteArray::number(qulonglong(info.st_ino));
#endif
}

} // namespace

DocumentTab *MainWindow::addTab()
{
    auto owned = std::make_unique<DocumentTab>();
    DocumentTab *tab = owned.get();
    auto *editor = new MarkdownEditor(m_editors);
    tab->editor = editor;
    editor->pasteImage = [this](const QImage &image) { pasteImage(image); };
    editor->setPlaceholderText(tr("Type your text here..."));
    editor->setUndoRedoEnabled(true);
    editor->viewport()->installEventFilter(this);
    // Watched for palette changes, so the highlighting follows every theme switch.
    editor->installEventFilter(this);
    QFont font = editor->font();
    font.setPointSizeF(m_editorBaseSize + m_textSize);
    editor->setFont(font);
    tab->highlighter = new MarkdownHighlighter(editor->document());
    tab->highlighter->setStyle(editor->palette(), m_editorBaseSize + m_textSize);

    connect(editor, &QPlainTextEdit::textChanged, this, [this, editor] {
        // Not restarted on every key, so a long typing session still gets a recovery copy.
        if (!m_autosaveTimer.isActive())
            m_autosaveTimer.start();
        if (editor != m_editor)
            return;
        // Any edit (typed, pasted or from a button) puts the writing pane in charge of scrolling.
        m_previewLeads = false;
        m_previewTimer.start();
        m_statusTimer.start();
    });
    connect(editor->document(), &QTextDocument::modificationChanged, this, [this, tab](bool changed) {
        updateTabText(tab);
        // Back to the saved text (for example with Undo): a crash now must not offer undone edits.
        if (!changed)
            clearRecovery(tab);
        if (tab->editor != m_editor)
            return;
        setWindowModified(changed);
        updateStatus();
    });
    connect(editor, &QPlainTextEdit::undoAvailable, this, [this, editor](bool available) {
        if (editor == m_editor)
            m_undoAction->setEnabled(available);
    });
    connect(editor, &QPlainTextEdit::redoAvailable, this, [this, editor](bool available) {
        if (editor == m_editor)
            m_redoAction->setEnabled(available);
    });
    QScrollBar *editorBar = editor->verticalScrollBar();
    connect(editorBar, &QScrollBar::valueChanged, this, [this, editor, editorBar] {
        if (editor == m_editor)
            syncScroll(editorBar, m_preview->verticalScrollBar());
    });
    connect(editorBar, &QScrollBar::rangeChanged, this, [this, editor] {
        if (editor == m_editor)
            syncFromLeader();
    });

    startRecovery(tab);
    m_editors->addWidget(editor);
    // In the list before the tab bar hears of it: adding the first tab makes it current.
    m_tabs.push_back(std::move(owned));
    m_tabBar->addTab(QString());
    updateTabText(tab);
    return tab;
}

void MainWindow::switchToTab(int index)
{
    DocumentTab *tab = tabAt(index);
    if (!tab || tab->editor == m_editor)
        return;
    // Remember where the preview was for the tab being left.
    if (DocumentTab *previous = currentTab()) {
        previous->previewScroll = scrollRatio(m_preview->verticalScrollBar());
        previous->previewLeads = m_previewLeads;
    }
    m_editor = tab->editor;
    m_editors->setCurrentWidget(m_editor);
    if (m_tabBar->currentIndex() != index) {
        const QSignalBlocker blocker(m_tabBar);
        m_tabBar->setCurrentIndex(index);
    }
    m_findBar->setEditor(m_editor);
    m_undoAction->setEnabled(m_editor->document()->isUndoAvailable());
    m_redoAction->setEnabled(m_editor->document()->isRedoAvailable());
    setWindowModified(m_editor->document()->isModified());
    setFilePath(tab->filePath);

    m_previewLeads = tab->previewLeads;
    updatePreview();
    // The preview goes back to where this tab left it, unless the writing pane leads and the
    // panes scroll together (then the preview follows the writing pane anyway).
    if (!m_syncScroll || m_previewLeads)
        setScrollRatio(m_preview->verticalScrollBar(), tab->previewScroll);
    // Opened in the background at startup: scroll its writing pane once it can be laid out.
    if (tab->pendingEditorScroll >= 0) {
        const double ratio = tab->pendingEditorScroll;
        tab->pendingEditorScroll = -1;
        MarkdownEditor *editor = m_editor;
        QTimer::singleShot(0, this, [this, editor, ratio] {
            if (editor != m_editor)
                return;
            setScrollRatio(editor->verticalScrollBar(), ratio);
            syncFromLeader();
        });
    }
    syncFromLeader();
    if (m_editMode)
        m_editor->setFocus();
}

DocumentTab *MainWindow::currentTab() const
{
    for (const auto &tab : m_tabs) {
        if (tab->editor == m_editor)
            return tab.get();
    }
    return nullptr;
}

DocumentTab *MainWindow::tabAt(int index) const
{
    return index >= 0 && index < int(m_tabs.size()) ? m_tabs[index].get() : nullptr;
}

int MainWindow::indexOf(const DocumentTab *tab) const
{
    for (int i = 0; i < int(m_tabs.size()); ++i) {
        if (m_tabs[i].get() == tab)
            return i;
    }
    return -1;
}

int MainWindow::indexOfFile(const QString &path) const
{
    if (path.isEmpty())
        return -1;
    const QString clean = cleanPath(path);
    for (int i = 0; i < int(m_tabs.size()); ++i) {
        if (m_tabs[i]->filePath == clean)
            return i;
    }
    // Another spelling of the same file: different case on Windows and macOS, a link, ...
    // Asking the filesystem keeps Notes.md and notes.md apart where they are two files.
    if (!QFileInfo::exists(clean))
        return -1;
    const QByteArray wanted = fileIdentity(clean);
    if (wanted.isEmpty())
        return -1;
    for (int i = 0; i < int(m_tabs.size()); ++i) {
        const QString open = m_tabs[i]->filePath;
        if (!open.isEmpty() && fileIdentity(open) == wanted)
            return i;
    }
    return -1;
}

void MainWindow::updateTabText(DocumentTab *tab)
{
    const int index = indexOf(tab);
    if (index < 0 || index >= m_tabBar->count())
        return;
    const QString name = tab->filePath.isEmpty() ? tr("New document") : QFileInfo(tab->filePath).fileName();
    m_tabBar->setTabText(index, tab->editor->document()->isModified() ? name + QStringLiteral(" *") : name);
    m_tabBar->setTabToolTip(index, tab->filePath.isEmpty() ? name : QDir::toNativeSeparators(tab->filePath));
}

bool MainWindow::isSpare(const DocumentTab *tab) const
{
    return tab && tab->filePath.isEmpty() && !tab->editor->document()->isModified();
}

void MainWindow::rememberOpenTabs()
{
    QStringList files;
    for (const auto &tab : m_tabs) {
        if (!tab->filePath.isEmpty())
            files.append(tab->filePath);
    }
    QSettings settings;
    settings.setValue(QStringLiteral("session/openFiles"), files);
    // An unsaved new document has nothing to reopen; the next start shows the last saved one.
    if (m_filePath.isEmpty())
        settings.remove(QStringLiteral("session/lastFile"));
    else
        settings.setValue(QStringLiteral("session/lastFile"), m_filePath);
}
