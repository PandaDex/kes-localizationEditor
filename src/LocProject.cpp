#include "LocProject.h"
#include "Languages.h"

#include "po_catalog.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#include <algorithm>

namespace kle {

namespace {

QString revisionDate()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mmt"));
}

PoFile newFile(const QString& code)
{
    PoFile po;
    po.hasHeader = true;
    po.setHeaderValue(QStringLiteral("Language"), code);
    po.setHeaderValue(QStringLiteral("MIME-Version"), QStringLiteral("1.0"));
    po.setHeaderValue(QStringLiteral("Content-Type"), QStringLiteral("text/plain; charset=UTF-8"));
    po.setHeaderValue(QStringLiteral("Content-Transfer-Encoding"), QStringLiteral("8bit"));
    po.setHeaderValue(QStringLiteral("Plural-Forms"), pluralFormsFor(code));
    po.setHeaderValue(QStringLiteral("X-Generator"), QStringLiteral("KES Localization Editor"));
    return po;
}

QString readText(const QString& path, bool* exists = nullptr)
{
    QFile f(path);
    if (exists) *exists = f.exists();
    if (!f.open(QIODevice::ReadOnly)) return {};
    return QString::fromUtf8(f.readAll());
}

} // namespace

bool LocProject::isValidKey(const QString& key)
{
    static const QRegularExpression re(QStringLiteral(R"(^[A-Za-z][A-Za-z0-9_.\-]*$)"));
    return re.match(key).hasMatch();
}

QString LocProject::stripKey(const QString& key)
{
    QString k = key.trimmed();
    if (k.startsWith(QLatin1Char('#'))) k.remove(0, 1);
    return k;
}

bool LocProject::load(const QString& directory, const QString& sourceLanguage, QStringList* errors)
{
    directory_ = QDir::cleanPath(directory);
    sourceLanguage_ = normalizeLanguageCode(sourceLanguage);
    if (sourceLanguage_.isEmpty()) sourceLanguage_ = QStringLiteral("en");
    languages_.clear();

    bool ok = true;
    const QDir dir(directory_);
    for (const QFileInfo& info : dir.entryInfoList({QStringLiteral("*.po")}, QDir::Files, QDir::Name)) {
        const QString code = normalizeLanguageCode(info.completeBaseName());
        if (code.isEmpty()) {
            if (errors) *errors << QStringLiteral("%1: the file name is not a language code").arg(info.fileName());
            continue;
        }
        LanguageFile lang;
        lang.code = code;
        lang.path = info.absoluteFilePath();
        QString error;
        if (!parsePo(readText(lang.path), lang.po, &error)) {
            if (errors) *errors << QStringLiteral("%1: %2").arg(info.fileName(), error);
            ok = false;
            continue;
        }
        languages_.push_back(std::move(lang));
    }

    if (languageIndex(sourceLanguage_) < 0) {
        LanguageFile source;
        source.code = sourceLanguage_;
        source.path = dir.absoluteFilePath(sourceLanguage_ + QStringLiteral(".po"));
        source.po = newFile(sourceLanguage_);
        languages_.push_back(std::move(source));
    }
    std::stable_sort(languages_.begin(), languages_.end(), [this](const LanguageFile& a, const LanguageFile& b) {
        if ((a.code == sourceLanguage_) != (b.code == sourceLanguage_)) return a.code == sourceLanguage_;
        return a.code < b.code;
    });
    return ok;
}

int LocProject::languageIndex(const QString& code) const
{
    for (int i = 0; i < languages_.size(); ++i)
        if (languages_[i].code == code) return i;
    return -1;
}

QStringList LocProject::languageCodes() const
{
    QStringList codes;
    for (const LanguageFile& l : languages_) codes << l.code;
    return codes;
}

QStringList LocProject::keys() const
{
    QSet<QString> set;
    for (const LanguageFile& l : languages_)
        for (const PoEntry& e : l.po.entries)
            if (!e.isPassthrough()) set.insert(e.key);
    QStringList list(set.begin(), set.end());
    std::sort(list.begin(), list.end(), [](const QString& a, const QString& b) {
        const int c = a.compare(b, Qt::CaseInsensitive);
        return c != 0 ? c < 0 : a < b;
    });
    return list;
}

bool LocProject::hasKey(const QString& key) const
{
    return sourceEntry(stripKey(key)) != nullptr;
}

const PoEntry* LocProject::entry(const QString& code, const QString& key) const
{
    const int index = languageIndex(code);
    if (index < 0) return nullptr;
    const PoFile& po = languages_[index].po;
    const int e = po.findEntry(key);
    return e >= 0 ? &po.entries[e] : nullptr;
}

const PoEntry* LocProject::sourceEntry(const QString& key) const
{
    if (const PoEntry* e = entry(sourceLanguage_, key)) return e;
    for (const LanguageFile& l : languages_) {
        const int e = l.po.findEntry(key);
        if (e >= 0) return &l.po.entries[e];
    }
    return nullptr;
}

CellStatus LocProject::status(const QString& code, const QString& key) const
{
    const PoEntry* e = entry(code, key);
    if (!e) return CellStatus::Missing;
    if (e->isTranslated()) return CellStatus::Translated;
    if (e->isFuzzy()) {
        for (const QString& t : e->translations)
            if (!t.isEmpty()) return CellStatus::Fuzzy;
    }
    return CellStatus::Untranslated;
}

LanguageFile* LocProject::file(const QString& code)
{
    const int index = languageIndex(code);
    return index >= 0 ? &languages_[index] : nullptr;
}

PoEntry& LocProject::ensureEntry(LanguageFile& lang, const QString& key)
{
    const int index = lang.po.findEntry(key);
    if (index >= 0) return lang.po.entries[index];
    PoEntry e;
    e.key = key;
    if (const PoEntry* source = sourceEntry(key)) {
        e.plural = source->plural;
        e.extractedComments = source->extractedComments;
    }
    const int forms = e.plural ? lang.po.pluralCount() : 1;
    for (int i = 0; i < forms; ++i) e.translations << QString();
    lang.po.entries.push_back(e);
    return lang.po.entries.back();
}

bool LocProject::addKey(const QString& rawKey, const QString& text, const QString& pluralText)
{
    const QString key = stripKey(rawKey);
    if (!isValidKey(key) || hasKey(key)) return false;
    LanguageFile* src = file(sourceLanguage_);
    if (!src) return false;
    PoEntry e;
    e.key = key;
    e.plural = !pluralText.isEmpty();
    e.translations << text;
    if (e.plural) {
        e.translations << pluralText;
        while (e.translations.size() < src->po.pluralCount()) e.translations << QString();
    }
    src->po.entries.push_back(e);
    return true;
}

void LocProject::removeKey(const QString& key)
{
    for (LanguageFile& l : languages_) {
        const int index = l.po.findEntry(key);
        if (index >= 0) {
            l.po.entries.removeAt(index);
            l.po.invalidateIndex();
        }
    }
}

bool LocProject::renameKey(const QString& from, const QString& rawTo)
{
    const QString to = stripKey(rawTo);
    if (!isValidKey(to) || from == to || hasKey(to) || !hasKey(from)) return false;
    for (LanguageFile& l : languages_) {
        const int index = l.po.findEntry(from);
        if (index >= 0) {
            l.po.entries[index].key = to;
            l.po.invalidateIndex();
        }
    }
    return true;
}

void LocProject::setPlural(const QString& key, bool plural)
{
    for (LanguageFile& l : languages_) {
        const int index = l.po.findEntry(key);
        if (index < 0) continue;
        PoEntry& e = l.po.entries[index];
        e.plural = plural;
        const int forms = plural ? l.po.pluralCount() : 1;
        while (e.translations.size() > forms) e.translations.removeLast();
        while (e.translations.size() < forms) e.translations << QString();
    }
}

void LocProject::setTranslation(const QString& code, const QString& key, const QStringList& forms)
{
    LanguageFile* l = file(code);
    if (!l || !sourceEntry(key)) return;
    PoEntry& e = ensureEntry(*l, key);
    const bool changed = e.translations != forms;
    e.translations = forms;
    const int wanted = e.plural ? std::max(l->po.pluralCount(), 1) : 1;
    while (e.translations.size() < wanted) e.translations << QString();
    if (!changed || code != sourceLanguage_) return;

    // New source text: translations made from the old one need review.
    for (LanguageFile& other : languages_) {
        if (other.code == sourceLanguage_) continue;
        const int index = other.po.findEntry(key);
        if (index < 0) continue;
        PoEntry& t = other.po.entries[index];
        for (const QString& form : t.translations) {
            if (!form.isEmpty()) {
                t.setFuzzy(true);
                break;
            }
        }
    }
}

void LocProject::setFuzzy(const QString& code, const QString& key, bool fuzzy)
{
    LanguageFile* l = file(code);
    if (!l || !sourceEntry(key)) return;
    ensureEntry(*l, key).setFuzzy(fuzzy);
}

void LocProject::setNote(const QString& key, const QString& note)
{
    const QStringList lines = note.isEmpty() ? QStringList() : note.split(QLatin1Char('\n'));
    for (LanguageFile& l : languages_) {
        const int index = l.po.findEntry(key);
        if (index >= 0) l.po.entries[index].extractedComments = lines;
    }
}

bool LocProject::addLanguage(const QString& rawCode, QString* error)
{
    const QString code = normalizeLanguageCode(rawCode);
    if (code.isEmpty()) {
        if (error) *error = QStringLiteral("\"%1\" is not a language code (use e.g. pl, de, pt_BR).").arg(rawCode);
        return false;
    }
    if (languageIndex(code) >= 0) {
        if (error) *error = QStringLiteral("%1 already exists.").arg(code);
        return false;
    }
    LanguageFile lang;
    lang.code = code;
    lang.path = QDir(directory_).absoluteFilePath(code + QStringLiteral(".po"));
    lang.po = newFile(code);
    languages_.push_back(lang);
    LanguageFile& added = languages_.back();
    for (const QString& key : keys()) ensureEntry(added, key);
    std::stable_sort(languages_.begin() + 1, languages_.end(),
                     [](const LanguageFile& a, const LanguageFile& b) { return a.code < b.code; });
    return true;
}

int LocProject::translatedCount(const QString& code) const
{
    int count = 0;
    for (const QString& key : keys())
        if (status(code, key) == CellStatus::Translated) ++count;
    return count;
}

QString LocProject::serialize(int languageIndex) const
{
    return writePo(languages_[languageIndex].po);
}

QStringList LocProject::save(QString* error)
{
    QStringList written;
    if (error) error->clear();
    QDir().mkpath(directory_);
    for (int i = 0; i < languages_.size(); ++i) {
        LanguageFile& lang = languages_[i];
        bool exists = false;
        const QString onDisk = readText(lang.path, &exists);
        QString text = serialize(i);
        if (exists && onDisk == text) continue;
        if (!exists && lang.po.entries.isEmpty()) continue;   // untouched new source file

        lang.po.setHeaderValue(QStringLiteral("PO-Revision-Date"), revisionDate());
        text = writePo(lang.po);

        PoCatalog check;
        std::string parseError;
        if (!check.Parse(text.toStdString(), &parseError)) {
            if (error)
                *error = QStringLiteral("%1 would not load in the game (%2); nothing was written for it.")
                             .arg(QFileInfo(lang.path).fileName(), QString::fromStdString(parseError));
            return written;
        }

        QSaveFile out(lang.path);
        if (!out.open(QIODevice::WriteOnly) || out.write(text.toUtf8()) < 0 || !out.commit()) {
            if (error) *error = QStringLiteral("Could not write %1: %2").arg(lang.path, out.errorString());
            return written;
        }
        written << lang.path;
    }
    return written;
}

} // namespace kle
