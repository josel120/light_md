#include "theme.h"

#include <QAbstractButton>
#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QSaveFile>
#include <QScrollBar>
#include <QSettings>
#include <QSignalBlocker>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(ThemeManager *themes, const QString &initialFile = QString())
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

        m_splitter = new QSplitter(Qt::Horizontal, central);
        m_editor = new QPlainTextEdit(m_splitter);
        m_editor->setPlaceholderText(QStringLiteral("Type your text here..."));
        m_editor->setUndoRedoEnabled(true);
        m_editor->viewport()->installEventFilter(this);
        m_preview = new QTextBrowser(m_splitter);
        m_preview->setOpenExternalLinks(true);
        QFont readFont = m_preview->font();
        readFont.setPointSizeF(readFont.pointSizeF() + 2);
        m_preview->setFont(readFont);
        QFont writeFont = m_editor->font();
        writeFont.setPointSizeF(writeFont.pointSizeF() + 1);
        m_editor->setFont(writeFont);
        m_splitter->addWidget(m_editor);
        m_splitter->addWidget(m_preview);
        m_splitter->setStretchFactor(0, 1);
        m_splitter->setStretchFactor(1, 1);
        m_splitter->setSizes({550, 550});
        root->addWidget(m_splitter, 1);

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

        connect(m_editor, &QPlainTextEdit::textChanged, this, [this] {
            m_previewTimer.start();
            m_statusTimer.start();
        });
        connect(m_editor->document(), &QTextDocument::modificationChanged, this, [this](bool changed) {
            setWindowModified(changed);
            updateStatus();
        });

        QSettings settings;
        const QByteArray geometry = settings.value(QStringLiteral("geometry")).toByteArray();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);

        // Classic defaults on first run: buttons visible, two panes (edit mode).
        const bool showButtons = settings.value(QStringLiteral("ui/showButtons"), true).toBool();
        const bool editMode = settings.value(QStringLiteral("ui/editMode"), true).toBool();

        if (!initialFile.isEmpty()) {
            loadFile(initialFile);
        } else {
            m_editor->setPlainText(QStringLiteral(
                "# Welcome to light_md\n\n"
                "Write on the **left** and see the result on the *right*.\n\n"
                "## Getting started\n\n"
                "- Use the buttons at the top to format your text\n"
                "- Hide the buttons anytime with **Options → Show buttons** (Ctrl+Shift+B)\n"
                "- **Ctrl+E** toggles the writing pane; **Ctrl+R** or **Esc** is Read only\n"
                "- Every action also lives under **Options** with a shortcut\n"));
            m_editor->document()->setModified(false);
            updateTitle();
            updatePreview();
        }

        // Apply after load so a failed open still gets remembered chrome.
        setShowButtons(showButtons);
        setEditMode(editMode);
    }

    bool loadFile(const QString &path)
    {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("Could not open"),
                                 QStringLiteral("Could not read the file:\n%1").arg(path));
            return false;
        }
        m_editor->setPlainText(QString::fromUtf8(file.readAll()));
        m_editor->document()->setModified(false);
        setFilePath(path);
        m_preview->verticalScrollBar()->setValue(0);
        updatePreview();
        // Keep the user's current Edit/Reader choice (remembered in QSettings).
        return true;
    }

protected:
    void closeEvent(QCloseEvent *event) override
    {
        if (!askSaveIfModified()) {
            event->ignore();
            return;
        }
        QSettings().setValue(QStringLiteral("geometry"), saveGeometry());
        event->accept();
    }

    void keyPressEvent(QKeyEvent *event) override
    {
        // Esc returns to Reader without stealing Esc from dialogs (those are modal).
        if (event->key() == Qt::Key_Escape && m_editMode) {
            setEditMode(false);
            event->accept();
            return;
        }
        QMainWindow::keyPressEvent(event);
    }

    // Dropping a document on the window opens it instead of pasting its path.
    void dragEnterEvent(QDragEnterEvent *event) override
    {
        if (droppedFile(event->mimeData()).isEmpty())
            return;
        event->acceptProposedAction();
    }

    void dropEvent(QDropEvent *event) override
    {
        const QString path = droppedFile(event->mimeData());
        if (path.isEmpty())
            return;
        event->acceptProposedAction();
        if (askSaveIfModified())
            loadFile(path);
    }

    bool eventFilter(QObject *watched, QEvent *event) override
    {
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

private:
    QAction *addOption(QMenu *menu, const QString &text, const QString &tip, const QKeySequence &keys)
    {
        auto *action = menu->addAction(text);
        action->setStatusTip(tip);
        action->setToolTip(tip);
        action->setShortcutContext(Qt::ApplicationShortcut);
        if (!keys.isEmpty())
            action->setShortcut(keys);
        return action;
    }

    QPushButton *addBarButton(QHBoxLayout *bar, QAction *action)
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

    void buildMenus()
    {
        auto *options = menuBar()->addMenu(QStringLiteral("&Options"));

        auto *actNew = addOption(options, QStringLiteral("&New"), QStringLiteral("Start a blank document"),
                                 QKeySequence::New);
        auto *actOpen = addOption(options, QStringLiteral("&Open..."), QStringLiteral("Open a saved document"),
                                  QKeySequence::Open);
        auto *actSave = addOption(options, QStringLiteral("&Save"), QStringLiteral("Save your changes"),
                                  QKeySequence::Save);
        auto *actSaveAs = addOption(options, QStringLiteral("Save &as..."),
                                    QStringLiteral("Save a copy with a different name"),
                                    QKeySequence::SaveAs);
        options->addSeparator();
        auto *actPdf = addOption(options, QStringLiteral("Export to &PDF..."),
                                 QStringLiteral("Create a nicely formatted PDF to send or share"),
                                 QKeySequence(QStringLiteral("Ctrl+Shift+E")));
        auto *actPrint = addOption(options, QStringLiteral("&Print..."), QStringLiteral("Print the document"),
                                   QKeySequence::Print);
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
        // Ctrl+Shift+Q often means Quit on Linux desktops; use Period instead.
        auto *actQuote = addOption(m_formatMenu, QStringLiteral("&Quote"),
                                   QStringLiteral("Mark the lines as a quote"),
                                   QKeySequence(QStringLiteral("Ctrl+Shift+.")));
        auto *actLink = addOption(m_formatMenu, QStringLiteral("Lin&k"),
                                  QStringLiteral("Add a link to a web page"),
                                  QKeySequence(QStringLiteral("Ctrl+K")));

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

        auto *actHelp = addOption(options, QStringLiteral("&Help"), QStringLiteral("How to use light_md"),
                                  QKeySequence::HelpContents);

        // Classic file bar (same order as origin/main).
        addBarButton(m_fileBarLayout, actNew);
        addBarButton(m_fileBarLayout, actOpen);
        addBarButton(m_fileBarLayout, actSave);
        addBarButton(m_fileBarLayout, actSaveAs);
        m_fileBarLayout->addSpacing(12);
        addBarButton(m_fileBarLayout, actPdf);
        addBarButton(m_fileBarLayout, actPrint);
        m_fileBarLayout->addStretch();
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
        addBarButton(m_formatBarLayout, actQuote);
        addBarButton(m_formatBarLayout, actLink);
        m_formatBarLayout->addStretch();

        connect(actNew, &QAction::triggered, this, [this] { newDocument(); });
        connect(actOpen, &QAction::triggered, this, [this] { openDocument(); });
        connect(actSave, &QAction::triggered, this, [this] { saveDocument(); });
        connect(actSaveAs, &QAction::triggered, this, [this] { saveDocumentAs(); });
        connect(actPdf, &QAction::triggered, this, [this] { exportPdf(); });
        connect(actPrint, &QAction::triggered, this, [this] { printDocument(); });
        connect(actHelp, &QAction::triggered, this, [this] { showHelp(); });
        connect(m_editAction, &QAction::toggled, this, [this](bool on) { setEditMode(on); });
        connect(actRead, &QAction::triggered, this, [this] { setEditMode(false); });
        connect(m_showButtonsAction, &QAction::toggled, this, [this](bool on) { setShowButtons(on); });

        connect(actUndo, &QAction::triggered, m_editor, &QPlainTextEdit::undo);
        connect(actRedo, &QAction::triggered, m_editor, &QPlainTextEdit::redo);
        connect(m_editor, &QPlainTextEdit::undoAvailable, actUndo, &QAction::setEnabled);
        connect(m_editor, &QPlainTextEdit::redoAvailable, actRedo, &QAction::setEnabled);
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
    }

    void setShowButtons(bool show)
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

    void setEditMode(bool editing)
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
        m_editMode = editing;
        m_editor->setVisible(editing);
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
        } else {
            m_preview->setFocus(Qt::OtherFocusReason);
        }
        QSettings().setValue(QStringLiteral("ui/editMode"), editing);
        updateStatus();
    }

    static QString droppedFile(const QMimeData *mime)
    {
        if (!mime->hasUrls())
            return QString();
        for (const QUrl &url : mime->urls()) {
            if (url.isLocalFile())
                return url.toLocalFile();
        }
        return QString();
    }

    void updatePreview()
    {
        // Keep the reading position; otherwise the pane jumps to the top on every keystroke.
        QScrollBar *bar = m_preview->verticalScrollBar();
        const int scroll = bar->value();
        m_preview->setMarkdown(m_editor->toPlainText());
        bar->setValue(scroll);
    }

    void updateStatus()
    {
        static const QRegularExpression spaces(QStringLiteral("\\s+"));
        const int words = m_editor->toPlainText().split(spaces, Qt::SkipEmptyParts).size();
        const bool modified = m_editor->document()->isModified();
        const QString mode = m_editMode ? QStringLiteral("Editing") : QStringLiteral("Reading");
        const QString chrome = m_showButtons ? QStringLiteral("Buttons") : QStringLiteral("Minimal");
        m_status->setText(QStringLiteral("%1 · %2    Words: %3    File: %4%5")
                              .arg(mode, chrome)
                              .arg(words)
                              .arg(m_filePath.isEmpty() ? QStringLiteral("(not saved)")
                                                        : QDir::toNativeSeparators(m_filePath))
                              .arg(modified ? QStringLiteral("    (unsaved changes)") : QString()));
    }

    void updateTitle()
    {
        const QString name = m_filePath.isEmpty() ? QStringLiteral("New document")
                                                  : QFileInfo(m_filePath).fileName();
        setWindowTitle(QStringLiteral("%1[*] - light_md").arg(name));
    }

    void setFilePath(const QString &path)
    {
        m_filePath = path;
        if (!path.isEmpty())
            QSettings().setValue(QStringLiteral("lastFolder"), QFileInfo(path).absolutePath());
        updateTitle();
        updateStatus();
    }

    QString lastFolder() const
    {
        return QSettings().value(QStringLiteral("lastFolder")).toString();
    }

    // Visits the lines touched by the selection (or the cursor's line).
    template <typename Fn>
    void forEachSelectedLine(Fn fn)
    {
        if (!m_editMode)
            setEditMode(true);
        QTextCursor cursor = m_editor->textCursor();
        QTextDocument *doc = m_editor->document();
        QTextBlock first = doc->findBlock(cursor.selectionStart());
        QTextBlock last = doc->findBlock(cursor.selectionEnd());
        // A selection that ends right at the start of a line does not include that line.
        if (cursor.hasSelection() && last != first && cursor.selectionEnd() == last.position())
            last = last.previous();
        const bool single = first == last;

        cursor.beginEditBlock();
        for (QTextBlock block = first; block.isValid(); block = block.next()) {
            if (single || !block.text().trimmed().isEmpty())
                fn(block);
            if (block == last)
                break;
        }
        cursor.endEditBlock();
        m_editor->setFocus();
    }

    template <typename PrefixFn>
    void prefixLines(PrefixFn prefixFor)
    {
        int n = 0;
        forEachSelectedLine([&](const QTextBlock &block) {
            QTextCursor c(block);
            c.insertText(prefixFor(++n));
        });
    }

    void setHeading(const QString &marks)
    {
        static const QRegularExpression existing(QStringLiteral("^#{1,6}\\s*"));
        forEachSelectedLine([&](const QTextBlock &block) {
            QTextCursor c(block);
            const QRegularExpressionMatch match = existing.match(block.text());
            if (match.hasMatch())
                c.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, match.capturedLength());
            c.insertText(marks);
        });
    }

    void wrapSelection(const QString &marks)
    {
        if (!m_editMode)
            setEditMode(true);
        QTextCursor cursor = m_editor->textCursor();
        cursor.beginEditBlock();
        if (cursor.hasSelection()) {
            const QString selected = cursor.selectedText();
            cursor.insertText(marks + selected + marks);
        } else {
            // Leave the sample word selected so the user can type over it.
            const QString sample = QStringLiteral("text");
            cursor.insertText(marks + sample + marks);
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, marks.size());
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, sample.size());
        }
        cursor.endEditBlock();
        m_editor->setTextCursor(cursor);
        m_editor->setFocus();
    }

    void insertLink()
    {
        if (!m_editMode)
            setEditMode(true);
        QTextCursor cursor = m_editor->textCursor();
        bool ok = false;
        QString address = QInputDialog::getText(this, QStringLiteral("Add link"),
                                                QStringLiteral("Web page address:"), QLineEdit::Normal,
                                                QStringLiteral("https://"), &ok)
                              .trimmed();
        if (!ok || address.isEmpty() || address == QStringLiteral("https://")) {
            m_editor->setFocus();
            return;
        }
        if (!address.contains(QStringLiteral("://")) && !address.startsWith(QStringLiteral("mailto:")))
            address.prepend(QStringLiteral("https://"));

        const bool hadSelection = cursor.hasSelection();
        const QString label = hadSelection ? cursor.selectedText() : QStringLiteral("link text");
        cursor.beginEditBlock();
        cursor.insertText(QStringLiteral("[%1](%2)").arg(label, address));
        cursor.endEditBlock();
        if (!hadSelection) {
            cursor.movePosition(QTextCursor::Left, QTextCursor::MoveAnchor, address.size() + 3);
            cursor.movePosition(QTextCursor::Left, QTextCursor::KeepAnchor, label.size());
        }
        m_editor->setTextCursor(cursor);
        m_editor->setFocus();
    }

    bool askSaveIfModified()
    {
        if (!m_editor->document()->isModified())
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
        if (answer == QMessageBox::Save)
            return saveDocument();
        return true;
    }

    void newDocument()
    {
        if (!askSaveIfModified())
            return;
        m_editor->clear();
        m_editor->document()->setModified(false);
        setFilePath(QString());
        updatePreview();
        setEditMode(true);
    }

    void openDocument()
    {
        if (!askSaveIfModified())
            return;
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Open document"), lastFolder(),
            QStringLiteral("Markdown documents (*.md *.markdown *.txt);;All files (*)"));
        if (path.isEmpty())
            return;
        loadFile(path);
    }

    bool writeTo(const QString &path)
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
        setFilePath(path);
        return true;
    }

    bool saveDocument()
    {
        if (m_filePath.isEmpty())
            return saveDocumentAs();
        return writeTo(m_filePath);
    }

    QString suggestedPath(const QString &extension) const
    {
        const QString base = m_filePath.isEmpty() ? QStringLiteral("document")
                                                  : QFileInfo(m_filePath).completeBaseName();
        const QString folder = m_filePath.isEmpty() ? lastFolder() : QFileInfo(m_filePath).absolutePath();
        return folder.isEmpty() ? base + extension : QDir(folder).filePath(base + extension);
    }

    bool saveDocumentAs()
    {
        QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Save as"), suggestedPath(QStringLiteral(".md")),
            QStringLiteral("Markdown documents (*.md);;All files (*)"));
        if (path.isEmpty())
            return false;
        if (QFileInfo(path).suffix().isEmpty())
            path += QStringLiteral(".md");
        return writeTo(path);
    }

    void exportPdf()
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

    void printDocument()
    {
        updatePreview();
        QPrinter printer(QPrinter::HighResolution);
        QPrintDialog dialog(&printer, this);
        dialog.setWindowTitle(QStringLiteral("Print"));
        if (dialog.exec() == QDialog::Accepted)
            m_preview->document()->print(&printer);
    }

    void showHelp()
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
                "Ctrl+R or Esc — Read only</p>"
                "<p><b>File</b><br>"
                "Ctrl+N New · Ctrl+O Open · Ctrl+S Save · Ctrl+Shift+S Save as<br>"
                "Ctrl+Shift+E Export PDF · Ctrl+P Print</p>"
                "<p><b>Format</b> (while editing)<br>"
                "Ctrl+Z Undo · Ctrl+Shift+Z / Ctrl+Y Redo<br>"
                "Ctrl+B Bold · Ctrl+I Italic · Ctrl+K Link<br>"
                "Ctrl+1 Big title · Ctrl+2 Subtitle<br>"
                "Ctrl+Shift+8 Bullet list · Ctrl+Shift+7 Numbered list · Ctrl+Shift+. Quote</p>"
                "<p><b>Theme</b><br>"
                "Ctrl+Shift+1 System · Ctrl+Shift+2 Minimal · Ctrl+Shift+3 Omarchy "
                "(Omarchy only where available)</p>"
                "<p>F1 — Help. You can also drag a document onto the window to open it.</p>"));
    }

    ThemeManager *m_themes = nullptr;
    QWidget *m_fileBar = nullptr;
    QWidget *m_formatBar = nullptr;
    QHBoxLayout *m_fileBarLayout = nullptr;
    QHBoxLayout *m_formatBarLayout = nullptr;
    QSplitter *m_splitter = nullptr;
    QPlainTextEdit *m_editor = nullptr;
    QTextBrowser *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_showButtonsAction = nullptr;
    QMenu *m_formatMenu = nullptr;
    QTimer m_previewTimer;
    QTimer m_statusTimer;
    QString m_filePath;
    bool m_editMode = true;
    bool m_showButtons = true;
};

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

    ThemeManager themes;
    themes.setKind(loadInitialTheme());

    const QStringList args = QApplication::arguments();
    const QString initialFile = args.size() > 1 ? args.at(1) : QString();
    MainWindow window(&themes, initialFile);
    window.show();
    return app.exec();
}
