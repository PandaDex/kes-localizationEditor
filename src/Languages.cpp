#include "Languages.h"

#include "po_catalog.h"

#include <QRegularExpression>

namespace kle {

namespace {
const QString kOneOther = QStringLiteral("nplurals=2; plural=(n != 1);");
const QString kZeroOne = QStringLiteral("nplurals=2; plural=(n > 1);");
const QString kNone = QStringLiteral("nplurals=1; plural=0;");
const QString kSlavic = QStringLiteral("nplurals=3; plural=(n%10==1 && n%100!=11 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);");
} // namespace

const QList<LanguageInfo>& knownLanguages()
{
    static const QList<LanguageInfo> list = {
        {"ar", "Arabic", "nplurals=6; plural=(n==0 ? 0 : n==1 ? 1 : n==2 ? 2 : n%100>=3 && n%100<=10 ? 3 : n%100>=11 ? 4 : 5);"},
        {"bg", "Bulgarian", kOneOther},
        {"cs", "Czech", "nplurals=3; plural=(n==1 ? 0 : (n>=2 && n<=4) ? 1 : 2);"},
        {"da", "Danish", kOneOther},
        {"de", "German", kOneOther},
        {"el", "Greek", kOneOther},
        {"en", "English", kOneOther},
        {"es", "Spanish", kOneOther},
        {"es_MX", "Spanish (Mexico)", kOneOther},
        {"fi", "Finnish", kOneOther},
        {"fr", "French", kZeroOne},
        {"hu", "Hungarian", kOneOther},
        {"it", "Italian", kOneOther},
        {"ja", "Japanese", kNone},
        {"ko", "Korean", kNone},
        {"nl", "Dutch", kOneOther},
        {"no", "Norwegian", kOneOther},
        {"pl", "Polish", "nplurals=3; plural=(n==1 ? 0 : n%10>=2 && n%10<=4 && (n%100<10 || n%100>=20) ? 1 : 2);"},
        {"pt", "Portuguese", kOneOther},
        {"pt_BR", "Portuguese (Brazil)", kZeroOne},
        {"ro", "Romanian", "nplurals=3; plural=(n==1 ? 0 : (n==0 || (n%100>0 && n%100<20)) ? 1 : 2);"},
        {"ru", "Russian", kSlavic},
        {"sk", "Slovak", "nplurals=3; plural=(n==1 ? 0 : (n>=2 && n<=4) ? 1 : 2);"},
        {"sv", "Swedish", kOneOther},
        {"th", "Thai", kNone},
        {"tr", "Turkish", kOneOther},
        {"uk", "Ukrainian", kSlavic},
        {"vi", "Vietnamese", kNone},
        {"zh_CN", "Chinese (Simplified)", kNone},
        {"zh_TW", "Chinese (Traditional)", kNone},
    };
    return list;
}

QString languageName(const QString& code)
{
    for (const LanguageInfo& l : knownLanguages())
        if (l.code == code) return l.name;
    const QString base = code.section(QLatin1Char('_'), 0, 0);
    for (const LanguageInfo& l : knownLanguages())
        if (l.code == base) return QStringLiteral("%1 (%2)").arg(l.name, code.section(QLatin1Char('_'), 1));
    return code;
}

QString pluralFormsFor(const QString& code)
{
    for (const LanguageInfo& l : knownLanguages())
        if (l.code == code) return l.pluralForms;
    const QString base = code.section(QLatin1Char('_'), 0, 0);
    for (const LanguageInfo& l : knownLanguages())
        if (l.code == base) return l.pluralForms;
    return kOneOther;
}

QString normalizeLanguageCode(const QString& code)
{
    static const QRegularExpression re(QStringLiteral(R"(^([A-Za-z]{2,3})(?:[-_]([A-Za-z0-9]{2,4}))?$)"));
    const QRegularExpressionMatch m = re.match(code.trimmed());
    if (!m.hasMatch()) return {};
    QString out = m.captured(1).toLower();
    if (!m.captured(2).isEmpty()) out += QLatin1Char('_') + m.captured(2).toUpper();
    return out;
}

QStringList pluralFormLabels(const QString& pluralForms, int count)
{
    QStringList labels;
    PoCatalog catalog;
    const std::string header = "msgid \"\"\nmsgstr \"Plural-Forms: " + pluralForms.toStdString() + "\\n\"\n";
    const bool ok = catalog.Parse(header);
    QVector<QList<int>> samples(count);
    for (int n = 0; n <= 200 && ok; ++n) {
        const size_t index = catalog.PluralIndex(static_cast<uint64_t>(n));
        if (index < static_cast<size_t>(count) && samples[static_cast<int>(index)].size() < 4)
            samples[static_cast<int>(index)].push_back(n);
    }
    for (int i = 0; i < count; ++i) {
        QStringList values;
        for (int n : samples[i]) values << QString::number(n);
        labels << (values.isEmpty() ? QStringLiteral("form %1").arg(i) : QStringLiteral("n = %1…").arg(values.join(QStringLiteral(", "))));
    }
    return labels;
}

} // namespace kle
