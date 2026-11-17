#pragma once
#include "Dialogs.h"
#include "KesProject.h"
#include "KeyScanner.h"
#include "LocProject.h"

#include <QMainWindow>

#include <functional>

class QComboBox;
class QLabel;
class QLineEdit;
class QMenu;
class QTableView;
class QToolButton;
class QUndoStack;

namespace kle {

class DetailsPanel;
class MissingKeysPanel;
class StringFilterModel;
class StringTableModel;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    bool openProject(const QString& path);
    void setProjectState(const LocProject& state);

protected:
    void closeEvent(QCloseEvent* e) override;
    void dragEnterEvent(QDragEnterEvent* e) override;
    void dropEvent(QDropEvent* e) override;

private:
    void buildActions();
    void buildMenusAndToolbar();
    void buildTable();

    // Edits with the same mergeKey in a row become one undo step, so typing isn't undone per letter.
    void applyEdit(const QString& text, const QString& mergeKey, const std::function<void(LocProject&)>& edit);
    void projectChanged();
    void rescan();

    void updateTitle();
    void updateStatus();
    void updateLanguageMenu();
    void applyColumnLayout();
    void setVisibleLanguages(const QStringList& codes);
    void readVisibleFromHeader();

    void loadProjectSettings();
    void saveProjectSettings() const;
    QString settingsGroup() const;
    void addRecent(const QString& path);
    void updateRecentMenu();

    bool maybeSave();
    bool save();
    void open();
    void reload();

    void addKey(const QString& prefill = {});
    void addKeys(const QStringList& keys);
    void deleteSelectedKeys();
    void addLanguage();
    void editProjectSettings();
    void nextNeedsWork();

    QString currentKey() const;
    void selectKey(const QString& key);

    ProjectPaths paths_;
    ProjectSettings settings_;
    LocProject project_;
    ScanResult scan_;
    bool scanned_ = false;
    bool loaded_ = false;
    QStringList visible_;      // in column order
    QStringList knownLanguages_;
    bool applyingLayout_ = false;
    int mergeSession_ = 0;

    QUndoStack* undo_;
    StringTableModel* model_;
    StringFilterModel* filter_;
    QTableView* table_;
    DetailsPanel* details_;
    MissingKeysPanel* missing_;
    QLineEdit* search_;
    QComboBox* filterMode_;
    QToolButton* languagesButton_;
    QMenu* languagesMenu_;
    QMenu* recentMenu_ = nullptr;
    QLabel* statusLabel_;
    QLabel* progressLabel_;

    QAction* actOpen_;
    QAction* actSave_;
    QAction* actReload_;
    QAction* actQuit_;
    QAction* actUndo_;
    QAction* actRedo_;
    QAction* actAddKey_;
    QAction* actDeleteKey_;
    QAction* actAddLanguage_;
    QAction* actRescan_;
    QAction* actSettings_;
    QAction* actFind_;
    QAction* actNext_;
};

} // namespace kle
