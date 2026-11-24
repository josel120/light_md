#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QIcon>
#include <QInputDialog>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollBar>
#include <QSettings>
#include <QShortcut>
#include <QSplitter>
#include <QTextBlock>
#include <QTextBrowser>
#include <QTextCursor>
#include <QTextDocument>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

class MainWindow : public QMainWindow
{
public:
    MainWindow()
    {
        resize(1100, 700);

        auto *central = new QWidget(this);
        auto *root = new QVBoxLayout(central);

        // File bar: everyday words, no technical terms.
        auto *fileBar = new QHBoxLayout;
        auto *btnNew = makeButton(QStringLiteral("New"), QStringLiteral("Start a blank document"),
                                  QKeySequence::New, central);
        auto *btnOpen = makeButton(QStringLiteral("Open..."), QStringLiteral("Open a saved document"),
                                   QKeySequence::Open, central);
        auto *btnRecent = makeButton(QStringLiteral("Recent"), QStringLiteral("Open a document you used recently"),
                                     QKeySequence(), central);
        m_recentMenu = new QMenu(btnRecent);
        m_recentMenu->setToolTipsVisible(true);
        btnRecent->setMenu(m_recentMenu);
        auto *btnSave = makeButton(QStringLiteral("Save"), QStringLiteral("Save your changes"),
                                   QKeySequence::Save, central);
        auto *btnSaveAs = makeButton(QStringLiteral("Save as..."),
                                     QStringLiteral("Save a copy with a different name"),
                                     QKeySequence::SaveAs, central);
        auto *btnPdf = makeButton(QStringLiteral("Export to PDF..."),
                                  QStringLiteral("Create a nicely formatted PDF to send or share"),
                                  QKeySequence(), central);
        auto *btnPrint = makeButton(QStringLiteral("Print..."), QStringLiteral("Print the document"),
                                    QKeySequence::Print, central);
        auto *btnFind = makeButton(QStringLiteral("Find..."), QStringLiteral("Find and replace words in the text"),
                                   QKeySequence::Find, central);
        auto *btnHelp = makeButton(QStringLiteral("Help"), QStringLiteral("How to use light_md"),
                                   QKeySequence::HelpContents, central);
        fileBar->addWidget(btnNew);
        fileBar->addWidget(btnOpen);
        fileBar->addWidget(btnRecent);
        fileBar->addWidget(btnSave);
        fileBar->addWidget(btnSaveAs);
        fileBar->addSpacing(12);
        fileBar->addWidget(btnPdf);
        fileBar->addWidget(btnPrint);
        fileBar->addStretch();
        fileBar->addWidget(btnFind);
        fileBar->addWidget(btnHelp);
        root->addLayout(fileBar);

        // Format bar: the user formats with buttons, not symbols.
        auto *formatBar = new QHBoxLayout;
        auto *btnUndo = makeButton(QStringLiteral("Undo"), QStringLiteral("Take back the last change"),
                                   QKeySequence::Undo, central);
        auto *btnRedo = makeButton(QStringLiteral("Redo"), QStringLiteral("Bring back the change you undid"),
                                   QKeySequence::Redo, central);
        auto *btnTitle = makeButton(QStringLiteral("Big title"),
                                    QStringLiteral("Turn the line into a big title"),
                                    QKeySequence(QStringLiteral("Ctrl+1")), central);
        auto *btnSubtitle = makeButton(QStringLiteral("Subtitle"),
                                       QStringLiteral("Turn the line into a subtitle"),
                                       QKeySequence(QStringLiteral("Ctrl+2")), central);
        auto *btnBold = makeButton(QStringLiteral("Bold"), QStringLiteral("Make the selected text bold"),
                                   QKeySequence::Bold, central);
        auto *btnItalic = makeButton(QStringLiteral("Italic"), QStringLiteral("Make the selected text italic"),
                                     QKeySequence::Italic, central);
        auto *btnBullets = makeButton(QStringLiteral("Bullet list"),
                                      QStringLiteral("Put a bullet in front of each selected line"),
                                      QKeySequence(), central);
        auto *btnNumbered = makeButton(QStringLiteral("Numbered list"),
                                       QStringLiteral("Number the selected lines: 1, 2, 3..."),
                                       QKeySequence(), central);
        auto *btnChecklist = makeButton(QStringLiteral("Checklist"),
                                        QStringLiteral("Put a box to tick in front of each selected line"),
                                        QKeySequence(), central);
        auto *btnQuote = makeButton(QStringLiteral("Quote"), QStringLiteral("Mark the lines as a quote"),
                                    QKeySequence(), central);
        auto *btnLink = makeButton(QStringLiteral("Link"), QStringLiteral("Add a link to a web page"),
                                   QKeySequence(QStringLiteral("Ctrl+K")), central);
        auto *btnInsert = makeButton(QStringLiteral("Insert"),
                                     QStringLiteral("Add a picture, a table, a code block or a separator line"),
                                     QKeySequence(), central);
        auto *insertMenu = new QMenu(btnInsert);
        insertMenu->addAction(QStringLiteral("Picture..."), QKeySequence(QStringLiteral("Ctrl+Shift+P")), this,
                              [this] { insertImage(); });
        insertMenu->addAction(QStringLiteral("Table"), QKeySequence(QStringLiteral("Ctrl+Shift+T")), this,
                              [this] { insertBlock(QStringLiteral("| Column 1 | Column 2 |\n"
                                                                  "| --- | --- |\n"
                                                                  "| Text | Text |"),
                                                   QStringLiteral("Column 1")); });
        insertMenu->addAction(QStringLiteral("Code block"), QKeySequence(QStringLiteral("Ctrl+Shift+C")), this,
                              [this] { insertCodeBlock(); });
        insertMenu->addAction(QStringLiteral("Separator line"), QKeySequence(QStringLiteral("Ctrl+Shift+L")), this,
                              [this] { insertBlock(QStringLiteral("---")); });
        // Menu shortcuts only work while the menu is open unless the window knows about them too.
        addActions(insertMenu->actions());
        btnInsert->setMenu(insertMenu);
        formatBar->addWidget(btnUndo);
        formatBar->addWidget(btnRedo);
        formatBar->addSpacing(12);
        formatBar->addWidget(btnTitle);
        formatBar->addWidget(btnSubtitle);
        formatBar->addWidget(btnBold);
        formatBar->addWidget(btnItalic);
        formatBar->addWidget(btnBullets);
        formatBar->addWidget(btnNumbered);
        formatBar->addWidget(btnChecklist);
        formatBar->addWidget(btnQuote);
        formatBar->addWidget(btnLink);
        formatBar->addWidget(btnInsert);
        formatBar->addStretch();
        root->addLayout(formatBar);

        // Two panes: write on the left, read the formatted result on the right.
        auto *splitter = new QSplitter(Qt::Horizontal, central);
        m_editor = new QPlainTextEdit(splitter);
        m_editor->setPlaceholderText(QStringLiteral("Type your text here..."));
        m_editor->viewport()->installEventFilter(this);
        m_preview = new QTextBrowser(splitter);
        m_preview->setOpenExternalLinks(true);
        QFont readFont = m_preview->font();
        readFont.setPointSizeF(readFont.pointSizeF() + 2);
        m_preview->setFont(readFont);
        QFont writeFont = m_editor->font();
        writeFont.setPointSizeF(writeFont.pointSizeF() + 1);
        m_editor->setFont(writeFont);
        splitter->addWidget(m_editor);
        splitter->addWidget(m_preview);
        splitter->setSizes({550, 550});
        root->addWidget(splitter, 1);

        // Find bar: hidden until the user asks for it.
        m_findBar = new QWidget(central);
        auto *findLayout = new QHBoxLayout(m_findBar);
        findLayout->setContentsMargins(0, 0, 0, 0);
        m_findEdit = new QLineEdit(m_findBar);
        m_findEdit->setPlaceholderText(QStringLiteral("Word to find"));
        m_findEdit->setClearButtonEnabled(true);
        auto *btnPrevious = new QPushButton(QStringLiteral("Previous"), m_findBar);
        auto *btnNext = new QPushButton(QStringLiteral("Next"), m_findBar);
        m_replaceEdit = new QLineEdit(m_findBar);
        m_replaceEdit->setPlaceholderText(QStringLiteral("Replace with"));
        m_replaceEdit->setClearButtonEnabled(true);
        auto *btnReplace = new QPushButton(QStringLiteral("Replace"), m_findBar);
        auto *btnReplaceAll = new QPushButton(QStringLiteral("Replace all"), m_findBar);
        m_matchCase = new QCheckBox(QStringLiteral("Match case"), m_findBar);
        m_findInfo = new QLabel(m_findBar);
        auto *btnCloseFind = new QPushButton(QStringLiteral("Close"), m_findBar);
        btnPrevious->setToolTip(QStringLiteral("Find the previous match (Shift+F3)"));
        btnNext->setToolTip(QStringLiteral("Find the next match (Enter or F3)"));
        btnCloseFind->setToolTip(QStringLiteral("Hide the find bar (Esc)"));
        findLayout->addWidget(new QLabel(QStringLiteral("Find:"), m_findBar));
        findLayout->addWidget(m_findEdit, 1);
        findLayout->addWidget(btnPrevious);
        findLayout->addWidget(btnNext);
        findLayout->addSpacing(12);
        findLayout->addWidget(m_replaceEdit, 1);
        findLayout->addWidget(btnReplace);
        findLayout->addWidget(btnReplaceAll);
        findLayout->addSpacing(12);
        findLayout->addWidget(m_matchCase);
        findLayout->addWidget(m_findInfo);
        findLayout->addWidget(btnCloseFind);
        m_findBar->hide();
        root->addWidget(m_findBar);

        m_status = new QLabel(central);
        root->addWidget(m_status);

        setCentralWidget(central);
        setAcceptDrops(true);

        // Short delay before refreshing the preview so typing never feels slow.
        m_previewTimer.setSingleShot(true);
        m_previewTimer.setInterval(150);
        connect(&m_previewTimer, &QTimer::timeout, this, [this] { updatePreview(); });

        connect(btnNew, &QPushButton::clicked, this, [this] { newDocument(); });
        connect(btnOpen, &QPushButton::clicked, this, [this] { openDocument(); });
        connect(btnSave, &QPushButton::clicked, this, [this] { saveDocument(); });
        connect(btnSaveAs, &QPushButton::clicked, this, [this] { saveDocumentAs(); });
        connect(btnPdf, &QPushButton::clicked, this, [this] { exportPdf(); });
        connect(btnPrint, &QPushButton::clicked, this, [this] { printDocument(); });
        connect(btnHelp, &QPushButton::clicked, this, [this] { showHelp(); });
        connect(m_recentMenu, &QMenu::aboutToShow, this, [this] { rebuildRecentMenu(); });

        connect(btnFind, &QPushButton::clicked, this, [this] { showFindBar(m_findEdit); });
        auto *replaceShortcut = new QShortcut(QKeySequence::Replace, this);
        connect(replaceShortcut, &QShortcut::activated, this, [this] { showFindBar(m_replaceEdit); });
        auto *nextShortcut = new QShortcut(QKeySequence::FindNext, this);
        connect(nextShortcut, &QShortcut::activated, this, [this] { findText(false); });
        auto *previousShortcut = new QShortcut(QKeySequence::FindPrevious, this);
        connect(previousShortcut, &QShortcut::activated, this, [this] { findText(true); });
        auto *closeFindShortcut = new QShortcut(QKeySequence(Qt::Key_Escape), m_findBar);
        closeFindShortcut->setContext(Qt::WidgetWithChildrenShortcut);
        connect(closeFindShortcut, &QShortcut::activated, this, [this] { hideFindBar(); });
        connect(btnCloseFind, &QPushButton::clicked, this, [this] { hideFindBar(); });
        connect(btnNext, &QPushButton::clicked, this, [this] { findText(false); });
        connect(btnPrevious, &QPushButton::clicked, this, [this] { findText(true); });
        connect(m_findEdit, &QLineEdit::returnPressed, this, [this] { findText(false); });
        connect(m_findEdit, &QLineEdit::textEdited, this, [this] { m_findInfo->clear(); });
        connect(m_replaceEdit, &QLineEdit::returnPressed, this, [this] { replaceOne(); });
        connect(btnReplace, &QPushButton::clicked, this, [this] { replaceOne(); });
        connect(btnReplaceAll, &QPushButton::clicked, this, [this] { replaceAll(); });

        connect(btnUndo, &QPushButton::clicked, m_editor, &QPlainTextEdit::undo);
        connect(btnRedo, &QPushButton::clicked, m_editor, &QPlainTextEdit::redo);
        connect(m_editor, &QPlainTextEdit::undoAvailable, btnUndo, &QPushButton::setEnabled);
        connect(m_editor, &QPlainTextEdit::redoAvailable, btnRedo, &QPushButton::setEnabled);
        btnUndo->setEnabled(false);
        btnRedo->setEnabled(false);

        connect(btnTitle, &QPushButton::clicked, this, [this] { setHeading(QStringLiteral("# ")); });
        connect(btnSubtitle, &QPushButton::clicked, this, [this] { setHeading(QStringLiteral("## ")); });
        connect(btnQuote, &QPushButton::clicked, this, [this] { prefixLines([](int) { return QStringLiteral("> "); }); });
        connect(btnBullets, &QPushButton::clicked, this, [this] { prefixLines([](int) { return QStringLiteral("- "); }); });
        connect(btnNumbered, &QPushButton::clicked, this,
                [this] { prefixLines([](int n) { return QStringLiteral("%1. ").arg(n); }); });
        connect(btnChecklist, &QPushButton::clicked, this,
                [this] { prefixLines([](int) { return QStringLiteral("- [ ] "); }); });
        connect(btnBold, &QPushButton::clicked, this, [this] { wrapSelection(QStringLiteral("**")); });
        connect(btnItalic, &QPushButton::clicked, this, [this] { wrapSelection(QStringLiteral("*")); });
        connect(btnLink, &QPushButton::clicked, this, [this] { insertLink(); });

        connect(m_editor, &QPlainTextEdit::textChanged, this, [this] { m_previewTimer.start(); });
        connect(m_editor->document(), &QTextDocument::modificationChanged, this, [this](bool changed) {
            setWindowModified(changed);
            updateStatus();
        });

        QSettings settings;
        restoreGeometry(settings.value(QStringLiteral("geometry")).toByteArray());

        m_editor->setPlainText(QStringLiteral(
            "# Welcome to light_md\n\n"
            "Write on the **left** and see the result on the *right*.\n\n"
            "## Getting started\n\n"
            "- Use the buttons at the top to format your text\n"
            "- You don't need to know Markdown\n"
            "- Keep your text with the Save button\n"));
        m_editor->document()->setModified(false);
        updateTitle();
        updatePreview();
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
        addRecentFile(path);
        m_preview->verticalScrollBar()->setValue(0);
        updatePreview();
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
    QPushButton *makeButton(const QString &text, const QString &tip, const QKeySequence &keys, QWidget *parent)
    {
        auto *button = new QPushButton(text, parent);
        button->setFocusPolicy(Qt::NoFocus);
        if (!keys.isEmpty()) {
            button->setShortcut(keys);
            button->setToolTip(QStringLiteral("%1 (%2)").arg(tip, keys.toString(QKeySequence::NativeText)));
        } else {
            button->setToolTip(tip);
        }
        return button;
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
        updateStatus();
    }

    void updateStatus()
    {
        static const QRegularExpression spaces(QStringLiteral("\\s+"));
        const int words = m_editor->toPlainText().split(spaces, Qt::SkipEmptyParts).size();
        const bool modified = m_editor->document()->isModified();
        m_status->setText(QStringLiteral("Words: %1    File: %2%3")
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
        // Pictures written as "photo.png" are looked up next to the document.
        m_preview->setSearchPaths(path.isEmpty() ? QStringList() : QStringList{QFileInfo(path).absolutePath()});
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

    // Inserts a multi-line element on its own lines, separated from the text around it by blank
    // lines (otherwise "---" under a line of text would turn that line into a title).
    void insertBlock(const QString &text, const QString &selectWord = QString())
    {
        QTextCursor cursor = m_editor->textCursor();
        cursor.clearSelection();
        // Go below the whole paragraph, so a table or list the cursor is in is not split.
        QTextBlock block = cursor.block();
        while (!block.text().trimmed().isEmpty() && block.next().isValid()
               && !block.next().text().trimmed().isEmpty())
            block = block.next();
        cursor.setPosition(block.position() + block.length() - 1);
        QString before;
        if (!block.text().trimmed().isEmpty())
            before = QStringLiteral("\n\n");
        else if (block.previous().isValid() && !block.previous().text().trimmed().isEmpty())
            before = QStringLiteral("\n");
        // End on a fresh line, with a blank line before any text that follows.
        const QTextBlock next = block.next();
        const QString after = !next.isValid() || !next.text().trimmed().isEmpty() ? QStringLiteral("\n") : QString();

        const int start = cursor.position() + before.size();
        cursor.beginEditBlock();
        cursor.insertText(before + text + after);
        cursor.endEditBlock();
        if (!selectWord.isEmpty()) {
            // Leave the sample word selected so the user can type over it.
            cursor.setPosition(start + text.indexOf(selectWord));
            cursor.movePosition(QTextCursor::NextCharacter, QTextCursor::KeepAnchor, selectWord.size());
        }
        m_editor->setTextCursor(cursor);
        m_editor->setFocus();
    }

    void insertCodeBlock()
    {
        QTextCursor cursor = m_editor->textCursor();
        if (!cursor.hasSelection()) {
            insertBlock(QStringLiteral("```\ncode\n```"), QStringLiteral("code"));
            return;
        }
        // Fence the selected lines without changing them.
        QTextDocument *doc = m_editor->document();
        const QTextBlock first = doc->findBlock(cursor.selectionStart());
        QTextBlock last = doc->findBlock(cursor.selectionEnd());
        if (last != first && cursor.selectionEnd() == last.position())
            last = last.previous();
        cursor.beginEditBlock();
        QTextCursor end(last);
        end.movePosition(QTextCursor::EndOfBlock);
        end.insertText(QStringLiteral("\n```"));
        QTextCursor begin(first);
        begin.insertText(QStringLiteral("```\n"));
        cursor.endEditBlock();
        m_editor->setFocus();
    }

    void insertImage()
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

    void showFindBar(QLineEdit *focusField)
    {
        const QTextCursor cursor = m_editor->textCursor();
        // Start from the selected word, unless it spans lines.
        if (cursor.hasSelection() && !cursor.selectedText().contains(QChar::ParagraphSeparator))
            m_findEdit->setText(cursor.selectedText());
        m_findInfo->clear();
        m_findBar->show();
        focusField->setFocus();
        focusField->selectAll();
    }

    void hideFindBar()
    {
        m_findBar->hide();
        m_editor->setFocus();
    }

    QTextDocument::FindFlags findFlags(bool backward) const
    {
        QTextDocument::FindFlags flags;
        if (backward)
            flags |= QTextDocument::FindBackward;
        if (m_matchCase->isChecked())
            flags |= QTextDocument::FindCaseSensitively;
        return flags;
    }

    bool findText(bool backward)
    {
        const QString text = m_findEdit->text();
        if (text.isEmpty()) {
            showFindBar(m_findEdit);
            return false;
        }
        m_findInfo->clear();
        const QTextDocument::FindFlags flags = findFlags(backward);
        if (m_editor->find(text, flags))
            return true;
        // Nothing more in this direction: continue from the other end of the document.
        const QTextCursor saved = m_editor->textCursor();
        QTextCursor cursor = saved;
        cursor.movePosition(backward ? QTextCursor::End : QTextCursor::Start);
        m_editor->setTextCursor(cursor);
        if (m_editor->find(text, flags)) {
            m_findInfo->setText(backward ? QStringLiteral("Continued from the end")
                                         : QStringLiteral("Continued from the top"));
            return true;
        }
        m_editor->setTextCursor(saved);
        m_findInfo->setText(QStringLiteral("Not found"));
        return false;
    }

    void replaceOne()
    {
        const QString text = m_findEdit->text();
        if (text.isEmpty())
            return;
        QTextCursor cursor = m_editor->textCursor();
        const Qt::CaseSensitivity cs = m_matchCase->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
        // The first click finds the word; the next one replaces it and moves on.
        if (cursor.hasSelection() && cursor.selectedText().compare(text, cs) == 0) {
            cursor.insertText(m_replaceEdit->text());
            m_editor->setTextCursor(cursor);
        }
        findText(false);
    }

    void replaceAll()
    {
        const QString text = m_findEdit->text();
        if (text.isEmpty())
            return;
        QTextDocument *doc = m_editor->document();
        const QTextDocument::FindFlags flags = findFlags(false);
        int count = 0;
        QTextCursor edit(doc);
        edit.beginEditBlock();
        // Searching from after each replacement avoids looping when the new text contains the old one.
        for (QTextCursor found = doc->find(text, 0, flags); !found.isNull(); found = doc->find(text, found, flags)) {
            found.insertText(m_replaceEdit->text());
            ++count;
        }
        edit.endEditBlock();
        m_findInfo->setText(count == 0 ? QStringLiteral("Not found")
                            : count == 1 ? QStringLiteral("1 replacement")
                                         : QStringLiteral("%1 replacements").arg(count));
    }

    static QStringList recentFiles() { return QSettings().value(QStringLiteral("recentFiles")).toStringList(); }

    static void setRecentFiles(const QStringList &files) { QSettings().setValue(QStringLiteral("recentFiles"), files); }

    static void addRecentFile(const QString &path)
    {
        const QString clean = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
        QStringList files = recentFiles();
        files.removeIf([&](const QString &f) { return f.compare(clean, Qt::CaseInsensitive) == 0; });
        files.prepend(clean);
        while (files.size() > 10)
            files.removeLast();
        setRecentFiles(files);
    }

    void rebuildRecentMenu()
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

    void openRecent(const QString &file)
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
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QMessageBox::warning(this, QStringLiteral("Could not save"),
                                 QStringLiteral("Could not write to the file:\n%1").arg(path));
            return false;
        }
        file.write(m_editor->toPlainText().toUtf8());
        m_editor->document()->setModified(false);
        setFilePath(path);
        addRecentFile(path);
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
                "<p>Write in the <b>left</b> pane. The <b>right</b> pane shows how it will look.</p>"
                "<p><b>To format text:</b> select it with the mouse and click a button, "
                "for example <i>Bold</i>. If nothing is selected, a sample word is inserted "
                "that you can replace by typing over it.</p>"
                "<p><b>Lists and titles:</b> select one or more lines and click <i>Bullet list</i>, "
                "<i>Numbered list</i>, <i>Checklist</i>, <i>Big title</i> or <i>Subtitle</i>.</p>"
                "<p><b>Pictures, tables and more:</b> click <i>Insert</i> and choose what to add.</p>"
                "<p><b>To find a word:</b> click <i>Find</i>. You can also replace it with another one.</p>"
                "<p><b>To share:</b> use <i>Export to PDF</i>; the PDF looks just like the right pane.</p>"
                "<p><b>Shortcuts:</b> Ctrl+S save, Ctrl+Z undo, Ctrl+B bold, Ctrl+I italic, "
                "Ctrl+K link, Ctrl+1 big title, Ctrl+2 subtitle, Ctrl+F find, Ctrl+H replace, "
                "F3 find next, Ctrl+Shift+P picture, Ctrl+Shift+T table, Ctrl+Shift+C code block, "
                "Ctrl+Shift+L separator line.</p>"
                "<p>You can also drag a document onto the window to open it, "
                "or reopen one from <i>Recent</i>.</p>"));
    }

    QPlainTextEdit *m_editor = nullptr;
    QTextBrowser *m_preview = nullptr;
    QLabel *m_status = nullptr;
    QMenu *m_recentMenu = nullptr;
    QWidget *m_findBar = nullptr;
    QLineEdit *m_findEdit = nullptr;
    QLineEdit *m_replaceEdit = nullptr;
    QCheckBox *m_matchCase = nullptr;
    QLabel *m_findInfo = nullptr;
    QTimer m_previewTimer;
    QString m_filePath;
};

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("light_md"));
    QApplication::setOrganizationName(QStringLiteral("light_md"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/light_md.png")));
    MainWindow window;
    // Lets a document be opened by double-click or "Open with" from File Explorer.
    const QStringList args = QApplication::arguments();
    if (args.size() > 1)
        window.loadFile(args.at(1));
    window.show();
    return app.exec();
}
