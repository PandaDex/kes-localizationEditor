#pragma once
#include "KeyScanner.h"
#include "LocProject.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;

namespace kle {

class MissingKeysPanel : public QWidget
{
    Q_OBJECT
public:
    explicit MissingKeysPanel(QWidget* parent = nullptr);

    void refresh(const LocProject* project, const ScanResult* scan, const QString& projectDir);
    int missingCount() const { return missing_.size(); }

signals:
    void addRequested(const QStringList& keys);
    void rescanRequested();

private:
    QStringList selectedKeys() const;

    QTreeWidget* tree_;
    QLabel* summary_;
    QPushButton* addSelected_;
    QPushButton* addAll_;
    QStringList missing_;
    QString projectDir_;
};

} // namespace kle
