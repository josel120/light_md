#include "highlighter.h"
#include "spellcheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QPlainTextEdit>
#include <QStandardPaths>
#include <QTest>
#include <QTextBlock>
#include <QTextLayout>

// The dictionaries downloaded at build time (cmake/Spelling.cmake), and the red wave under
// misspelled words in the writing pane.
class TestSpellCheck : public QObject
{
    Q_OBJECT

private:
    QPlainTextEdit editor;
    MarkdownHighlighter *highlighter = nullptr;
    SpellChecker speller;

    bool wavy(int line, int column) const
    {
        const QTextBlock block = editor.document()->findBlockByNumber(line);
        for (const QTextLayout::FormatRange &range : block.layout()->formats()) {
            if (column >= range.start && column < range.start + range.length
                && range.format.underlineStyle() == QTextCharFormat::SpellCheckUnderline)
                return true;
        }
        return false;
    }

    void show(const QString &text)
    {
        editor.setPlainText(text);
        highlighter->rehighlight();
    }

private slots:
    void initTestCase()
    {
        // personal.txt goes to a test folder, not the real one.
        QStandardPaths::setTestModeEnabled(true);
        QCoreApplication::setOrganizationName(QStringLiteral("light_md-tests"));
        QCoreApplication::setApplicationName(QStringLiteral("light_md-tests"));
        QDir(SpellChecker::userFolder()).removeRecursively();
        SpellChecker::setSearchPaths({QStringLiteral(LIGHT_MD_TEST_DICTIONARIES)});
        QVERIFY(speller.setLanguage(QStringLiteral("en")));
        highlighter = new MarkdownHighlighter(editor.document());
        highlighter->setSpellChecker(&speller);
        highlighter->setStyle(editor.palette(), 10);
    }

    void cleanupTestCase()
    {
        QDir(SpellChecker::userFolder()).removeRecursively();
    }

    void everyLanguageLoads()
    {
        for (const QString &language : SpellChecker::languages()) {
            SpellChecker checker;
            QElapsedTimer timer;
            timer.start();
            QVERIFY2(checker.setLanguage(language), qPrintable(language));
            qInfo("%s loaded in %lld ms", qPrintable(language), timer.elapsed());
        }
    }

    void wordsInEachLanguage()
    {
        const QList<std::tuple<QString, QString, QString>> cases{
            {QStringLiteral("en"), QStringLiteral("because"), QStringLiteral("becuase")},
            {QStringLiteral("es"), QStringLiteral("canción"), QStringLiteral("cancion")},
            {QStringLiteral("pt"), QStringLiteral("informação"), QStringLiteral("informacao")},
            {QStringLiteral("fr"), QStringLiteral("l’été"), QStringLiteral("éttté")},
            // The German dictionary is Latin-1, the others UTF-8.
            {QStringLiteral("de"), QStringLiteral("Größe"), QStringLiteral("Grösse")},
        };
        for (const auto &[language, right, wrong] : cases) {
            SpellChecker checker;
            QVERIFY(checker.setLanguage(language));
            QVERIFY2(checker.isCorrect(right), qPrintable(right));
            QVERIFY2(!checker.isCorrect(wrong), qPrintable(wrong));
        }
    }

    void suggestionsIncludeTheRightWord()
    {
        QVERIFY(speller.suggestions(QStringLiteral("becuase")).contains(QStringLiteral("because")));
        SpellChecker german;
        QVERIFY(german.setLanguage(QStringLiteral("de")));
        QVERIFY(german.suggestions(QStringLiteral("Grösse")).contains(QStringLiteral("Größe")));
    }

    void offChecksNothing()
    {
        SpellChecker checker;
        QVERIFY(!checker.setLanguage(QString()));
        QVERIFY(checker.isCorrect(QStringLiteral("becuase")));
    }

    void misspelledWordsAreUnderlined()
    {
        show(QStringLiteral("This is becuase of **teh** title"));
        QVERIFY(!wavy(0, 1));
        QVERIFY(wavy(0, 10));
        QVERIFY(wavy(0, 22));
    }

    void codeAddressesAndNamesAreNotChecked()
    {
        show(QStringLiteral("`becuase` [link](https://exmple.com/becuase) www.foo.com/xyzzy x86 snake_name PDF\n"
                            "```\nbecuase\n```"));
        QVERIFY(!wavy(0, 3));
        QVERIFY(!wavy(0, 30));
        QVERIFY(!wavy(0, 52));
        QVERIFY(!wavy(2, 2));
    }

    void linkTextIsChecked()
    {
        show(QStringLiteral("[becuase](https://example.com)"));
        QVERIFY(wavy(0, 3));
    }

    void addedWordsCountAsCorrect()
    {
        QVERIFY(!speller.isCorrect(QStringLiteral("lightmd")));
        QVERIFY(speller.addWord(QStringLiteral("lightmd")));
        QVERIFY(speller.isCorrect(QStringLiteral("lightmd")));
        // Kept for next time, in every language.
        SpellChecker later;
        QVERIFY(later.setLanguage(QStringLiteral("es")));
        QVERIFY(later.isCorrect(QStringLiteral("lightmd")));
    }
};

QTEST_MAIN(TestSpellCheck)
#include "tst_spellcheck.moc"
