#include "KesStyle.h"
#include "MainWindow.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>

int main(int argc, char** argv)
{
    // Qt's native Windows file dialogs keep the main thread blocked for seconds after closing.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    QApplication::setApplicationName("kesLocalizationEditor");
    QApplication::setOrganizationName("KES Engine");
    QApplication::setWindowIcon(QIcon(":/icons/icon.png"));
    kes::style::apply(app);

    kle::MainWindow w;
    w.show();
    const QStringList args = app.arguments();
    const QStringList recent = QSettings().value(QStringLiteral("recentProjects")).toStringList();
    if (args.size() > 1)
        w.openProject(QDir::fromNativeSeparators(args.at(1)));
    else if (!recent.isEmpty() && QFileInfo::exists(recent.first()))
        w.openProject(recent.first());
    return app.exec();
}
