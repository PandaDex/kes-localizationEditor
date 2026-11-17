#pragma once
// Keeps what the engine ignores (comments, flags, obsolete entries) so saving
// only touches what was edited.
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace kle {

struct PoEntry
{
    QString key;                     // msgid
    bool plural = false;
    QStringList translations;
    QStringList translatorComments;  // #
    QStringList extractedComments;   // #.
    QStringList references;          // #:
    QStringList flags;               // #,
    QStringList previous;            // #|
    QString raw;                     // obsolete or msgctxt entry, written back as-is

    bool isPassthrough() const { return !raw.isEmpty(); }
    bool isFuzzy() const { return flags.contains(QStringLiteral("fuzzy")); }
    void setFuzzy(bool fuzzy);
    bool isTranslated() const;
};

struct PoHeaderField
{
    QString name;
    QString value;
};

struct PoFile
{
    QStringList headerComments;
    QVector<PoHeaderField> header;
    QStringList headerFlags;
    bool hasHeader = false;
    QVector<PoEntry> entries;

    QString headerValue(const QString& name) const;
    void setHeaderValue(const QString& name, const QString& value);
    int findEntry(const QString& key) const;
    // Needed after removing or renaming entries.
    void invalidateIndex() { indexSize_ = -1; }
    int pluralCount() const;

private:
    mutable QHash<QString, int> index_;
    mutable qsizetype indexSize_ = -1;
};

bool parsePo(const QString& text, PoFile& out, QString* error = nullptr);
QString writePo(const PoFile& file);

QString escapePoString(const QString& text);

} // namespace kle
