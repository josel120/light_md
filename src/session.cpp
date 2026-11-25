// Recent documents, where each document was left off, and crash recovery.
#include "mainwindow.h"

#include "editor.h"

#include <QAbstractButton>
#include <QAction>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTimer>
#include <QUuid>
#include <algorithm>

// Saves where a document was left: the cursor, and how far each pane was scrolled (scrolling
// does not move the cursor, so the visible part can be elsewhere).
void MainWindow::rememberPosition(DocumentTab *tab)
{
    if (!tab || tab->filePath.isEmpty())
        return;
    const bool current = tab->editor == m_editor;
    QSettings settings;
    QVariantMap all = settings.value(QStringLiteral("session/positions")).toMap();
    QVariantMap spot{{QStringLiteral("cursor"), tab->editor->textCursor().position()},
                     {QStringLiteral("scroll"), current ? scrollRatio(m_preview->verticalScrollBar())
                                                        : tab->previewScroll}};
    // A hidden writing pane (Reader mode) has a stale position; the reading pane is what counts then.
    // A tab in the background keeps its own pane's position (or the one it was reopened with).
    if (current ? tab->editor->isVisible() : m_editMode) {
        const double editorScroll =
            tab->pendingEditorScroll >= 0 ? tab->pendingEditorScroll : scrollRatio(tab->editor->verticalScrollBar());
        spot.insert(QStringLiteral("editorScroll"), editorScroll);
    }
    all.insert(tab->filePath, spot);
    // Only documents still in the Recent list are worth remembering; this keeps the list short.
    const QStringList recent = recentFiles();
    for (auto it = all.begin(); it != all.end();) {
        if (recent.contains(it.key(), Qt::CaseInsensitive))
            ++it;
        else
            it = all.erase(it);
    }
    settings.setValue(QStringLiteral("session/positions"), all);
}

// Puts the current tab's document back where it was left.
void MainWindow::restorePosition(const QString &path)
{
    const QVariantMap saved =
        QSettings().value(QStringLiteral("session/positions")).toMap().value(cleanPath(path)).toMap();
    if (saved.isEmpty())
        return;
    QTextCursor cursor = m_editor->textCursor();
    const int last = m_editor->document()->characterCount() - 1;
    cursor.setPosition(qBound(0, saved.value(QStringLiteral("cursor")).toInt(), last));
    m_editor->setTextCursor(cursor);
    const double scroll = saved.value(QStringLiteral("scroll")).toDouble();
    const bool hasEditorScroll = saved.contains(QStringLiteral("editorScroll"));
    const double editorScroll = saved.value(QStringLiteral("editorScroll")).toDouble();
    // Wait for the window to be laid out, so both panes know their height before scrolling.
    DocumentTab *tab = currentTab();
    MarkdownEditor *editor = m_editor;
    QTimer::singleShot(0, this, [this, tab, editor, scroll, hasEditorScroll, editorScroll] {
        if (indexOf(tab) < 0 || tab->editor != editor)
            return;
        // Reopened behind another tab: switchToTab scrolls it when it is first shown.
        if (editor != m_editor) {
            tab->previewScroll = scroll;
            tab->previewLeads = !hasEditorScroll;
            tab->pendingEditorScroll = hasEditorScroll ? editorScroll : -1;
            return;
        }
        QScrollBar *editorBar = m_editor->verticalScrollBar();
        QScrollBar *previewBar = m_preview->verticalScrollBar();
        if (m_editor->isVisible() && hasEditorScroll) {
            m_previewLeads = false;
            setScrollRatio(editorBar, editorScroll);
            if (!m_syncScroll)
                setScrollRatio(previewBar, scroll);
        } else {
            // Left in Reader mode (or saved by an older version): the reading position leads.
            m_previewLeads = true;
            setScrollRatio(previewBar, scroll);
            if (!m_syncScroll)
                m_editor->centerCursor();
        }
        syncFromLeader();
    });
}

// Crash recovery. Each open tab owns "<id>.md" (the text) and "<id>.json" (which document it
// belongs to) in the app's data folder, plus "<id>.lock", held while the tab is open. A copy
// whose lock nobody holds was left behind by a crash.
QString MainWindow::recoveryFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("recovery"));
}

QString MainWindow::recoveryFile(const QString &id, const QString &extension)
{
    return QDir(recoveryFolder()).filePath(id + extension);
}

void MainWindow::startRecovery(DocumentTab *tab)
{
    QDir().mkpath(recoveryFolder());
    tab->recoveryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tab->recoveryLock = std::make_unique<QLockFile>(recoveryFile(tab->recoveryId, QStringLiteral(".lock")));
    // 0 = only a dead process makes the lock stale, however long the tab stays open.
    tab->recoveryLock->setStaleLockTime(0);
    tab->recoveryLock->tryLock(0);
}

// Returns true when a complete copy of the tab's unsaved text is on disk.
bool MainWindow::writeRecovery(DocumentTab *tab)
{
    if (!tab->editor->document()->isModified()) {
        clearRecovery(tab);
        return false;
    }
    // The text first and the description last, so a description always points to a complete copy.
    QSaveFile text(recoveryFile(tab->recoveryId, QStringLiteral(".md")));
    if (!text.open(QIODevice::WriteOnly) || text.write(tab->editor->toPlainText().toUtf8()) < 0 || !text.commit())
        return false;
    const QJsonObject info{{QStringLiteral("path"), tab->filePath},
                           {QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
    QSaveFile json(recoveryFile(tab->recoveryId, QStringLiteral(".json")));
    if (!json.open(QIODevice::WriteOnly) || json.write(QJsonDocument(info).toJson()) < 0)
        return false;
    return json.commit();
}

void MainWindow::clearRecovery(DocumentTab *tab)
{
    if (!tab)
        return;
    QFile::remove(recoveryFile(tab->recoveryId, QStringLiteral(".json")));
    QFile::remove(recoveryFile(tab->recoveryId, QStringLiteral(".md")));
}

// Offers back the copies left behind by crashed windows, each in its own tab.
void MainWindow::offerRecovery()
{
    const QDir folder(recoveryFolder());
    QStringList ownIds;
    for (const auto &tab : m_tabs)
        ownIds.append(tab->recoveryId);

    struct LeftBehind
    {
        QString id;
        QJsonObject info;
        std::unique_ptr<QLockFile> lock;
    };
    std::vector<LeftBehind> copies;
    const QStringList descriptions = folder.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &name : descriptions) {
        const QString id = QFileInfo(name).completeBaseName();
        if (ownIds.contains(id) || !QFileInfo::exists(folder.filePath(id + QStringLiteral(".md"))))
            continue;
        // Locked = that tab is still open (maybe in a second light_md); leave its copy alone.
        auto lock = std::make_unique<QLockFile>(folder.filePath(id + QStringLiteral(".lock")));
        lock->setStaleLockTime(0);
        if (!lock->tryLock(0))
            continue;
        QFile file(folder.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        copies.push_back({id, QJsonDocument::fromJson(file.readAll()).object(), std::move(lock)});
    }
    // A crashed tab with nothing unsaved leaves only its lock behind; tidy those up.
    const QStringList locks = folder.entryList({QStringLiteral("*.lock")}, QDir::Files);
    for (const QString &name : locks) {
        const QString id = QFileInfo(name).completeBaseName();
        if (ownIds.contains(id) || QFileInfo::exists(folder.filePath(id + QStringLiteral(".json"))))
            continue;
        QLockFile stale(folder.filePath(name));
        stale.setStaleLockTime(0);
        if (stale.tryLock(0))
            stale.unlock(); // Unlocking removes the file.
    }
    if (copies.empty())
        return;
    // Newest first: if two copies belong to the same document (two windows had it open), the
    // newest is restored into that document and the others open as new documents.
    const auto savedAt = [](const LeftBehind &copy) { return copy.info.value(QStringLiteral("savedAt")).toString(); };
    std::sort(copies.begin(), copies.end(),
              [&](const LeftBehind &a, const LeftBehind &b) { return savedAt(a) > savedAt(b); });

    const auto nameOf = [](const LeftBehind &copy) {
        const QString path = copy.info.value(QStringLiteral("path")).toString();
        return path.isEmpty() ? QStringLiteral("a new document") : QFileInfo(path).fileName();
    };
    QString question;
    if (copies.size() == 1) {
        const QDateTime time = QDateTime::fromString(savedAt(copies.front()), Qt::ISODate);
        question = QStringLiteral("light_md closed unexpectedly while you had unsaved changes in <b>%1</b>.<br>"
                                  "A copy was kept automatically at %2.<br><br>"
                                  "Do you want to get those changes back?")
                       .arg(nameOf(copies.front()).toHtmlEscaped(),
                            QLocale().toString(time.time(), QLocale::ShortFormat));
    } else {
        QStringList names;
        for (const LeftBehind &copy : copies)
            names.append(QStringLiteral("<b>%1</b>").arg(nameOf(copy).toHtmlEscaped()));
        question = QStringLiteral("light_md closed unexpectedly while you had unsaved changes in %1 documents: "
                                  "%2.<br>Copies were kept automatically.<br><br>"
                                  "Do you want to get those changes back? Each one opens in its own tab.")
                       .arg(copies.size())
                       .arg(names.join(QStringLiteral(", ")));
    }
    QMessageBox box(QMessageBox::Question, QStringLiteral("Restore unsaved changes?"), question,
                    QMessageBox::NoButton, this);
    QPushButton *restore = box.addButton(QStringLiteral("Restore"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Discard"), QMessageBox::RejectRole);
    box.setDefaultButton(restore);
    box.exec();

    int extraCopies = 0;
    for (const LeftBehind &copy : copies) {
        const auto deleteOldCopy = [&] {
            QFile::remove(folder.filePath(copy.id + QStringLiteral(".json")));
            QFile::remove(folder.filePath(copy.id + QStringLiteral(".md")));
        };
        if (box.clickedButton() != restore) {
            deleteOldCopy();
            continue;
        }
        // From here on, a copy that cannot be restored is kept (only its lock is released when
        // `copies` goes away), so it is offered again on the next start.
        QFile textFile(folder.filePath(copy.id + QStringLiteral(".md")));
        if (!textFile.open(QIODevice::ReadOnly))
            continue;
        const QString text = QString::fromUtf8(textFile.readAll());
        textFile.close();

        // Open the original document first, so Save writes back to it. If a newer copy of it
        // was just restored there, this one gets its own untitled tab and needs Save As.
        const QString path = copy.info.value(QStringLiteral("path")).toString();
        const int open = indexOfFile(path);
        const bool taken = open >= 0 && tabAt(open)->editor->document()->isModified();
        if (taken)
            ++extraCopies;
        if (!taken && !path.isEmpty() && QFileInfo::exists(path)) {
            if (!loadFile(path))
                continue;
        } else {
            if (!isSpare(currentTab()))
                switchToTab(indexOf(addTab()));
            // A moved or deleted original keeps its name, so Save recreates it there.
            if (!taken && !path.isEmpty())
                setFilePath(path);
        }
        // Replace through the cursor so the change can be undone (Ctrl+Z shows the saved version).
        QTextCursor cursor(m_editor->document());
        cursor.select(QTextCursor::Document);
        cursor.insertText(text);
        m_editor->document()->setModified(true);
        updatePreview();
        // Only drop the old copy once this tab holds its own copy on disk, so a second crash
        // right now still loses nothing.
        if (writeRecovery(currentTab()))
            deleteOldCopy();
    }
    if (box.clickedButton() == restore && extraCopies > 0)
        m_status->setText(QStringLiteral("Unsaved changes restored. Older copies of the same document "
                                         "opened as new documents; use Save As to keep them."));
    else if (box.clickedButton() == restore)
        m_status->setText(QStringLiteral("Unsaved changes restored. Save to keep them."));
}

void MainWindow::addRecentFile(const QString &path)
{
    const QString clean = cleanPath(path);
    QStringList files = recentFiles();
    files.removeIf([&](const QString &f) { return f.compare(clean, Qt::CaseInsensitive) == 0; });
    files.prepend(clean);
    while (files.size() > 10)
        files.removeLast();
    setRecentFiles(files);
}

void MainWindow::rebuildRecentMenu()
{
    m_recentMenu->clear();
    const QStringList files = recentFiles();
    if (files.isEmpty()) {
        m_recentMenu->addAction(QStringLiteral("No recent documents yet"))->setEnabled(false);
        return;
    }
    for (const QString &file : files) {
        QAction *action = m_recentMenu->addAction(QFileInfo(file).fileName());
        action->setToolTip(QDir::toNativeSeparators(file));
        connect(action, &QAction::triggered, this, [this, file] { openRecent(file); });
    }
    m_recentMenu->addSeparator();
    m_recentMenu->addAction(QStringLiteral("Clear this list"), this, [] { setRecentFiles(QStringList()); });
}

void MainWindow::openRecent(const QString &file)
{
    if (!QFileInfo::exists(file)) {
        QMessageBox::information(this, QStringLiteral("Document not found"),
                                 QStringLiteral("This document was moved or deleted:\n%1\n\n"
                                                "It has been removed from the list.")
                                     .arg(QDir::toNativeSeparators(file)));
        QStringList files = recentFiles();
        files.removeAll(file);
        setRecentFiles(files);
        return;
    }
    loadFile(file);
}
