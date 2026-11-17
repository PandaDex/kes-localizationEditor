#pragma once
#include "KeyScanner.h"
#include "LocProject.h"

#include <QPointer>
#include <QWidget>

class QCheckBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QVBoxLayout;

namespace kle {

class DetailsPanel : public QWidget
{
    Q_OBJECT
public:
    explicit DetailsPanel(QWidget* parent = nullptr);

    void setProject(const LocProject* project, const ScanResult* scan, const QString& projectDir);
    void setLanguages(const QStringList& visibleCodes);
    void setKey(const QString& key);
    QString key() const { return key_; }
    void refresh();
    void focusLanguage(const QString& code);

signals:
    void renameRequested(const QString& from, const QString& to);
    void noteEdited(const QString& key, const QString& note);
    void pluralToggled(const QString& key, bool plural);
    void translationEdited(const QString& code, const QString& key, const QStringList& forms);
    void fuzzyToggled(const QString& code, const QString& key, bool fuzzy);

private:
    struct LanguageEditor
    {
        QString code;
        QPointer<QCheckBox> fuzzy;
        QPointer<QCheckBox> plural;            // source language only
        QList<QPointer<QPlainTextEdit>> forms;
        QList<QPointer<QLabel>> formLabels;
        QPointer<QLabel> status;
    };

    void rebuild();
    void emitTranslation(const QString& code);
    static void setTextKeepingCursor(QPlainTextEdit* edit, const QString& text);

    const LocProject* project_ = nullptr;
    const ScanResult* scan_ = nullptr;
    QString projectDir_;
    QStringList languages_;
    QString key_;
    bool updating_ = false;

    QLabel* emptyLabel_;
    QWidget* scroll_;
    QWidget* content_;
    QLineEdit* keyEdit_;
    QPlainTextEdit* note_;
    QListWidget* references_;
    QVBoxLayout* languagesLayout_;
    QList<LanguageEditor> editors_;
};

} // namespace kle
