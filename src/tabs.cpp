// Tabs: several documents open at once, each with its own writing pane, undo history, unsaved
// marker and recovery copy. The preview is shared and shows the current tab.
#include "mainwindow.h"

#include "editor.h"
#include "findbar.h"
#include "highlighter.h"
#include "spellcheck.h"

#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTabBar>
#include <QTextBrowser>
#include <QTextDocument>
#include <QTimer>

#include <filesystem>

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
    tab->highlighter->setSpellChecker(m_speller.get());
    tab->highlighter->setStyle(editor->palette(), m_editorBaseSize + m_textSize);
    editor->speller = m_speller.get();
    editor->addWord = [this](const QString &word) { addToDictionary(word); };

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
    if (m_previewLeads)
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
    const std::filesystem::path wanted = QFileInfo(clean).filesystemFilePath();
    for (int i = 0; i < int(m_tabs.size()); ++i) {
        const QString open = m_tabs[i]->filePath;
        std::error_code error;
        if (!open.isEmpty() && std::filesystem::equivalent(QFileInfo(open).filesystemFilePath(), wanted, error))
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
