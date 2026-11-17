#pragma once
// C++: string literal keys passed to cppFunctions. RML: text nodes that are
// exactly #Key, the same rule the engine uses. Other text files: #Some.Key
// with at least one dot, so CSS colours and ids don't match.
#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>

namespace kle {

struct KeyReference
{
    QString file;   // relative to the scan's base folder
    int line = 0;
};

struct ScanSettings
{
    QString baseFolder;
    QStringList folders;
    QStringList cppFunctions = {QStringLiteral("Tr"), QStringLiteral("TrPlural"), QStringLiteral("LocalizationHasKey")};
    QStringList skipFolderNames = {QStringLiteral(".git"), QStringLiteral(".vs"), QStringLiteral(".vscode"),
                                   QStringLiteral("out"), QStringLiteral("build"), QStringLiteral("bin")};
};

struct ScanResult
{
    QHash<QString, QVector<KeyReference>> references;
    int filesScanned = 0;
};

enum class ScanFileKind
{
    Skip,
    Cpp,
    Rml,
    Text,
};

ScanFileKind scanFileKind(const QString& fileName);
void scanText(const QString& text, ScanFileKind kind, const QString& file, const QStringList& cppFunctions, ScanResult& out);
ScanResult scanForKeys(const ScanSettings& settings);

} // namespace kle
