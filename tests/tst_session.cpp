#include "editor.h"
#include "mainwindow.h"
#include "theme.h"

#include <QAction>
#include <QApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMenuBar>
#include <QTabBar>
#include <QTextBrowser>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>

// Closing light_md and opening it again brings back the tabs, the cursor and the scroll position.
class TestSession : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;
    QString m_path;

    void writeDocument(int lines)
    {
        QFile file(m_path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        for (int i = 1; i <= lines; ++i)
            file.write(QStringLiteral("Line %1 with a few words in it\n").arg(i).toUtf8());
    }

    QString writeOther(const QString &name)
    {
        const QString path = QDir::cleanPath(QFileInfo(m_dir.filePath(name)).absoluteFilePath());
        QFile file(path);
        if (file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            file.write("# " + name.toUtf8() + "\n");
        return path;
    }

    static int tabCount(const MainWindow &window) { return int(window.m_tabs.size()); }

    QVariantMap savedSpot() const
    {
        return QSettings().value(QStringLiteral("session/positions")).toMap().value(m_path).toMap();
    }

    static void showWindow(MainWindow &window)
    {
        window.resize(900, 600);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
    }

    // Answers the next message box as soon as it shows up, however long that takes (a fixed
    // delay is too short on a slow CI machine, and an unanswered box waits forever).
    static void answerNextBox(const std::function<void(QMessageBox *)> &answer)
    {
        auto *timer = new QTimer;
        auto *clock = new QElapsedTimer;
        clock->start();
        QObject::connect(timer, &QTimer::timeout, [timer, clock, answer] {
            auto *box = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!box && clock->elapsed() < 30000)
                return;
            timer->stop();
            timer->deleteLater();
            delete clock;
            if (box)
                answer(box);
        });
        timer->start(20);
    }

    static void clickButton(QMessageBox *box, const QString &text)
    {
        for (QAbstractButton *button : box->buttons()) {
            if (button->text() == text)
                button->click();
        }
    }

private slots:
    void initTestCase()
    {
        // Keep the real settings, recent documents and recovery copies out of this.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("light_md-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("light_md-tests"));
        QSettings().clear();
        QVERIFY(m_dir.isValid());
        m_path = QDir::cleanPath(QFileInfo(m_dir.filePath(QStringLiteral("long.md"))).absoluteFilePath());
    }

    void cleanupTestCase()
    {
        QSettings().clear();
    }

    void reopensWhereLeftOff()
    {
        writeDocument(400);
        ThemeManager themes;
        {
            MainWindow window(&themes, m_path);
            showWindow(window);
            QTextCursor cursor = window.m_editor->textCursor();
            cursor.setPosition(5000);
            window.m_editor->setTextCursor(cursor);
            QScrollBar *bar = window.m_editor->verticalScrollBar();
            QVERIFY(bar->maximum() > 0);
            bar->setValue(bar->maximum() / 2);
            QVERIFY(window.close());
        }

        const QVariantMap spot = savedSpot();
        QCOMPARE(spot.value(QStringLiteral("cursor")).toInt(), 5000);
        QVERIFY(spot.contains(QStringLiteral("editorScroll")));
        QCOMPARE(QSettings().value(QStringLiteral("session/lastFile")).toString(), m_path);

        // No file given: the last document comes back, at the same place.
        MainWindow window(&themes);
        showWindow(window);
        QCOMPARE(window.m_filePath, m_path);
        QCOMPARE(window.m_editor->textCursor().position(), 5000);
        const double editorScroll = spot.value(QStringLiteral("editorScroll")).toDouble();
        QTRY_VERIFY(qAbs(MainWindow::scrollRatio(window.m_editor->verticalScrollBar()) - editorScroll) < 0.02);
    }

    void cursorPastTheEndStaysInside()
    {
        // The document got shorter outside light_md since it was last open.
        writeDocument(3);
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        QCOMPARE(window.m_editor->textCursor().position(), window.m_editor->document()->characterCount() - 1);
    }

    void newDocumentIsNotReopened()
    {
        ThemeManager themes;
        {
            MainWindow window(&themes, m_path);
            showWindow(window);
            window.newDocument();
            QCOMPARE(tabCount(window), 2);
            QVERIFY(window.close());
        }
        QVERIFY(!QSettings().contains(QStringLiteral("session/lastFile")));
        QCOMPARE(QSettings().value(QStringLiteral("session/openFiles")).toStringList(), QStringList{m_path});
    }

    void welcomeTabMakesWayForTheFirstDocument()
    {
        QSettings().clear();
        ThemeManager themes;
        MainWindow window(&themes);
        showWindow(window);
        QCOMPARE(tabCount(window), 1);
        QVERIFY(window.m_filePath.isEmpty());
        QVERIFY(window.loadFile(m_path));
        QCOMPARE(tabCount(window), 1);
        QCOMPARE(window.m_filePath, m_path);
    }

    void documentsOpenInTheirOwnTabs()
    {
        const QString other = writeOther(QStringLiteral("other.md"));
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        QVERIFY(window.loadFile(other));
        QCOMPARE(tabCount(window), 2);
        QCOMPARE(window.m_filePath, other);
        // Opening one that is already open just goes to its tab.
        QVERIFY(window.loadFile(m_path));
        QCOMPARE(tabCount(window), 2);
        QCOMPARE(window.m_filePath, m_path);
        QCOMPARE(window.m_editors->currentWidget(), window.m_editor);
    }

    void eachTabHasItsOwnUndo()
    {
        const QString other = writeOther(QStringLiteral("other.md"));
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        window.m_editor->insertPlainText(QStringLiteral("typed in the first tab"));
        QVERIFY(window.m_undoAction->isEnabled());
        QVERIFY(window.loadFile(other));
        QVERIFY(!window.m_undoAction->isEnabled());
        QVERIFY(!window.isWindowModified());
        window.switchToTab(0);
        QVERIFY(window.m_undoAction->isEnabled());
        QVERIFY(window.isWindowModified());
        window.m_undoAction->trigger();
        QVERIFY(!window.m_editor->toPlainText().contains(QStringLiteral("typed")));
    }

    void reopensEveryTab()
    {
        const QString other = writeOther(QStringLiteral("other.md"));
        ThemeManager themes;
        {
            MainWindow window(&themes, m_path);
            showWindow(window);
            window.loadFile(other);
            window.switchToTab(0);
            QVERIFY(window.close());
        }
        MainWindow window(&themes);
        showWindow(window);
        QCOMPARE(tabCount(window), 2);
        QCOMPARE(window.m_tabs[0]->filePath, m_path);
        QCOMPARE(window.m_tabs[1]->filePath, other);
        // The tab that was in front comes back in front.
        QCOMPARE(window.m_filePath, m_path);
    }

    void closingTheLastTabLeavesABlankOne()
    {
        QSettings().clear();
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        QVERIFY(window.closeTab(0));
        QCOMPARE(tabCount(window), 1);
        QVERIFY(window.m_filePath.isEmpty());
        QVERIFY(window.m_editor->toPlainText().isEmpty());
    }

    void crashRecoveryRestoresEveryTab()
    {
        QSettings().clear();
        const QString other = writeOther(QStringLiteral("other.md"));
        ThemeManager themes;
        {
            // Unsaved changes in two tabs, then the window disappears without closing (a crash).
            auto *window = new MainWindow(&themes, m_path);
            showWindow(*window);
            window->loadFile(other);
            for (const auto &tab : window->m_tabs) {
                tab->editor->insertPlainText(QStringLiteral("unsaved "));
                QVERIFY(window->writeRecovery(tab.get()));
            }
            delete window;
        }
        // The next start asks once; answer Restore.
        answerNextBox([](QMessageBox *box) { clickButton(box, QStringLiteral("Restore")); });
        MainWindow window(&themes);
        showWindow(window);
        QTRY_COMPARE(tabCount(window), 2);
        for (const auto &tab : window.m_tabs) {
            QVERIFY(tab->editor->document()->isModified());
            QVERIFY(tab->editor->toPlainText().startsWith(QStringLiteral("unsaved ")));
        }
        QSet<QString> files;
        for (const auto &tab : window.m_tabs)
            files.insert(tab->filePath);
        QCOMPARE(files, QSet<QString>({m_path, other}));
        // Nothing left to offer next time.
        for (const auto &tab : window.m_tabs)
            window.clearRecovery(tab.get());
    }

    void twoCopiesOfOneDocumentGetTwoTabs()
    {
        QSettings().clear();
        writeDocument(3);
        ThemeManager themes;
        // Two windows edit the same document, then both crash.
        auto *first = new MainWindow(&themes, m_path);
        showWindow(*first);
        auto *second = new MainWindow(&themes, m_path);
        showWindow(*second);
        first->m_editor->insertPlainText(QStringLiteral("first "));
        QVERIFY(first->writeRecovery(first->currentTab()));
        second->m_editor->insertPlainText(QStringLiteral("second "));
        QVERIFY(second->writeRecovery(second->currentTab()));
        delete first;
        delete second;
        answerNextBox([](QMessageBox *box) { clickButton(box, QStringLiteral("Restore")); });
        MainWindow window(&themes);
        showWindow(window);
        QTRY_COMPARE(tabCount(window), 2);
        QStringList texts;
        int tiedToTheFile = 0;
        for (const auto &tab : window.m_tabs) {
            QVERIFY(tab->editor->document()->isModified());
            texts.append(tab->editor->toPlainText().section(QLatin1Char(' '), 0, 0));
            tiedToTheFile += tab->filePath == m_path ? 1 : 0;
        }
        texts.sort();
        QCOMPARE(texts, QStringList({QStringLiteral("first"), QStringLiteral("second")}));
        QCOMPARE(tiedToTheFile, 1);
        for (const auto &tab : window.m_tabs)
            window.clearRecovery(tab.get());
    }

    void closingSavesTheTabItAskedAbout()
    {
        QSettings().clear();
        writeDocument(3);
        const QString other = writeOther(QStringLiteral("other.md"));
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        window.m_editor->insertPlainText(QStringLiteral("keep me "));
        // While "Save changes?" is up, another document opens (from Finder, say); then Save.
        answerNextBox([&window, other](QMessageBox *box) {
            window.loadFile(other);
            box->button(QMessageBox::Save)->click();
        });
        QVERIFY(window.closeTab(0));
        QCOMPARE(tabCount(window), 1);
        QCOMPARE(window.m_filePath, other);
        QFile saved(m_path);
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QVERIFY(saved.readAll().startsWith("keep me "));
        QFile untouched(other);
        QVERIFY(untouched.open(QIODevice::ReadOnly));
        QCOMPARE(untouched.readAll(), QByteArray("# other.md\n"));
    }

    void distractionFreeShowsOnlyTheWritingPane()
    {
        QSettings().clear();
        writeDocument(3);
        const QString other = writeOther(QStringLiteral("other.md"));
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        QTest::keyClick(window.m_editor, Qt::Key_F11);
        QVERIFY(window.m_distractionFree);
        QVERIFY(window.m_distractionFreeAction->isChecked());
        QVERIFY(!window.m_fileBar->isVisible());
        QVERIFY(!window.m_formatBar->isVisible());
        QVERIFY(!window.m_preview->isVisible());
        QVERIFY(!window.m_status->isVisible());
        QVERIFY(!window.menuBar()->isVisible());
        QVERIFY(window.m_editor->isVisible());
        QVERIFY(window.m_editor->isCentered());
        QVERIFY(window.m_editor->viewport()->geometry().x() > 0);
        // A second tab doesn't bring the tab bar back, and its pane is centered too.
        QVERIFY(window.loadFile(other));
        QCOMPARE(tabCount(window), 2);
        QVERIFY(!window.m_tabBar->isVisible());
        QVERIFY(window.m_editor->isCentered());
        // Esc goes back to everything as it was.
        QTest::keyClick(window.m_editor, Qt::Key_Escape);
        QVERIFY(!window.m_distractionFree);
        QVERIFY(window.m_fileBar->isVisible());
        QVERIFY(window.m_preview->isVisible());
        QVERIFY(window.menuBar()->isVisible());
        QVERIFY(window.m_tabBar->isVisible());
        QVERIFY(!window.m_editor->isCentered());
        QVERIFY(!window.isFullScreen());
    }

    void shortcutsWorkWithoutTheMenu()
    {
        QSettings().clear();
        ThemeManager themes;
        MainWindow window(&themes);
        showWindow(window);
        window.setDistractionFree(true);
        window.m_editor->setPlainText(QStringLiteral("word"));
        window.m_editor->selectAll();
        QTest::keyClick(window.m_editor, Qt::Key_B, Qt::ControlModifier);
        QCOMPARE(window.m_editor->toPlainText(), QStringLiteral("**word**"));
        // Reader needs the writing pane hidden, so it ends distraction-free mode.
        QTest::keyClick(window.m_editor, Qt::Key_R, Qt::ControlModifier);
        QVERIFY(!window.m_distractionFree);
        QVERIFY(!window.m_editMode);
    }

    void startsFromReaderAndGoesBackToIt()
    {
        QSettings().clear();
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        window.setEditMode(false);
        window.setDistractionFree(true);
        QVERIFY(window.m_editor->isVisible());
        window.setDistractionFree(false);
        QVERIFY(!window.m_editMode);
        QVERIFY(window.m_preview->isVisible());
    }

    void anotherSpellingFindsTheSameTab()
    {
        QSettings().clear();
        ThemeManager themes;
        MainWindow window(&themes, m_path);
        showWindow(window);
        QCOMPARE(window.indexOfFile(m_dir.filePath(QStringLiteral("./long.md"))), 0);
#ifdef Q_OS_WIN
        // Windows paths ignore case.
        QCOMPARE(window.indexOfFile(m_path.toUpper()), 0);
#endif
        QCOMPARE(window.indexOfFile(m_dir.filePath(QStringLiteral("missing.md"))), -1);
    }
};

QTEST_MAIN(TestSession)
#include "tst_session.moc"
