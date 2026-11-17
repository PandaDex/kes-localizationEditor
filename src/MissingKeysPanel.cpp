#include "MissingKeysPanel.h"
#include "KesStyle.h"

#include <QDesktopServices>
#include <QDir>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

#include <algorithm>

namespace kle {

MissingKeysPanel::MissingKeysPanel(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    auto* bar = new QHBoxLayout;
    summary_ = new QLabel(this);
    bar->addWidget(summary_, 1);
    auto* rescan = new QPushButton(kes::style::icon(QStringLiteral("search")), QStringLiteral("Rescan"), this);
    rescan->setToolTip(QStringLiteral("Scan the project's content and source files again (F5)"));
    addSelected_ = new QPushButton(kes::style::icon(QStringLiteral("plus")), QStringLiteral("Add Selected"), this);
    addAll_ = new QPushButton(QStringLiteral("Add All"), this);
    bar->addWidget(rescan);
    bar->addWidget(addSelected_);
    bar->addWidget(addAll_);
    layout->addLayout(bar);

    tree_ = new QTreeWidget(this);
    tree_->setColumnCount(2);
    tree_->setHeaderLabels({QStringLiteral("Key"), QStringLiteral("Used in")});
    tree_->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tree_->setRootIsDecorated(true);
    tree_->setUniformRowHeights(true);
    tree_->header()->setSectionResizeMode(0, QHeaderView::Interactive);
    tree_->header()->resizeSection(0, 280);
    layout->addWidget(tree_, 1);

    connect(rescan, &QPushButton::clicked, this, &MissingKeysPanel::rescanRequested);
    connect(addSelected_, &QPushButton::clicked, this, [this] {
        const QStringList keys = selectedKeys();
        if (!keys.isEmpty()) emit addRequested(keys);
    });
    connect(addAll_, &QPushButton::clicked, this, [this] {
        if (!missing_.isEmpty()) emit addRequested(missing_);
    });
    connect(tree_, &QTreeWidget::itemSelectionChanged, this, [this] { addSelected_->setEnabled(!selectedKeys().isEmpty()); });
    connect(tree_, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item) {
        const QString file = item->data(1, Qt::UserRole).toString();
        if (!file.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(projectDir_).absoluteFilePath(file)));
    });

    refresh(nullptr, nullptr, {});
}

void MissingKeysPanel::refresh(const LocProject* project, const ScanResult* scan, const QString& projectDir)
{
    projectDir_ = projectDir;
    missing_.clear();
    if (project && scan) {
        for (auto it = scan->references.cbegin(); it != scan->references.cend(); ++it)
            if (!project->hasKey(it.key())) missing_ << it.key();
        std::sort(missing_.begin(), missing_.end(), [](const QString& a, const QString& b) {
            return a.compare(b, Qt::CaseInsensitive) < 0;
        });
    }

    QStringList expanded;
    for (int i = 0; i < tree_->topLevelItemCount(); ++i)
        if (tree_->topLevelItem(i)->isExpanded()) expanded << tree_->topLevelItem(i)->text(0);

    tree_->clear();
    for (const QString& key : missing_) {
        const QVector<KeyReference> refs = scan->references.value(key);
        auto* item = new QTreeWidgetItem(tree_, {QStringLiteral("#%1").arg(key),
                                                 refs.size() == 1 ? QStringLiteral("%1:%2").arg(refs[0].file).arg(refs[0].line)
                                                                  : QStringLiteral("%1 places").arg(refs.size())});
        item->setData(0, Qt::UserRole, key);
        if (refs.size() == 1) item->setData(1, Qt::UserRole, refs[0].file);
        if (refs.size() > 1) {
            for (const KeyReference& r : refs) {
                auto* child = new QTreeWidgetItem(item, {QString(), QStringLiteral("%1:%2").arg(r.file).arg(r.line)});
                child->setData(1, Qt::UserRole, r.file);
                child->setFlags(child->flags() & ~Qt::ItemIsSelectable);
            }
        }
        item->setExpanded(expanded.contains(item->text(0)));
    }

    if (!scan)
        summary_->setText(QStringLiteral("Not scanned yet. Rescan finds #Keys used in content and source files."));
    else if (missing_.isEmpty())
        summary_->setText(QStringLiteral("Every key used in the project is defined (%1 files scanned).").arg(scan->filesScanned));
    else
        summary_->setText(QStringLiteral("<span style=\"color:%1\">%2 keys</span> are used but not in any .po file (%3 files scanned).")
                              .arg(kes::style::warning().name())
                              .arg(missing_.size())
                              .arg(scan->filesScanned));
    addAll_->setEnabled(!missing_.isEmpty());
    addSelected_->setEnabled(!selectedKeys().isEmpty());
}

QStringList MissingKeysPanel::selectedKeys() const
{
    QStringList keys;
    for (QTreeWidgetItem* item : tree_->selectedItems()) {
        const QString key = item->data(0, Qt::UserRole).toString();
        if (!key.isEmpty()) keys << key;
    }
    return keys;
}

} // namespace kle
