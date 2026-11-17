#include "KeyScanner.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

namespace kle {

namespace {

constexpr qint64 kMaxFileSize = 16 * 1024 * 1024;

const QSet<QString>& cppExtensions()
{
    static const QSet<QString> set = {"c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx", "inl", "ipp"};
    return set;
}

// Checked before reading so big binary assets aren't loaded just to find a NUL.
const QSet<QString>& skippedExtensions()
{
    static const QSet<QString> set = {"po", "pot", "rcss", "css", "png", "jpg", "jpeg", "tga", "bmp", "dds", "ktx",
                                      "ktex", "kmodel", "kanim", "ksound", "pak", "wav", "ogg", "mp3", "flac", "qoa",
                                      "ttf", "otf", "glb", "gltf", "fbx", "obj", "bin", "spv", "dxbc", "exe", "dll",
                                      "lib", "pdb", "obj", "zip", "ico", "psd", "blend"};
    return set;
}

int lineAt(const QString& text, qsizetype offset, QVector<qsizetype>& lineStarts)
{
    if (lineStarts.isEmpty()) {
        lineStarts.push_back(0);
        for (qsizetype i = 0; i < text.size(); ++i)
            if (text[i] == QLatin1Char('\n')) lineStarts.push_back(i + 1);
    }
    const auto it = std::upper_bound(lineStarts.begin(), lineStarts.end(), offset);
    return static_cast<int>(it - lineStarts.begin());
}

} // namespace

ScanFileKind scanFileKind(const QString& fileName)
{
    const QString ext = QFileInfo(fileName).suffix().toLower();
    if (cppExtensions().contains(ext)) return ScanFileKind::Cpp;
    if (ext == QLatin1String("rml")) return ScanFileKind::Rml;
    if (skippedExtensions().contains(ext)) return ScanFileKind::Skip;
    return ScanFileKind::Text;
}

void scanText(const QString& text, ScanFileKind kind, const QString& file, const QStringList& cppFunctions, ScanResult& out)
{
    QRegularExpression re;
    switch (kind) {
    case ScanFileKind::Cpp: {
        QStringList names;
        for (const QString& f : cppFunctions)
            if (!f.trimmed().isEmpty()) names << QRegularExpression::escape(f.trimmed());
        if (names.isEmpty()) return;
        re.setPattern(QStringLiteral(R"re((?<![A-Za-z0-9_])(?:%1)\s*\(\s*(?:u8|L|u|U)?"#?([A-Za-z][A-Za-z0-9_.\-]*)")re")
                          .arg(names.join(QLatin1Char('|'))));
        break;
    }
    case ScanFileKind::Rml:
        re.setPattern(QStringLiteral(R"re(>\s*#([A-Za-z][A-Za-z0-9_.\-]*)\s*<)re"));
        break;
    case ScanFileKind::Text:
        re.setPattern(QStringLiteral(R"re((?<![A-Za-z0-9_#&/])#([A-Za-z][A-Za-z0-9_\-]*(?:\.[A-Za-z0-9_\-]+)+)(?!\.?[A-Za-z0-9_\-]))re"));
        break;
    case ScanFileKind::Skip:
        return;
    }

    QVector<qsizetype> lineStarts;
    QRegularExpressionMatchIterator it = re.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch m = it.next();
        const QString key = m.captured(1);
        if (key.endsWith(QLatin1Char('.'))) continue;
        out.references[key].push_back({file, lineAt(text, m.capturedStart(1), lineStarts)});
    }
}

ScanResult scanForKeys(const ScanSettings& settings)
{
    ScanResult result;
    const QDir base(settings.baseFolder);
    QSet<QString> seen;
    for (const QString& folder : settings.folders) {
        if (!QFileInfo(folder).isDir()) continue;
        QDirIterator it(folder, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
        while (it.hasNext()) {
            const QString path = QDir::cleanPath(it.next());
            if (seen.contains(path)) continue;
            seen.insert(path);

            const QString relative = base.relativeFilePath(path);
            bool skippedFolder = false;
            const QStringList parts = QDir(folder).relativeFilePath(path).split(QLatin1Char('/'));
            for (int i = 0; i + 1 < parts.size() && !skippedFolder; ++i)
                skippedFolder = settings.skipFolderNames.contains(parts[i], Qt::CaseInsensitive);
            if (skippedFolder) continue;

            const ScanFileKind kind = scanFileKind(path);
            if (kind == ScanFileKind::Skip || it.fileInfo().size() > kMaxFileSize) continue;
            QFile f(path);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QByteArray bytes = f.readAll();
            if (bytes.left(8192).contains('\0')) continue;   // binary
            ++result.filesScanned;
            scanText(QString::fromUtf8(bytes), kind, relative, settings.cppFunctions, result);
        }
    }
    return result;
}

} // namespace kle
