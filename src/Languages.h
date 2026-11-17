#pragma once
#include <QString>
#include <QStringList>

namespace kle {

struct LanguageInfo
{
    QString code;
    QString name;
    QString pluralForms;
};

const QList<LanguageInfo>& knownLanguages();
QString languageName(const QString& code);
QString pluralFormsFor(const QString& code);
QString normalizeLanguageCode(const QString& code);

// Sample n values per form ("n = 2, 3, 4, 22…"), computed with the engine's parser.
QStringList pluralFormLabels(const QString& pluralForms, int count);

} // namespace kle
