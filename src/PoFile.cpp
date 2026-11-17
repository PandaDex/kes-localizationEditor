#include "PoFile.h"

#include <QByteArray>
#include <QRegularExpression>

#include <algorithm>
#include <cctype>

namespace kle {

namespace {

QByteArray trimmed(const QByteArray& line) { return line.trimmed(); }

bool unescape(const QByteArray& line, int& pos, QByteArray& out, QString& error)
{
    ++pos;
    while (pos < line.size()) {
        const char ch = line[pos++];
        if (ch == '"') return true;
        if (ch != '\\') {
            out.append(ch);
            continue;
        }
        if (pos >= line.size()) break;
        const char e = line[pos++];
        switch (e) {
        case 'n': out.append('\n'); break;
        case 't': out.append('\t'); break;
        case 'r': out.append('\r'); break;
        case 'a': out.append('\a'); break;
        case 'b': out.append('\b'); break;
        case 'f': out.append('\f'); break;
        case 'v': out.append('\v'); break;
        case '\\': case '"': case '\'': case '?': out.append(e); break;
        case 'x': {
            int value = 0, digits = 0;
            while (pos < line.size() && digits < 2 && std::isxdigit(static_cast<unsigned char>(line[pos]))) {
                const char h = static_cast<char>(std::tolower(static_cast<unsigned char>(line[pos++])));
                value = value * 16 + (h <= '9' ? h - '0' : h - 'a' + 10);
                ++digits;
            }
            if (!digits) {
                error = QStringLiteral("\\x without hex digits");
                return false;
            }
            out.append(static_cast<char>(value));
            break;
        }
        default:
            if (e >= '0' && e <= '7') {
                int value = e - '0';
                for (int d = 1; d < 3 && pos < line.size() && line[pos] >= '0' && line[pos] <= '7'; ++d)
                    value = value * 8 + (line[pos++] - '0');
                out.append(static_cast<char>(value));
                break;
            }
            error = QStringLiteral("unknown escape \\%1").arg(QChar(e));
            return false;
        }
    }
    if (error.isEmpty()) error = QStringLiteral("unterminated string");
    return false;
}

struct Pending
{
    PoEntry entry;
    QStringList rawLines;
    QStringList commentLines;
    QByteArray context, msgid, msgidPlural;
    QVector<QByteArray> msgstr;
    QByteArray* target = nullptr;
    bool hasContext = false, hasMsgid = false, hasMsgstr = false, hasMsgidPlural = false, indexed = false;
    bool obsolete = false;
    int line = 0;

    bool empty() const { return rawLines.isEmpty(); }
};

void writeString(QString& out, const char* keyword, const QString& value)
{
    out += QLatin1String(keyword);
    const int newline = value.indexOf(QLatin1Char('\n'));
    if (newline < 0 || newline == value.size() - 1) {
        out += QStringLiteral(" \"%1\"\n").arg(escapePoString(value));
        return;
    }
    out += QStringLiteral(" \"\"\n");
    int start = 0;
    while (start < value.size()) {
        int end = value.indexOf(QLatin1Char('\n'), start);
        end = end < 0 ? value.size() : end + 1;
        out += QStringLiteral("\"%1\"\n").arg(escapePoString(value.mid(start, end - start)));
        start = end;
    }
}

void writeComments(QString& out, const char* prefix, const QStringList& lines)
{
    for (const QString& line : lines)
        out += line.isEmpty() ? QString::fromLatin1(prefix).trimmed() + QLatin1Char('\n')
                              : QLatin1String(prefix) + line + QLatin1Char('\n');
}

} // namespace

void PoEntry::setFuzzy(bool fuzzy)
{
    flags.removeAll(QStringLiteral("fuzzy"));
    if (fuzzy) flags.prepend(QStringLiteral("fuzzy"));
}

bool PoEntry::isTranslated() const
{
    if (isFuzzy() || translations.isEmpty()) return false;
    for (const QString& t : translations)
        if (t.isEmpty()) return false;
    return true;
}

QString PoFile::headerValue(const QString& name) const
{
    for (const PoHeaderField& f : header)
        if (f.name.compare(name, Qt::CaseInsensitive) == 0) return f.value;
    return {};
}

void PoFile::setHeaderValue(const QString& name, const QString& value)
{
    hasHeader = true;
    for (PoHeaderField& f : header) {
        if (f.name.compare(name, Qt::CaseInsensitive) == 0) {
            f.value = value;
            return;
        }
    }
    header.push_back({name, value});
}

int PoFile::findEntry(const QString& key) const
{
    if (indexSize_ != entries.size()) {
        index_.clear();
        for (int i = 0; i < entries.size(); ++i)
            if (!entries[i].isPassthrough() && !index_.contains(entries[i].key)) index_.insert(entries[i].key, i);
        indexSize_ = entries.size();
    }
    const int i = index_.value(key, -1);
    if (i >= 0 && entries[i].key != key) {   // renamed without invalidateIndex()
        indexSize_ = -1;
        return findEntry(key);
    }
    return i;
}

int PoFile::pluralCount() const
{
    static const QRegularExpression re(QStringLiteral(R"(nplurals\s*=\s*(\d+))"));
    const QRegularExpressionMatch m = re.match(headerValue(QStringLiteral("Plural-Forms")));
    const int count = m.hasMatch() ? m.captured(1).toInt() : 2;
    return count >= 1 && count <= 32 ? count : 2;
}

QString escapePoString(const QString& text)
{
    QString out;
    out.reserve(text.size());
    for (const QChar c : text) {
        switch (c.unicode()) {
        case '\\': out += QLatin1String("\\\\"); break;
        case '"': out += QLatin1String("\\\""); break;
        case '\n': out += QLatin1String("\\n"); break;
        case '\t': out += QLatin1String("\\t"); break;
        case '\r': out += QLatin1String("\\r"); break;
        default:
            if (c.unicode() < 0x20)
                out += QStringLiteral("\\x%1").arg(uint(c.unicode()), 2, 16, QLatin1Char('0'));
            else
                out += c;
        }
    }
    return out;
}

bool parsePo(const QString& text, PoFile& out, QString* error)
{
    out = PoFile{};
    QByteArray bytes = text.toUtf8();
    if (bytes.startsWith("\xEF\xBB\xBF")) bytes.remove(0, 3);
    const QList<QByteArray> lines = bytes.split('\n');

    Pending p;
    int lineNumber = 0;
    QString message;

    auto fail = [&](const QString& what) {
        if (error) *error = QStringLiteral("line %1: %2").arg(lineNumber).arg(what);
        out = PoFile{};
        return false;
    };

    auto finish = [&]() -> bool {
        Pending done = std::move(p);
        p = Pending{};
        if (done.empty()) return true;

        const bool keywords = done.hasContext || done.hasMsgid || done.hasMsgstr;
        if (!keywords || done.obsolete) {
            // Trailing comments or obsolete entries.
            PoEntry passthrough;
            passthrough.raw = done.rawLines.join(QLatin1Char('\n'));
            out.entries.push_back(passthrough);
            return true;
        }
        if (!done.hasMsgid || !done.hasMsgstr) {
            lineNumber = done.line;
            return fail(done.hasMsgid ? QStringLiteral("msgid without msgstr") : QStringLiteral("msgstr without msgid"));
        }

        if (!done.hasContext && done.msgid.isEmpty() && !out.hasHeader && out.entries.isEmpty()) {
            out.hasHeader = true;
            out.headerComments = done.commentLines;
            out.headerFlags = done.entry.flags;
            const QString header = done.msgstr.isEmpty() ? QString() : QString::fromUtf8(done.msgstr[0]);
            for (const QString& line : header.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const int colon = line.indexOf(QLatin1Char(':'));
                if (colon < 0) continue;
                out.header.push_back({line.left(colon).trimmed(), line.mid(colon + 1).trimmed()});
            }
            return true;
        }

        QString key = QString::fromUtf8(done.msgid).trimmed();
        if (key.startsWith(QLatin1Char('#'))) key.remove(0, 1);
        if (done.hasContext || key.isEmpty()) {
            PoEntry passthrough;
            passthrough.raw = done.rawLines.join(QLatin1Char('\n'));
            out.entries.push_back(passthrough);
            return true;
        }

        PoEntry entry = std::move(done.entry);
        entry.key = key;
        entry.plural = done.hasMsgidPlural || done.indexed;
        for (const QByteArray& s : done.msgstr) entry.translations.push_back(QString::fromUtf8(s));
        out.entries.push_back(std::move(entry));
        return true;
    };

    for (const QByteArray& rawLine : lines) {
        ++lineNumber;
        QByteArray line = trimmed(rawLine);
        const QString rawText = QString::fromUtf8(rawLine).remove(QLatin1Char('\r'));

        if (line.isEmpty()) {
            if (p.hasMsgstr && !finish()) return false;
            continue;
        }

        // Obsolete entries (#~) are kept as raw blocks up to the next blank
        // line, comment block or keyword.
        if (line.startsWith("#~")) {
            if (p.hasMsgstr && !finish()) return false;
            p.obsolete = true;
            p.rawLines << rawText;
            continue;
        }
        if (line.startsWith('#')) {
            if ((p.hasMsgstr || p.obsolete) && !finish()) return false;
            p.rawLines << rawText;
            const char kind = line.size() > 1 ? line[1] : ' ';
            const QString body = QString::fromUtf8(line.mid(2)).trimmed();
            switch (kind) {
            case '.': p.entry.extractedComments << body; break;
            case ':': p.entry.references << body; break;
            case ',':
                for (const QString& flag : body.split(QLatin1Char(','), Qt::SkipEmptyParts))
                    p.entry.flags << flag.trimmed();
                break;
            case '|': p.entry.previous << body; break;
            default: {
                QString comment = QString::fromUtf8(line.mid(1));
                if (comment.startsWith(QLatin1Char(' '))) comment.remove(0, 1);
                p.entry.translatorComments << comment;
                p.commentLines << rawText;
                break;
            }
            }
            p.target = nullptr;
            continue;
        }
        if (p.obsolete && !finish()) return false;
        p.rawLines << rawText;

        if (line.startsWith('"')) {
            if (!p.target) return fail(QStringLiteral("string without a keyword"));
            int pos = 0;
            if (!unescape(line, pos, *p.target, message)) return fail(message);
            if (!trimmed(line.mid(pos)).isEmpty()) return fail(QStringLiteral("unexpected text after string"));
            continue;
        }

        int keywordEnd = 0;
        while (keywordEnd < line.size() && !std::isspace(static_cast<unsigned char>(line[keywordEnd])) && line[keywordEnd] != '"')
            ++keywordEnd;
        const QByteArray keyword = line.left(keywordEnd);
        const QByteArray rest = trimmed(line.mid(keywordEnd));
        if (!rest.startsWith('"'))
            return fail(QStringLiteral("expected a quoted string after %1").arg(QString::fromUtf8(keyword)));

        if (keyword == "msgctxt" || keyword == "msgid") {
            const bool startsNew = p.hasMsgstr || (keyword == "msgid" && p.hasMsgid) ||
                                   (keyword == "msgctxt" && (p.hasContext || p.hasMsgid));
            if (startsNew) {
                p.rawLines.removeLast();   // this line belongs to the next entry
                if (!finish()) return false;
                p.rawLines << rawText;
            }
            if (!p.hasContext && !p.hasMsgid) p.line = lineNumber;
            if (keyword == "msgctxt") {
                p.hasContext = true;
                p.target = &p.context;
            } else {
                p.hasMsgid = true;
                p.target = &p.msgid;
            }
        } else if (keyword == "msgid_plural") {
            if (!p.hasMsgid || p.hasMsgstr) return fail(QStringLiteral("msgid_plural must follow msgid"));
            p.hasMsgidPlural = true;
            p.target = &p.msgidPlural;
        } else if (keyword == "msgstr") {
            if (!p.hasMsgid || p.hasMsgstr) return fail(QStringLiteral("msgstr must follow msgid"));
            p.hasMsgstr = true;
            p.msgstr.push_back({});
            p.target = &p.msgstr.back();
        } else if (keyword.startsWith("msgstr[") && keyword.endsWith(']')) {
            bool ok = false;
            const int index = QString::fromUtf8(keyword.mid(7, keyword.size() - 8)).toInt(&ok);
            if (!ok || !p.hasMsgid) return fail(QStringLiteral("bad %1").arg(QString::fromUtf8(keyword)));
            if (index != p.msgstr.size()) return fail(QStringLiteral("%1 is out of order").arg(QString::fromUtf8(keyword)));
            p.hasMsgstr = true;
            p.indexed = true;
            p.msgstr.push_back({});
            p.target = &p.msgstr.back();
        } else {
            return fail(QStringLiteral("unknown keyword %1").arg(QString::fromUtf8(keyword)));
        }

        int pos = 0;
        if (!unescape(rest, pos, *p.target, message)) return fail(message);
        if (!trimmed(rest.mid(pos)).isEmpty()) return fail(QStringLiteral("unexpected text after string"));
    }
    return finish();
}

QString writePo(const PoFile& file)
{
    QString out;
    if (file.hasHeader) {
        for (const QString& line : file.headerComments) out += line + QLatin1Char('\n');
        if (!file.headerFlags.isEmpty()) out += QStringLiteral("#, %1\n").arg(file.headerFlags.join(QStringLiteral(", ")));
        out += QStringLiteral("msgid \"\"\nmsgstr \"\"\n");
        for (const PoHeaderField& f : file.header)
            out += QStringLiteral("\"%1\"\n").arg(escapePoString(f.name + QStringLiteral(": ") + f.value + QLatin1Char('\n')));
    }

    const int plurals = file.pluralCount();
    for (const PoEntry& e : file.entries) {
        if (!out.isEmpty()) out += QLatin1Char('\n');
        if (e.isPassthrough()) {
            out += e.raw + QLatin1Char('\n');
            continue;
        }
        writeComments(out, "# ", e.translatorComments);
        writeComments(out, "#. ", e.extractedComments);
        writeComments(out, "#: ", e.references);
        if (!e.flags.isEmpty()) out += QStringLiteral("#, %1\n").arg(e.flags.join(QStringLiteral(", ")));
        writeComments(out, "#| ", e.previous);
        writeString(out, "msgid", e.key);
        if (e.plural) {
            // PO tools reject msgstr[n] without msgid_plural, so the key goes there too.
            writeString(out, "msgid_plural", e.key);
            const int count = std::max<int>(plurals, e.translations.size());
            for (int i = 0; i < count; ++i)
                writeString(out, QStringLiteral("msgstr[%1]").arg(i).toLatin1().constData(),
                            i < e.translations.size() ? e.translations[i] : QString());
        } else {
            writeString(out, "msgstr", e.translations.isEmpty() ? QString() : e.translations.first());
        }
    }
    return out;
}

} // namespace kle
