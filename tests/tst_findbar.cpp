#include "findbar.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QTest>
#include <QTextCursor>

class TestFindBar : public QObject
{
    Q_OBJECT

private:
    QWidget window;
    QPlainTextEdit *editor = nullptr;
    FindBar *bar = nullptr;

    void setUp(const QString &text, const QString &find, const QString &replace = QString(), bool matchCase = false)
    {
        editor->setPlainText(text);
        editor->moveCursor(QTextCursor::Start);
        bar->findField()->setText(find);
        bar->replaceField()->setText(replace);
        bar->matchCaseBox()->setChecked(matchCase);
    }

    int selectionStart() const { return editor->textCursor().selectionStart(); }

private slots:
    void init()
    {
        delete bar;
        delete editor;
        editor = new QPlainTextEdit(&window);
        bar = new FindBar(editor, &window);
    }

    void findsMatchesInOrder()
    {
        setUp(QStringLiteral("cat dog cat"), QStringLiteral("cat"));
        QVERIFY(bar->findNext(false));
        QCOMPARE(selectionStart(), 0);
        QVERIFY(bar->findNext(false));
        QCOMPARE(selectionStart(), 8);
    }

    void continuesFromTheTop()
    {
        setUp(QStringLiteral("cat dog cat"), QStringLiteral("cat"));
        bar->findNext(false);
        bar->findNext(false);
        QVERIFY(bar->findNext(false));
        QCOMPARE(selectionStart(), 0);
        QCOMPARE(bar->info(), QStringLiteral("Continued from the top"));
    }

    void findsBackwardFromTheEnd()
    {
        setUp(QStringLiteral("cat dog cat"), QStringLiteral("cat"));
        QVERIFY(bar->findNext(true));
        QCOMPARE(selectionStart(), 8);
        QCOMPARE(bar->info(), QStringLiteral("Continued from the end"));
    }

    void notFoundKeepsCursor()
    {
        setUp(QStringLiteral("cat dog"), QStringLiteral("bird"));
        QVERIFY(!bar->findNext(false));
        QCOMPARE(bar->info(), QStringLiteral("Not found"));
        QVERIFY(!editor->textCursor().hasSelection());
    }

    void matchCase()
    {
        setUp(QStringLiteral("Cat cat"), QStringLiteral("cat"), QString(), true);
        QVERIFY(bar->findNext(false));
        QCOMPARE(selectionStart(), 4);
    }

    void callsTheWindowBack()
    {
        int before = 0, found = 0;
        bar->beforeSearch = [&] { ++before; };
        bar->matchFound = [&] { ++found; };
        setUp(QStringLiteral("cat"), QStringLiteral("cat"));
        bar->findNext(false);
        QCOMPARE(before, 1);
        QCOMPARE(found, 1);
    }

    void replaceOneFindsThenReplaces()
    {
        setUp(QStringLiteral("cat dog cat"), QStringLiteral("cat"), QStringLiteral("cow"));
        bar->replaceOne();
        QCOMPARE(editor->toPlainText(), QStringLiteral("cat dog cat"));
        bar->replaceOne();
        QCOMPARE(editor->toPlainText(), QStringLiteral("cow dog cat"));
        QCOMPARE(selectionStart(), 8);
    }

    void replaceAllIgnoresCase()
    {
        setUp(QStringLiteral("cat Cat CAT"), QStringLiteral("cat"), QStringLiteral("dog"));
        QCOMPARE(bar->replaceAll(), 3);
        QCOMPARE(editor->toPlainText(), QStringLiteral("dog dog dog"));
        QCOMPARE(bar->info(), QStringLiteral("3 replacements"));
    }

    void replaceAllWithMatchCase()
    {
        setUp(QStringLiteral("cat Cat cat"), QStringLiteral("cat"), QStringLiteral("dog"), true);
        QCOMPARE(bar->replaceAll(), 2);
        QCOMPARE(editor->toPlainText(), QStringLiteral("dog Cat dog"));
    }

    void replaceAllWhenNewTextContainsOld()
    {
        setUp(QStringLiteral("a b a"), QStringLiteral("a"), QStringLiteral("aa"));
        QCOMPARE(bar->replaceAll(), 2);
        QCOMPARE(editor->toPlainText(), QStringLiteral("aa b aa"));
    }

    void replaceAllIsOneUndoStep()
    {
        setUp(QStringLiteral("x x x"), QStringLiteral("x"), QStringLiteral("y"));
        bar->replaceAll();
        editor->undo();
        QCOMPARE(editor->toPlainText(), QStringLiteral("x x x"));
    }

    void replaceAllNothingFound()
    {
        setUp(QStringLiteral("abc"), QStringLiteral("z"), QStringLiteral("y"));
        QCOMPARE(bar->replaceAll(), 0);
        QCOMPARE(bar->info(), QStringLiteral("Not found"));
    }

    void openingTakesTheSelectedWord()
    {
        setUp(QStringLiteral("find this word"), QString());
        QTextCursor cursor(editor->document());
        cursor.setPosition(5);
        cursor.setPosition(9, QTextCursor::KeepAnchor);
        editor->setTextCursor(cursor);
        bar->openFind();
        QCOMPARE(bar->findField()->text(), QStringLiteral("this"));
        QVERIFY(!bar->isHidden());
        bar->closeBar();
        QVERIFY(bar->isHidden());
    }
};

QTEST_MAIN(TestFindBar)
#include "tst_findbar.moc"
