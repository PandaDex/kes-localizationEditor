#pragma once
// Undo keeps whole copies of this; Qt containers share data until written, so
// a copy only duplicates the file being edited.
#include "PoFile.h"

#include <QString>
#include <QStringList>
#include <QVector>

namespace kle {

struct LanguageFile
{
    QString code;
    QString path;
    PoFile po;
};

enum class CellStatus
{
    Translated,
    Fuzzy,
    Untranslated,   // entry exists, a form is empty
    Missing,        // no entry for the key in this language
};

class LocProject
{
public:
    // Adds an empty source language file if there is none; it's only written once it has entries.
    bool load(const QString& directory, const QString& sourceLanguage, QStringList* errors = nullptr);

    QString directory() const { return directory_; }
    QString sourceLanguage() const { return sourceLanguage_; }
    const QVector<LanguageFile>& languages() const { return languages_; }
    int languageIndex(const QString& code) const;
    QStringList languageCodes() const;

    QStringList keys() const;
    bool hasKey(const QString& key) const;
    const PoEntry* entry(const QString& code, const QString& key) const;
    const PoEntry* sourceEntry(const QString& key) const;
    CellStatus status(const QString& code, const QString& key) const;

    bool addKey(const QString& key, const QString& text, const QString& pluralText = {});
    void removeKey(const QString& key);
    bool renameKey(const QString& from, const QString& to);
    void setPlural(const QString& key, bool plural);
    // Changing the source language text marks existing translations fuzzy.
    void setTranslation(const QString& code, const QString& key, const QStringList& forms);
    void setFuzzy(const QString& code, const QString& key, bool fuzzy);
    void setNote(const QString& key, const QString& note);
    bool addLanguage(const QString& code, QString* error = nullptr);

    int translatedCount(const QString& code) const;

    QString serialize(int languageIndex) const;
    // Writes only changed files, after checking each with the engine's parser.
    QStringList save(QString* error = nullptr);

    static bool isValidKey(const QString& key);
    static QString stripKey(const QString& key);

private:
    LanguageFile* file(const QString& code);
    PoEntry& ensureEntry(LanguageFile& file, const QString& key);

    QString directory_;
    QString sourceLanguage_ = QStringLiteral("en");
    QVector<LanguageFile> languages_;   // source language first, then sorted
};

} // namespace kle
