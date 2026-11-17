#pragma once
#include "KeyScanner.h"
#include "LocProject.h"

#include <QAbstractTableModel>
#include <QSortFilterProxyModel>

namespace kle {

class StringTableModel : public QAbstractTableModel
{
    Q_OBJECT
public:
    enum Column { KeyColumn = 0, UsesColumn = 1, FirstLanguageColumn = 2 };
    enum Role { KeyRole = Qt::UserRole + 1, StatusRole, LanguageRole };

    explicit StringTableModel(QObject* parent = nullptr);

    void setProject(const LocProject* project);
    void setScan(const ScanResult* scan);
    // Resets only when keys or languages changed, so text edits keep the selection and scroll position.
    void projectChanged();

    QString keyAt(int row) const { return row >= 0 && row < keys_.size() ? keys_[row] : QString(); }
    int rowOf(const QString& key) const { return keys_.indexOf(key); }
    QString languageAt(int column) const;
    int columnOf(const QString& code) const;
    int translatedCount(const QString& code) const { return translated_.value(code); }

    int rowCount(const QModelIndex& parent = {}) const override;
    int columnCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool setData(const QModelIndex& index, const QVariant& value, int role) override;

signals:
    // The model never edits the project itself; the window does, through the undo stack.
    void translationEdited(const QString& code, const QString& key, const QString& text);

private:
    void updateStats();

    const LocProject* project_ = nullptr;
    QHash<QString, int> translated_;
    const ScanResult* scan_ = nullptr;
    QStringList keys_;
    QStringList languages_;
};

class StringFilterModel : public QSortFilterProxyModel
{
    Q_OBJECT
public:
    enum class Mode { All, NeedsWork, Untranslated, Fuzzy, Unused };

    using QSortFilterProxyModel::QSortFilterProxyModel;

    void setMode(Mode mode);
    void setText(const QString& text);
    void setVisibleLanguages(const QStringList& codes);
    void setScanned(bool scanned);

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex& sourceParent) const override;

private:
    void beginChange();
    void endChange();

    Mode mode_ = Mode::All;
    QString text_;
    QStringList visible_;
    bool scanned_ = false;
};

} // namespace kle
