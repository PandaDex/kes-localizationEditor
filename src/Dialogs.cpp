#include "Dialogs.h"
#include "KesStyle.h"
#include "Languages.h"
#include "LocProject.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace kle {

namespace {

QLabel* makeErrorLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setStyleSheet(QStringLiteral("color: %1;").arg(kes::style::error().name()));
    label->setWordWrap(true);
    return label;
}

} // namespace

AddKeyDialog::AddKeyDialog(const LocProject& project, const QString& key, QWidget* parent) : QDialog(parent), project_(project)
{
    setWindowTitle(QStringLiteral("Add Key"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    key_ = new QLineEdit(key, this);
    key_->setPlaceholderText(QStringLiteral("MainMenu.NewGame"));
    form->addRow(QStringLiteral("Key  #"), key_);
    text_ = new QPlainTextEdit(this);
    text_->setPlaceholderText(QStringLiteral("Text in %1").arg(languageName(project.sourceLanguage())));
    text_->setTabChangesFocus(true);
    text_->setMaximumHeight(90);
    form->addRow(languageName(project.sourceLanguage()), text_);
    plural_ = new QCheckBox(QStringLiteral("Has plural forms (TrPlural)"), this);
    form->addRow(QString(), plural_);
    pluralText_ = new QPlainTextEdit(this);
    pluralText_->setPlaceholderText(QStringLiteral("{0} apples"));
    pluralText_->setTabChangesFocus(true);
    pluralText_->setMaximumHeight(90);
    pluralText_->setEnabled(false);
    form->addRow(QStringLiteral("Plural"), pluralText_);
    layout->addLayout(form);
    error_ = makeErrorLabel(this);
    layout->addWidget(error_);
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons_);

    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(key_, &QLineEdit::textChanged, this, &AddKeyDialog::validate);
    connect(plural_, &QCheckBox::toggled, this, [this](bool on) {
        pluralText_->setEnabled(on);
        validate();
    });
    connect(pluralText_, &QPlainTextEdit::textChanged, this, &AddKeyDialog::validate);
    resize(460, sizeHint().height());
    validate();
    if (!key.isEmpty()) text_->setFocus();
}

QString AddKeyDialog::key() const { return LocProject::stripKey(key_->text()); }
QString AddKeyDialog::sourceText() const { return text_->toPlainText(); }
QString AddKeyDialog::sourcePlural() const { return plural_->isChecked() ? pluralText_->toPlainText() : QString(); }

void AddKeyDialog::validate()
{
    QString error;
    const QString k = key();
    if (k.isEmpty())
        error = QStringLiteral(" ");
    else if (!LocProject::isValidKey(k))
        error = QStringLiteral("Keys start with a letter and use letters, digits, '.', '_' or '-'.");
    else if (project_.hasKey(k))
        error = QStringLiteral("#%1 already exists.").arg(k);
    else if (plural_->isChecked() && pluralText_->toPlainText().isEmpty())
        error = QStringLiteral("Enter the plural text.");
    error_->setText(error.trimmed());
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(error.isEmpty());
}

AddLanguageDialog::AddLanguageDialog(const QStringList& existing, QWidget* parent) : QDialog(parent), existing_(existing)
{
    setWindowTitle(QStringLiteral("Add Language"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    code_ = new QComboBox(this);
    code_->setEditable(true);
    code_->setInsertPolicy(QComboBox::NoInsert);
    for (const LanguageInfo& l : knownLanguages())
        if (!existing.contains(l.code)) code_->addItem(QStringLiteral("%1 - %2").arg(l.code, l.name), l.code);
    code_->setCurrentIndex(-1);
    code_->setEditText(QString());
    form->addRow(QStringLiteral("Language code"), code_);
    layout->addLayout(form);
    info_ = new QLabel(this);
    info_->setWordWrap(true);
    layout->addWidget(info_);
    buttons_ = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons_);
    connect(buttons_, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons_, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(code_, &QComboBox::editTextChanged, this, &AddLanguageDialog::validate);
    resize(460, sizeHint().height());
    validate();
}

QString AddLanguageDialog::code() const
{
    return normalizeLanguageCode(code_->currentText().section(QLatin1Char(' '), 0, 0));
}

void AddLanguageDialog::validate()
{
    const QString c = code();
    bool ok = false;
    if (c.isEmpty()) {
        info_->setText(QStringLiteral("A gettext code: two letters, optionally a region (pl, de, pt_BR)."));
    } else if (existing_.contains(c)) {
        info_->setText(QStringLiteral("%1 already exists.").arg(c));
    } else {
        ok = true;
        info_->setText(QStringLiteral("Creates localization/%1.po (%2) with every key, untranslated.<br>Plural-Forms: <code>%3</code>")
                           .arg(c, languageName(c).toHtmlEscaped(), pluralFormsFor(c).toHtmlEscaped()));
    }
    buttons_->button(QDialogButtonBox::Ok)->setEnabled(ok);
}

ProjectSettingsDialog::ProjectSettingsDialog(const ProjectSettings& settings, QWidget* parent) : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Project Settings"));
    auto* layout = new QVBoxLayout(this);
    auto* form = new QFormLayout;
    source_ = new QLineEdit(settings.sourceLanguage, this);
    source_->setToolTip(QStringLiteral("Language the msgids are written in; must match LocalizationSetSourceLanguage (default en)"));
    form->addRow(QStringLiteral("Source language"), source_);
    folders_ = new QPlainTextEdit(settings.extraFolders.join(QLatin1Char('\n')), this);
    folders_->setPlaceholderText(QStringLiteral("One folder per line, e.g. engine sources that use game keys"));
    folders_->setMaximumHeight(90);
    auto* browse = new QPushButton(QStringLiteral("Add Folder..."), this);
    auto* folderBox = new QVBoxLayout;
    folderBox->addWidget(folders_);
    folderBox->addWidget(browse, 0, Qt::AlignLeft);
    form->addRow(QStringLiteral("Extra scan folders"), folderBox);
    functions_ = new QLineEdit(settings.cppFunctions.join(QStringLiteral(", ")), this);
    functions_->setToolTip(QStringLiteral("C++ functions whose first argument is a key string literal"));
    form->addRow(QStringLiteral("C++ functions"), functions_);
    layout->addLayout(form);
    auto* note = new QLabel(QStringLiteral("The project folder (where the .kproject is) is always scanned. "
                                           "Folders named .git, .vs, .vscode, out, build and bin are skipped, and so are .po files."),
                            this);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("color: %1;").arg(kes::style::muted().name()));
    layout->addWidget(note);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(browse, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Scan Folder"));
        if (!dir.isEmpty()) folders_->appendPlainText(QDir::toNativeSeparators(dir));
    });
    resize(520, sizeHint().height());
}

ProjectSettings ProjectSettingsDialog::settings() const
{
    ProjectSettings s;
    s.sourceLanguage = normalizeLanguageCode(source_->text());
    if (s.sourceLanguage.isEmpty()) s.sourceLanguage = QStringLiteral("en");
    for (const QString& line : folders_->toPlainText().split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        if (!line.trimmed().isEmpty()) s.extraFolders << QDir::fromNativeSeparators(line.trimmed());
    for (const QString& f : functions_->text().split(QLatin1Char(','), Qt::SkipEmptyParts))
        if (!f.trimmed().isEmpty()) s.cppFunctions << f.trimmed();
    return s;
}

} // namespace kle
