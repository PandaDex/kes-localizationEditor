#include "DetailsPanel.h"
#include "KesStyle.h"
#include "Languages.h"

#include <QCheckBox>
#include <QDesktopServices>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QToolTip>
#include <QUrl>
#include <QVBoxLayout>

namespace kle {

namespace {

QPlainTextEdit* makeTextEdit(QWidget* parent, int lines)
{
    auto* edit = new QPlainTextEdit(parent);
    edit->setTabChangesFocus(true);
    const int line = edit->fontMetrics().lineSpacing();
    edit->setMinimumHeight(line * lines + 12);
    edit->setMaximumHeight(line * (lines + 3) + 12);
    return edit;
}

QString statusLabel(CellStatus s)
{
    const auto colored = [](const QColor& c, const QString& text) {
        return QStringLiteral("<span style=\"color:%1\">%2</span>").arg(c.name(), text);
    };
    switch (s) {
    case CellStatus::Translated: return colored(kes::style::accent(), QStringLiteral("Translated"));
    case CellStatus::Fuzzy: return colored(kes::style::warning(), QStringLiteral("Fuzzy"));
    case CellStatus::Untranslated: return colored(kes::style::error(), QStringLiteral("Untranslated"));
    case CellStatus::Missing: return colored(kes::style::error(), QStringLiteral("Not in file"));
    }
    return {};
}

} // namespace

DetailsPanel::DetailsPanel(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);

    emptyLabel_ = new QLabel(QStringLiteral("Select a key to edit it in every visible language."), this);
    emptyLabel_->setAlignment(Qt::AlignCenter);
    emptyLabel_->setWordWrap(true);
    emptyLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(kes::style::muted().name()));
    outer->addWidget(emptyLabel_);

    auto* scroll = new QScrollArea(this);
    scroll_ = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    content_ = new QWidget(scroll);
    scroll->setWidget(content_);
    outer->addWidget(scroll, 1);

    auto* layout = new QVBoxLayout(content_);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(8);

    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    keyEdit_ = new QLineEdit(content_);
    keyEdit_->setToolTip(QStringLiteral("Used as #Key in code and UI. Press Enter to rename it in every language."));
    form->addRow(QStringLiteral("Key  #"), keyEdit_);
    note_ = makeTextEdit(content_, 2);
    note_->setPlaceholderText(QStringLiteral("Context for translators (where it is shown, length limits...)"));
    form->addRow(QStringLiteral("Note"), note_);
    references_ = new QListWidget(content_);
    references_->setMaximumHeight(110);
    references_->setToolTip(QStringLiteral("Double-click to open the file"));
    form->addRow(QStringLiteral("Used in"), references_);
    layout->addLayout(form);

    languagesLayout_ = new QVBoxLayout;
    languagesLayout_->setSpacing(8);
    layout->addLayout(languagesLayout_);
    layout->addStretch(1);

    connect(keyEdit_, &QLineEdit::editingFinished, this, [this] {
        if (updating_ || key_.isEmpty()) return;
        const QString to = LocProject::stripKey(keyEdit_->text());
        if (to == key_) return;
        if (!LocProject::isValidKey(to) || (project_ && project_->hasKey(to))) {
            QToolTip::showText(keyEdit_->mapToGlobal(QPoint(0, keyEdit_->height())),
                               LocProject::isValidKey(to) ? QStringLiteral("#%1 already exists").arg(to)
                                                          : QStringLiteral("Keys start with a letter and use letters, digits, '.', '_' or '-'"),
                               keyEdit_);
            keyEdit_->setText(key_);
            return;
        }
        emit renameRequested(key_, to);
    });
    connect(note_, &QPlainTextEdit::textChanged, this, [this] {
        if (!updating_ && !key_.isEmpty()) emit noteEdited(key_, note_->toPlainText());
    });
    connect(references_, &QListWidget::itemActivated, this, [this](QListWidgetItem* item) {
        const QString file = item->data(Qt::UserRole).toString();
        if (!file.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(projectDir_).absoluteFilePath(file)));
    });

    setKey({});
}

void DetailsPanel::setProject(const LocProject* project, const ScanResult* scan, const QString& projectDir)
{
    project_ = project;
    scan_ = scan;
    projectDir_ = projectDir;
    key_.clear();
    rebuild();
}

void DetailsPanel::setLanguages(const QStringList& visibleCodes)
{
    if (visibleCodes == languages_) return;
    languages_ = visibleCodes;
    rebuild();
}

void DetailsPanel::setKey(const QString& key)
{
    if (key == key_ && !editors_.isEmpty()) {
        refresh();
        return;
    }
    key_ = key;
    rebuild();
}

void DetailsPanel::rebuild()
{
    for (LanguageEditor& e : editors_)
        for (auto& f : e.forms)
            if (f) f->disconnect(this);
    editors_.clear();
    while (QLayoutItem* item = languagesLayout_->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    const bool hasKey = project_ && !key_.isEmpty() && project_->hasKey(key_);
    emptyLabel_->setVisible(!hasKey);
    scroll_->setVisible(hasKey);
    if (!hasKey) return;

    const PoEntry* source = project_->sourceEntry(key_);
    const bool plural = source && source->plural;
    QStringList codes = languages_;
    // Source text first so it's in view while translating.
    codes.removeAll(project_->sourceLanguage());
    codes.prepend(project_->sourceLanguage());

    for (const QString& code : codes) {
        const int langIndex = project_->languageIndex(code);
        if (langIndex < 0) continue;
        const bool isSource = code == project_->sourceLanguage();

        auto* box = new QGroupBox(isSource ? QStringLiteral("%1 (%2) · source").arg(languageName(code), code)
                                           : QStringLiteral("%1 (%2)").arg(languageName(code), code),
                                  content_);
        auto* boxLayout = new QVBoxLayout(box);
        boxLayout->setSpacing(4);

        LanguageEditor editor;
        editor.code = code;
        auto* header = new QHBoxLayout;
        editor.status = new QLabel(box);
        header->addWidget(editor.status);
        header->addStretch(1);
        if (isSource) {
            editor.plural = new QCheckBox(QStringLiteral("Plural forms"), box);
            editor.plural->setToolTip(QStringLiteral("Use TrPlural(\"#Key\", n): each language gets its own plural forms"));
            header->addWidget(editor.plural);
            connect(editor.plural, &QCheckBox::toggled, this, [this](bool on) {
                if (!updating_) emit pluralToggled(key_, on);
            });
        } else {
            editor.fuzzy = new QCheckBox(QStringLiteral("Fuzzy"), box);
            editor.fuzzy->setToolTip(QStringLiteral("Needs review. The game ignores fuzzy translations and falls back to the source language."));
            header->addWidget(editor.fuzzy);
            connect(editor.fuzzy, &QCheckBox::toggled, this, [this, code](bool on) {
                if (!updating_) emit fuzzyToggled(code, key_, on);
            });
        }
        boxLayout->addLayout(header);

        QStringList labels;
        int count = 1;
        if (plural) {
            const PoFile& po = project_->languages()[langIndex].po;
            count = po.pluralCount();
            labels = pluralFormLabels(po.headerValue(QStringLiteral("Plural-Forms")), count);
        }
        for (int i = 0; i < count; ++i) {
            if (i < labels.size()) {
                auto* label = new QLabel(labels[i], box);
                label->setStyleSheet(QStringLiteral("color: %1;").arg(kes::style::muted().name()));
                boxLayout->addWidget(label);
                editor.formLabels << label;
            }
            QPlainTextEdit* edit = makeTextEdit(box, 2);
            edit->setPlaceholderText(QStringLiteral("Untranslated"));
            boxLayout->addWidget(edit);
            editor.forms << edit;
            connect(edit, &QPlainTextEdit::textChanged, this, [this, code] {
                if (!updating_) emitTranslation(code);
            });
        }
        languagesLayout_->addWidget(box);
        editors_ << editor;
    }
    refresh();
}

void DetailsPanel::refresh()
{
    if (!project_ || key_.isEmpty() || !project_->hasKey(key_)) {
        if (!editors_.isEmpty() || !key_.isEmpty()) {
            key_.clear();
            rebuild();
        }
        return;
    }

    const PoEntry* source = project_->sourceEntry(key_);
    const bool plural = source && source->plural;
    // Plural forms appeared or went away, so the editors need rebuilding.
    for (const LanguageEditor& e : editors_) {
        const int langIndex = project_->languageIndex(e.code);
        const int wanted = plural && langIndex >= 0 ? project_->languages()[langIndex].po.pluralCount() : 1;
        if (e.forms.size() != wanted || langIndex < 0) {
            rebuild();
            return;
        }
    }

    updating_ = true;
    if (!keyEdit_->hasFocus()) keyEdit_->setText(key_);
    setTextKeepingCursor(note_, source ? source->extractedComments.join(QLatin1Char('\n')) : QString());

    references_->clear();
    const QVector<KeyReference> refs = scan_ ? scan_->references.value(key_) : QVector<KeyReference>();
    for (const KeyReference& r : refs) {
        auto* item = new QListWidgetItem(QStringLiteral("%1:%2").arg(r.file).arg(r.line), references_);
        item->setData(Qt::UserRole, r.file);
    }
    if (refs.isEmpty()) {
        auto* item = new QListWidgetItem(scan_ ? QStringLiteral("Not used by any scanned file") : QStringLiteral("Not scanned yet (F5)"),
                                         references_);
        item->setForeground(scan_ ? kes::style::warning() : kes::style::muted());
    }

    for (LanguageEditor& e : editors_) {
        const PoEntry* entry = project_->entry(e.code, key_);
        e.status->setText(statusLabel(project_->status(e.code, key_)));
        if (e.plural) e.plural->setChecked(plural);
        if (e.fuzzy) e.fuzzy->setChecked(entry && entry->isFuzzy());
        for (int i = 0; i < e.forms.size(); ++i) setTextKeepingCursor(e.forms[i], entry ? entry->translations.value(i) : QString());
    }
    updating_ = false;
}

void DetailsPanel::focusLanguage(const QString& code)
{
    for (const LanguageEditor& e : editors_) {
        if (e.code == code && !e.forms.isEmpty() && e.forms.first()) {
            e.forms.first()->setFocus();
            e.forms.first()->moveCursor(QTextCursor::End);
            return;
        }
    }
}

void DetailsPanel::emitTranslation(const QString& code)
{
    for (const LanguageEditor& e : editors_) {
        if (e.code != code) continue;
        QStringList forms;
        for (const auto& f : e.forms) forms << (f ? f->toPlainText() : QString());
        emit translationEdited(code, key_, forms);
        return;
    }
}

void DetailsPanel::setTextKeepingCursor(QPlainTextEdit* edit, const QString& text)
{
    if (!edit || edit->toPlainText() == text) return;
    const bool blocked = edit->blockSignals(true);
    edit->setPlainText(text);
    edit->blockSignals(blocked);
}

} // namespace kle
