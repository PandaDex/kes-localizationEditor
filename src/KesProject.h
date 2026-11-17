#pragma once

#include <QString>

namespace kle {

struct ProjectPaths
{
    QString projectFile;    // empty when there's no .kproject
    QString projectDir;
    QString contentRoot;
    QString localizationDir;

    bool isValid() const { return !localizationDir.isEmpty(); }
};

QString contentRootFromProject(const QString& kprojectPath);

// Walks up from `startPath`; the nearest .kproject wins, then a folder named "content".
ProjectPaths findProject(const QString& startPath);

} // namespace kle
