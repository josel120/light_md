#include "editor.h"
#include "mainwindow.h"
#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScrollBar>
#include <QSettings>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextCursor>

// Closing a document and opening light_md again brings back the cursor and the scroll position.
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
            QVERIFY(window.close());
        }
        QVERIFY(!QSettings().contains(QStringLiteral("session/lastFile")));
    }
};

QTEST_MAIN(TestSession)
#include "tst_session.moc"
