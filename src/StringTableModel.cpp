#include "StringTableModel.h"
#include "KesStyle.h"
#include "Languages.h"

#include <QColor>
#include <QFont>

namespace kle {

namespace {


QColor tint(const QColor& c, int alpha)
{
    QColor t = c;
    t.setAlpha(alpha);
    return t;
}

QString statusText(CellStatus s, bool isSource)
{
    const QString fallback = isSource ? QStringLiteral("the game shows #Key") : QStringLiteral("the game falls back to the source language");
    switch (s) {
    case CellStatus::Translated: return QStringLiteral("Translated");
    case CellStatus::Fuzzy: return QStringLiteral("Fuzzy, needs review: ") + fallback;
    case CellStatus::Untranslated: return QStringLiteral("Untranslated: ") + fallback;
    case CellStatus::Missing: return QStringLiteral("Not in this language's file: ") + fallback;
    }
    return {};
}

} // namespace

StringTableModel::StringTableModel(QObject* parent) : QAbstractTableModel(parent) {}

void StringTableModel::setProject(const LocProject* project)
{
    beginResetModel();
    project_ = project;
    keys_ = project_ ? project_->keys() : QStringList();
    languages_ = project_ ? project_->languageCodes() : QStringList();
    updateStats();
    endResetModel();
}

void StringTableModel::updateStats()
{
    translated_.clear();
    if (!project_) return;
    for (const QString& code : languages_) {
        int count = 0;
        for (const QString& key : keys_)
            if (project_->status(code, key) == CellStatus::Translated) ++count;
        translated_.insert(code, count);
    }
}

void StringTableModel::setScan(const ScanResult* scan)
{
    scan_ = scan;
    if (!keys_.isEmpty()) emit dataChanged(index(0, UsesColumn), index(keys_.size() - 1, UsesColumn));
}

void StringTableModel::projectChanged()
{
    const QStringList keys = project_ ? project_->keys() : QStringList();
    const QStringList languages = project_ ? project_->languageCodes() : QStringList();
    if (keys != keys_ || languages != languages_) {
        beginResetModel();
        keys_ = keys;
        languages_ = languages;
        updateStats();
        endResetModel();
        return;
    }
    updateStats();
    if (!keys_.isEmpty()) emit dataChanged(index(0, 0), index(keys_.size() - 1, columnCount() - 1));
    if (!languages_.isEmpty()) emit headerDataChanged(Qt::Horizontal, FirstLanguageColumn, columnCount() - 1);
}

QString StringTableModel::languageAt(int column) const
{
    const int i = column - FirstLanguageColumn;
    return i >= 0 && i < languages_.size() ? languages_[i] : QString();
}

int StringTableModel::columnOf(const QString& code) const
{
    const int i = languages_.indexOf(code);
    return i < 0 ? -1 : FirstLanguageColumn + i;
}

int StringTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : keys_.size();
}

int StringTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : FirstLanguageColumn + languages_.size();
}

QVariant StringTableModel::data(const QModelIndex& index, int role) const
{
    if (!project_ || !index.isValid()) return {};
    const QString key = keys_[index.row()];
    const int column = index.column();

    if (role == KeyRole) return key;

    if (column == KeyColumn) {
        if (role == Qt::DisplayRole || role == Qt::EditRole) return key;
        if (role == Qt::ToolTipRole) return QStringLiteral("#%1").arg(key);
        return {};
    }

    if (column == UsesColumn) {
        const int uses = scan_ ? scan_->references.value(key).size() : 0;
        if (role == Qt::DisplayRole) return scan_ ? QVariant(uses) : QVariant();
        if (role == Qt::EditRole) return uses;
        if (role == Qt::ForegroundRole && scan_ && uses == 0) return kes::style::warning();
        if (role == Qt::ToolTipRole && scan_) {
            if (uses == 0) return QStringLiteral("Not used by any scanned file");
            QStringList lines;
            for (const KeyReference& r : scan_->references.value(key).mid(0, 15))
                lines << QStringLiteral("%1:%2").arg(r.file).arg(r.line);
            if (uses > 15) lines << QStringLiteral("… and %1 more").arg(uses - 15);
            return lines.join(QLatin1Char('\n'));
        }
        if (role == Qt::TextAlignmentRole) return int(Qt::AlignCenter);
        return {};
    }

    const QString code = languageAt(column);
    const PoEntry* e = project_->entry(code, key);
    const CellStatus status = project_->status(code, key);
    if (role == StatusRole) return int(status);
    if (role == LanguageRole) return code;

    const QStringList forms = e ? e->translations : QStringList();
    const PoEntry* source = project_->sourceEntry(key);
    const bool plural = source && source->plural;

    switch (role) {
    case Qt::DisplayRole: {
        QStringList shown;
        for (const QString& f : forms) shown << QString(f).replace(QLatin1Char('\n'), QStringLiteral(" ⏎ "));
        return plural ? shown.join(QStringLiteral("  │  ")) : shown.value(0);
    }
    case Qt::EditRole:
        return forms.value(0);
    case Qt::BackgroundRole:
        switch (status) {
        case CellStatus::Translated: return {};
        case CellStatus::Fuzzy: return tint(kes::style::warning(), 45);
        case CellStatus::Untranslated:
        case CellStatus::Missing: return tint(kes::style::error(), 40);
        }
        return {};
    case Qt::ToolTipRole: {
        QString tip = QStringLiteral("<b>%1</b> · %2").arg(languageName(code).toHtmlEscaped(),
                                                            statusText(status, code == project_->sourceLanguage()));
        if (plural) tip += QStringLiteral("<br>Plural entry: edit it in the Details panel.");
        return tip;
    }
    case Qt::FontRole:
        if (plural) {
            QFont f;
            f.setItalic(true);
            return f;
        }
        return {};
    default:
        return {};
    }
}

QVariant StringTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal) return {};
    if (role == Qt::DisplayRole) {
        if (section == KeyColumn) return QStringLiteral("Key");
        if (section == UsesColumn) return QStringLiteral("Uses");
        const QString code = languageAt(section);
        if (project_ && code == project_->sourceLanguage())
            return QStringLiteral("%1 (%2) · source").arg(languageName(code), code);
        if (project_ && !keys_.isEmpty()) {
            const int percent = translated_.value(code) * 100 / int(keys_.size());
            return QStringLiteral("%1 (%2) · %3%").arg(languageName(code), code).arg(percent);
        }
        return QStringLiteral("%1 (%2)").arg(languageName(code), code);
    }
    if (role == Qt::ToolTipRole && section == UsesColumn)
        return QStringLiteral("References found by the last scan (F5)");
    return {};
}

Qt::ItemFlags StringTableModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags f = QAbstractTableModel::flags(index);
    if (!project_ || index.column() < FirstLanguageColumn) return f;
    // Plural and multi-line text is edited in the Details panel; a line edit would lose the newlines.
    const PoEntry* source = project_->sourceEntry(keyAt(index.row()));
    if (source && !source->plural && !index.data(Qt::EditRole).toString().contains(QLatin1Char('\n')))
        f |= Qt::ItemIsEditable;
    return f;
}

bool StringTableModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!project_ || role != Qt::EditRole || index.column() < FirstLanguageColumn) return false;
    const QString key = keyAt(index.row());
    const QString code = languageAt(index.column());
    const QString text = value.toString();
    if (data(index, Qt::EditRole).toString() == text && project_->entry(code, key)) return false;
    emit translationEdited(code, key, text);
    return true;
}

void StringFilterModel::setMode(Mode mode)
{
    beginChange();
    mode_ = mode;
    endChange();
}

void StringFilterModel::setText(const QString& text)
{
    beginChange();
    text_ = text.trimmed();
    endChange();
}

void StringFilterModel::setVisibleLanguages(const QStringList& codes)
{
    beginChange();
    visible_ = codes;
    endChange();
}

void StringFilterModel::setScanned(bool scanned)
{
    beginChange();
    scanned_ = scanned;
    endChange();
}

// invalidateFilter() is deprecated from Qt 6.10 on
void StringFilterModel::beginChange()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
#endif
}

void StringFilterModel::endChange()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    invalidateFilter();
#endif
}

bool StringFilterModel::filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const
{
    auto* model = static_cast<const StringTableModel*>(sourceModel());
    const QModelIndex keyIndex = model->index(sourceRow, StringTableModel::KeyColumn, sourceParent);

    auto statusOf = [&](const QString& code) {
        const int column = model->columnOf(code);
        return column < 0 ? CellStatus::Translated
                          : CellStatus(model->index(sourceRow, column, sourceParent).data(StringTableModel::StatusRole).toInt());
    };

    bool keep = true;
    switch (mode_) {
    case Mode::All:
        break;
    case Mode::NeedsWork:
    case Mode::Untranslated:
    case Mode::Fuzzy:
        keep = false;
        for (const QString& code : visible_) {
            const CellStatus s = statusOf(code);
            if ((mode_ != Mode::Fuzzy && (s == CellStatus::Untranslated || s == CellStatus::Missing)) ||
                (mode_ != Mode::Untranslated && s == CellStatus::Fuzzy)) {
                keep = true;
                break;
            }
        }
        break;
    case Mode::Unused:
        keep = scanned_ && model->index(sourceRow, StringTableModel::UsesColumn, sourceParent).data(Qt::EditRole).toInt() == 0;
        break;
    }
    if (!keep) return false;
    if (text_.isEmpty()) return true;

    if (keyIndex.data().toString().contains(text_, Qt::CaseInsensitive)) return true;
    for (const QString& code : visible_) {
        const int column = model->columnOf(code);
        if (column >= 0 && model->index(sourceRow, column, sourceParent).data().toString().contains(text_, Qt::CaseInsensitive))
            return true;
    }
    return false;
}

} // namespace kle
