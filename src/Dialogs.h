#pragma once
#include <QDialog>
#include <QStringList>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QDialogButtonBox;

namespace kle {

class LocProject;

class AddKeyDialog : public QDialog
{
    Q_OBJECT
public:
    AddKeyDialog(const LocProject& project, const QString& key, QWidget* parent = nullptr);

    QString key() const;
    QString sourceText() const;
    QString sourcePlural() const;

private:
    void validate();

    const LocProject& project_;
    QLineEdit* key_;
    QPlainTextEdit* text_;
    QCheckBox* plural_;
    QPlainTextEdit* pluralText_;
    QLabel* error_;
    QDialogButtonBox* buttons_;
};

class AddLanguageDialog : public QDialog
{
    Q_OBJECT
public:
    AddLanguageDialog(const QStringList& existing, QWidget* parent = nullptr);
    QString code() const;

private:
    void validate();

    QStringList existing_;
    QComboBox* code_;
    QLabel* info_;
    QDialogButtonBox* buttons_;
};

struct ProjectSettings
{
    QString sourceLanguage = QStringLiteral("en");
    QStringList extraFolders;
    QStringList cppFunctions;
};

class ProjectSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    ProjectSettingsDialog(const ProjectSettings& settings, QWidget* parent = nullptr);
    ProjectSettings settings() const;

private:
    QLineEdit* source_;
    QPlainTextEdit* folders_;
    QLineEdit* functions_;
};

} // namespace kle
