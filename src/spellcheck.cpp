#include "spellcheck.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTextStream>

#include <hunspell.hxx>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace {

QStringList g_searchPaths;

// The file names a dictionary for `language` can have, preferred first.
QStringList fileNames(const QString &language)
{
    static const QHash<QString, QStringList> names{
        {QStringLiteral("en"), {QStringLiteral("en_US"), QStringLiteral("en_GB")}},
        {QStringLiteral("es"), {QStringLiteral("es_ES"), QStringLiteral("es")}},
        {QStringLiteral("pt"), {QStringLiteral("pt_BR"), QStringLiteral("pt_PT")}},
        {QStringLiteral("fr"), {QStringLiteral("fr"), QStringLiteral("fr_FR")}},
        {QStringLiteral("de"), {QStringLiteral("de_DE_frami"), QStringLiteral("de_DE")}},
    };
    return names.value(language);
}

// A path Hunspell can open. It uses plain fopen(), which on Windows takes the local code page,
// so a folder name outside it (a Chinese user name on a Spanish Windows, say) goes by its short
// 8.3 name instead.
QByteArray hunspellPath(const QString &path)
{
#ifdef Q_OS_WIN
    const QString native = QDir::toNativeSeparators(path);
    const QByteArray local = native.toLocal8Bit();
    if (QString::fromLocal8Bit(local) == native)
        return local;
    wchar_t shortPath[MAX_PATH];
    const DWORD length = GetShortPathNameW(reinterpret_cast<LPCWSTR>(native.utf16()), shortPath, MAX_PATH);
    if (length > 0 && length < MAX_PATH)
        return QString::fromWCharArray(shortPath, int(length)).toLocal8Bit();
    return local;
#else
    return QFile::encodeName(path);
#endif
}

} // namespace

SpellChecker::SpellChecker()
{
    QFile file(QDir(userFolder()).filePath(QStringLiteral("personal.txt")));
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        in.setEncoding(QStringConverter::Utf8);
        while (!in.atEnd()) {
            const QString word = in.readLine().trimmed();
            if (!word.isEmpty())
                m_personal.append(word);
        }
    }
}

SpellChecker::~SpellChecker() = default;

QStringList SpellChecker::languages()
{
    return {QStringLiteral("en"), QStringLiteral("es"), QStringLiteral("pt"), QStringLiteral("fr"),
            QStringLiteral("de")};
}

QString SpellChecker::userFolder()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
        .filePath(QStringLiteral("dictionaries"));
}

QStringList SpellChecker::searchPaths()
{
    if (!g_searchPaths.isEmpty())
        return g_searchPaths;
    const QString app = QCoreApplication::applicationDirPath();
    return {
        userFolder(),
        QDir(app).filePath(QStringLiteral("dictionaries")),
        // macOS: light_md.app/Contents/Resources. Linux: <prefix>/share/light_md.
        QDir(app).filePath(QStringLiteral("../Resources/dictionaries")),
        QDir(app).filePath(QStringLiteral("../share/light_md/dictionaries")),
        QStringLiteral("/usr/share/hunspell"),
        QStringLiteral("/usr/share/myspell"),
        QStringLiteral("/usr/share/myspell/dicts"),
    };
}

void SpellChecker::setSearchPaths(const QStringList &paths)
{
    g_searchPaths = paths;
}

bool SpellChecker::setLanguage(const QString &language)
{
    m_hunspell.reset();
    m_cache.clear();
    m_language.clear();
    for (const QString &folder : searchPaths()) {
        for (const QString &name : fileNames(language)) {
            const QString aff = QDir(folder).filePath(name + QStringLiteral(".aff"));
            const QString dic = QDir(folder).filePath(name + QStringLiteral(".dic"));
            if (!QFileInfo::exists(aff) || !QFileInfo::exists(dic))
                continue;
            m_hunspell = std::make_unique<Hunspell>(hunspellPath(aff).constData(), hunspellPath(dic).constData());
            const QString encoding = QString::fromStdString(m_hunspell->get_dict_encoding()).toUpper();
            // UTF-8 or Latin-1 ("ISO8859-1", the German one); other 8-bit sets are rare nowadays.
            m_encoding = encoding.endsWith(QStringLiteral("8859-1")) ? QStringConverter::Latin1
                                                                    : QStringConverter::Utf8;
            for (const QString &word : std::as_const(m_personal))
                m_hunspell->add(encode(word).toStdString());
            m_language = language;
            return true;
        }
    }
    return false;
}

bool SpellChecker::representable(const QString &word) const
{
    if (m_encoding != QStringConverter::Latin1)
        return true;
    for (const QChar c : word) {
        if (c.unicode() > 0xFF)
            return false;
    }
    return true;
}

QByteArray SpellChecker::encode(const QString &word) const
{
    return m_encoding == QStringConverter::Latin1 ? word.toLatin1() : word.toUtf8();
}

QString SpellChecker::decode(const std::string &word) const
{
    const QByteArray bytes = QByteArray::fromStdString(word);
    return m_encoding == QStringConverter::Latin1 ? QString::fromLatin1(bytes) : QString::fromUtf8(bytes);
}

bool SpellChecker::isCorrect(const QString &word) const
{
    if (!m_hunspell || word.isEmpty())
        return true;
    if (const auto cached = m_cache.constFind(word); cached != m_cache.constEnd())
        return *cached;
    // Typographic apostrophes (l’eau, don’t) are spelled with ' in the dictionaries.
    QString plain = word;
    plain.replace(QChar(0x2019), QLatin1Char('\''));
    // A word the dictionary's character set can't even write is not ours to judge.
    const bool correct = !representable(plain) || m_hunspell->spell(encode(plain).toStdString());
    m_cache.insert(word, correct);
    return correct;
}

QStringList SpellChecker::suggestions(const QString &word, int limit) const
{
    QStringList result;
    if (!m_hunspell)
        return result;
    QString plain = word;
    plain.replace(QChar(0x2019), QLatin1Char('\''));
    if (!representable(plain))
        return result;
    for (const std::string &suggestion : m_hunspell->suggest(encode(plain).toStdString())) {
        result.append(decode(suggestion));
        if (result.size() >= limit)
            break;
    }
    return result;
}

bool SpellChecker::addWord(const QString &word)
{
    const QString clean = word.trimmed();
    if (clean.isEmpty() || m_personal.contains(clean))
        return true;
    if (!QDir().mkpath(userFolder()))
        return false;
    // Rewritten whole, so a failed write never leaves half a list.
    QSaveFile file(QDir(userFolder()).filePath(QStringLiteral("personal.txt")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
        return false;
    const QByteArray bytes = (QStringList(m_personal) << clean).join(QLatin1Char('\n')).toUtf8() + '\n';
    if (file.write(bytes) != bytes.size() || !file.commit())
        return false;
    m_personal.append(clean);
    if (m_hunspell && representable(clean))
        m_hunspell->add(encode(clean).toStdString());
    m_cache.insert(clean, true);
    return true;
}
