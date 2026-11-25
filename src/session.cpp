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

// Saves where the current document was left: the cursor, and how far each pane was scrolled
// (scrolling does not move the cursor, so the visible part can be elsewhere).
void MainWindow::rememberPosition()
{
    if (m_filePath.isEmpty())
        return;
    QSettings settings;
    QVariantMap all = settings.value(QStringLiteral("session/positions")).toMap();
    QVariantMap spot{{QStringLiteral("cursor"), m_editor->textCursor().position()},
                     {QStringLiteral("scroll"), scrollRatio(m_preview->verticalScrollBar())}};
    // A hidden writing pane (Reader mode) has a stale position; the reading pane is what counts then.
    if (m_editor->isVisible())
        spot.insert(QStringLiteral("editorScroll"), scrollRatio(m_editor->verticalScrollBar()));
    all.insert(cleanPath(m_filePath), spot);
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
    QTimer::singleShot(0, this, [this, scroll, hasEditorScroll, editorScroll] {
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

// Crash recovery. Each running window owns "<id>.md" (the text) and "<id>.json" (which document
// it belongs to) in the app's data folder, plus "<id>.lock", held while the window is open.
// A copy whose lock nobody holds was left behind by a crash.
QString MainWindow::recoveryFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("recovery"));
}

QString MainWindow::recoveryFile(const QString &extension) const
{
    return QDir(recoveryFolder()).filePath(m_recoveryId + extension);
}

void MainWindow::startRecovery()
{
    QDir().mkpath(recoveryFolder());
    m_recoveryId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_recoveryLock = std::make_unique<QLockFile>(recoveryFile(QStringLiteral(".lock")));
    // 0 = only a dead process makes the lock stale, however long the window stays open.
    m_recoveryLock->setStaleLockTime(0);
    m_recoveryLock->tryLock(0);
}

// Returns true when a complete copy of the unsaved text is on disk.
bool MainWindow::writeRecovery()
{
    if (!m_editor->document()->isModified()) {
        clearRecovery();
        return false;
    }
    // The text first and the description last, so a description always points to a complete copy.
    QSaveFile text(recoveryFile(QStringLiteral(".md")));
    if (!text.open(QIODevice::WriteOnly) || text.write(m_editor->toPlainText().toUtf8()) < 0 || !text.commit())
        return false;
    const QJsonObject info{{QStringLiteral("path"), m_filePath},
                           {QStringLiteral("savedAt"), QDateTime::currentDateTime().toString(Qt::ISODate)}};
    QSaveFile json(recoveryFile(QStringLiteral(".json")));
    if (!json.open(QIODevice::WriteOnly) || json.write(QJsonDocument(info).toJson()) < 0)
        return false;
    return json.commit();
}

void MainWindow::clearRecovery()
{
    m_autosaveTimer.stop();
    QFile::remove(recoveryFile(QStringLiteral(".json")));
    QFile::remove(recoveryFile(QStringLiteral(".md")));
}

// Offers the newest copy left behind by a crashed window, if any.
void MainWindow::offerRecovery()
{
    const QDir folder(recoveryFolder());
    QString bestId;
    QJsonObject bestInfo;
    std::unique_ptr<QLockFile> bestLock;
    const QStringList descriptions = folder.entryList({QStringLiteral("*.json")}, QDir::Files);
    for (const QString &name : descriptions) {
        const QString id = QFileInfo(name).completeBaseName();
        if (id == m_recoveryId || !QFileInfo::exists(folder.filePath(id + QStringLiteral(".md"))))
            continue;
        // Locked = that window is still open (maybe a second light_md); leave its copy alone.
        auto lock = std::make_unique<QLockFile>(folder.filePath(id + QStringLiteral(".lock")));
        lock->setStaleLockTime(0);
        if (!lock->tryLock(0))
            continue;
        QFile file(folder.filePath(name));
        if (!file.open(QIODevice::ReadOnly))
            continue;
        const QJsonObject info = QJsonDocument::fromJson(file.readAll()).object();
        if (bestId.isEmpty() || info.value(QStringLiteral("savedAt")).toString()
                                    > bestInfo.value(QStringLiteral("savedAt")).toString()) {
            bestId = id;
            bestInfo = info;
            bestLock = std::move(lock);
        }
    }
    // A crashed window with nothing unsaved leaves only its lock behind; tidy those up.
    const QStringList locks = folder.entryList({QStringLiteral("*.lock")}, QDir::Files);
    for (const QString &name : locks) {
        const QString id = QFileInfo(name).completeBaseName();
        if (id == m_recoveryId || id == bestId || QFileInfo::exists(folder.filePath(id + QStringLiteral(".json"))))
            continue;
        QLockFile stale(folder.filePath(name));
        stale.setStaleLockTime(0);
        if (stale.tryLock(0))
            stale.unlock(); // Unlocking removes the file.
    }
    if (bestId.isEmpty())
        return;

    QFile textFile(folder.filePath(bestId + QStringLiteral(".md")));
    if (!textFile.open(QIODevice::ReadOnly))
        return;
    const QString text = QString::fromUtf8(textFile.readAll());
    textFile.close();
    const QString path = bestInfo.value(QStringLiteral("path")).toString();
    const QDateTime savedAt =
        QDateTime::fromString(bestInfo.value(QStringLiteral("savedAt")).toString(), Qt::ISODate);
    const QString name = path.isEmpty() ? QStringLiteral("a new document") : QFileInfo(path).fileName();

    QMessageBox box(QMessageBox::Question, QStringLiteral("Restore unsaved changes?"),
                    QStringLiteral("light_md closed unexpectedly while you had unsaved changes in <b>%1</b>.<br>"
                                   "A copy was kept automatically at %2.<br><br>"
                                   "Do you want to get those changes back?")
                        .arg(name.toHtmlEscaped(), QLocale().toString(savedAt.time(), QLocale::ShortFormat)),
                    QMessageBox::NoButton, this);
    QPushButton *restore = box.addButton(QStringLiteral("Restore"), QMessageBox::AcceptRole);
    box.addButton(QStringLiteral("Discard"), QMessageBox::RejectRole);
    box.setDefaultButton(restore);
    box.exec();

    const auto deleteOldCopy = [&] {
        QFile::remove(folder.filePath(bestId + QStringLiteral(".json")));
        QFile::remove(folder.filePath(bestId + QStringLiteral(".md")));
    };
    if (box.clickedButton() != restore) {
        deleteOldCopy();
        return;
    }
    // From here on, any early return keeps the old copy (only its lock is released by
    // bestLock going out of scope), so it is offered again on the next start.

    // Open the original document first, so Save writes back to it.
    if (!path.isEmpty() && QFileInfo::exists(path) && cleanPath(path) != m_filePath) {
        if (!askSaveIfModified() || !loadFile(path))
            return;
    } else if (path.isEmpty() && !m_filePath.isEmpty()) {
        if (!askSaveIfModified())
            return;
        m_editor->clear();
        setFilePath(QString());
    } else if (!path.isEmpty() && !QFileInfo::exists(path)) {
        // The original was moved or deleted; keep its name so Save recreates it there.
        setFilePath(path);
    }
    // Replace through the cursor so the change can be undone (Ctrl+Z shows the saved version).
    QTextCursor cursor(m_editor->document());
    cursor.select(QTextCursor::Document);
    cursor.insertText(text);
    m_editor->document()->setModified(true);
    updatePreview();
    // Only drop the old copy once this window holds its own copy on disk, so a second crash
    // right now still loses nothing.
    if (writeRecovery()) {
        m_autosaveTimer.stop();
        deleteOldCopy();
    }
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
    if (askSaveIfModified())
        loadFile(file);
}
