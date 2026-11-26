#pragma once

#include <QHash>
#include <QString>
#include <QStringConverter>
#include <QStringList>
#include <memory>

class Hunspell;

// Spell checking with Hunspell, in one of the interface languages. The dictionaries ship next
// to the program (see dictionaries/README.md); one in the user's dictionaries folder wins, and
// on Linux the system's are used when light_md has none. Words the user adds are kept in
// personal.txt in that folder and count as correct in every language.
class SpellChecker
{
public:
    SpellChecker();
    ~SpellChecker();

    // "en", "es", "pt", "fr", "de".
    static QStringList languages();
    // Where the user can put more dictionaries and where personal.txt lives.
    static QString userFolder();
    // Folders searched for dictionaries, user folder first. Tests replace them.
    static QStringList searchPaths();
    static void setSearchPaths(const QStringList &paths);

    // Loads the dictionary for `language` (one of languages()), or switches checking off for an
    // empty string or a language without a dictionary. Returns whether checking is on.
    bool setLanguage(const QString &language);
    QString language() const { return m_language; }
    bool isActive() const { return m_hunspell != nullptr; }

    // True for correct words, and whenever checking is off.
    bool isCorrect(const QString &word) const;
    QStringList suggestions(const QString &word, int limit = 6) const;
    // Remembers `word` as correct from now on (personal.txt). Returns false if it can't be saved.
    bool addWord(const QString &word);

private:
    static QString canonical(const QString &word);
    static QStringList readPersonal();
    QByteArray encode(const QString &word) const;
    QString decode(const std::string &word) const;
    bool representable(const QString &word) const;

    std::unique_ptr<Hunspell> m_hunspell;
    QString m_language;
    QStringConverter::Encoding m_encoding = QStringConverter::Utf8;
    QStringList m_personal;
    mutable QHash<QString, bool> m_cache;
};
