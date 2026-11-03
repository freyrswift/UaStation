#include "skill_catalog.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringDecoder>
#include <QStringList>
#include <QVector>
#include <algorithm>

namespace ua::core {
namespace {

constexpr qsizetype MaxDocumentBytes = 8 * 1024 * 1024;
constexpr qsizetype MaxFrontmatterBytes = 64 * 1024;
constexpr qsizetype MaxFrontmatterLines = 2048;
constexpr qsizetype MaxDescriptionLength = 1024;

struct Line {
    qsizetype start;
    qsizetype contentEnd;
    qsizetype end;
    QString text;
};

struct Field {
    QString value;
    qsizetype start = 0;
    qsizetype end = 0;
    bool present = false;
};

struct Frontmatter {
    Field name;
    Field description;
};

bool fail(QString* error, const char* message)
{
    if (error)
        *error = QString::fromLatin1(message);
    return false;
}

bool canonical(const QString& name)
{
    if (name.isEmpty() || name.size() > 64 || name.front() == QLatin1Char('-') ||
        name.back() == QLatin1Char('-') || name.contains(QLatin1String("--")))
        return false;
    for (const QChar c : name) {
        if (!((c >= QLatin1Char('a') && c <= QLatin1Char('z')) ||
              (c >= QLatin1Char('0') && c <= QLatin1Char('9')) || c == QLatin1Char('-')))
            return false;
    }
    return true;
}

qsizetype indentation(const QString& text)
{
    qsizetype count = 0;
    while (count < text.size() && text[count] == QLatin1Char(' '))
        ++count;
    return count;
}

bool emptyOrComment(const QString& text)
{
    const QString trimmed = text.trimmed();
    return trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'));
}

QString stripPlainComment(const QString& text)
{
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text[i] == QLatin1Char('#') && (i == 0 || text[i - 1].isSpace()))
            return text.first(i).trimmed();
    }
    return text.trimmed();
}

// YAML flow scalars fold one line break to a space and blank lines to newlines.
QString foldFlow(const QString& source)
{
    const QStringList lines = source.split(QLatin1Char('\n'));
    QString result = lines.first();
    for (qsizetype i = 1; i < lines.size();) {
        qsizetype blanks = 0;
        while (i < lines.size() && lines[i].trimmed().isEmpty()) {
            ++blanks;
            ++i;
        }
        if (i == lines.size()) {
            result += QString(blanks, QLatin1Char('\n'));
            break;
        }
        while (!result.isEmpty() && (result.back() == QLatin1Char(' ') ||
                                     result.back() == QLatin1Char('\t')))
            result.chop(1);
        result += blanks ? QString(blanks, QLatin1Char('\n')) : QStringLiteral(" ");
        result += lines[i].trimmed();
        ++i;
    }
    return result;
}

bool appendCodepoint(QString& output, uint codepoint)
{
    if (codepoint > 0x10ffff || (codepoint >= 0xd800 && codepoint <= 0xdfff))
        return false;
    if (codepoint <= 0xffff) {
        output += QChar(ushort(codepoint));
    } else {
        output += QChar(QChar::highSurrogate(codepoint));
        output += QChar(QChar::lowSurrogate(codepoint));
    }
    return true;
}

bool decodeQuoted(const QString& raw, QChar quote, QString* value, qsizetype* consumed)
{
    QString flow;
    for (qsizetype i = 0; i < raw.size(); ++i) {
        if (quote == QLatin1Char('"') && raw[i] == QLatin1Char('\\') &&
            i + 1 < raw.size() && raw[i + 1] == QLatin1Char('\n')) {
            ++i;
            while (i + 1 < raw.size() && (raw[i + 1] == QLatin1Char(' ') ||
                                        raw[i + 1] == QLatin1Char('\t')))
                ++i;
        } else if (quote == QLatin1Char('"') && raw[i] == QLatin1Char('\\') &&
                   i + 1 < raw.size()) {
            flow += raw[i];
            flow += raw[++i];
        } else {
            flow += raw[i];
        }
    }
    flow = foldFlow(flow);
    QString decoded;
    for (qsizetype i = 1; i < flow.size(); ++i) {
        const QChar c = flow[i];
        if (c == quote) {
            if (quote == QLatin1Char('\'') && i + 1 < flow.size() && flow[i + 1] == quote) {
                decoded += quote;
                ++i;
                continue;
            }
            const QString tail = flow.sliced(i + 1).trimmed();
            if (!tail.isEmpty() && !tail.startsWith(QLatin1Char('#')))
                return false;
            *value = decoded;
            *consumed = i + 1;
            return true;
        }
        if (quote != QLatin1Char('"') || c != QLatin1Char('\\')) {
            decoded += c;
            continue;
        }
        if (++i == flow.size())
            return false;
        const QChar escape = flow[i];
        if (escape == QLatin1Char('\n')) {
            while (i + 1 < flow.size() && (flow[i + 1] == QLatin1Char(' ') ||
                                        flow[i + 1] == QLatin1Char('\t')))
                ++i;
            continue;
        }
        const QString codes = QStringLiteral("0abtnvfre \"/\\N_LP");
        const ushort replacements[] = {0, 7, 8, 9, 10, 11, 12, 13, 27, 32, 34,
                                       47, 92, 0x85, 0xa0, 0x2028, 0x2029};
        const qsizetype index = codes.indexOf(escape);
        if (index >= 0) {
            decoded += QChar(replacements[index]);
            continue;
        }
        int digits = 0;
        if (escape == QLatin1Char('x')) digits = 2;
        if (escape == QLatin1Char('u')) digits = 4;
        if (escape == QLatin1Char('U')) digits = 8;
        if (!digits || i + digits >= flow.size())
            return false;
        uint codepoint = 0;
        for (int digit = 0; digit < digits; ++digit) {
            const QChar hex = flow[++i];
            const int number = hex.digitValue();
            int value = number;
            if (hex >= QLatin1Char('a') && hex <= QLatin1Char('f')) value = hex.unicode() - 'a' + 10;
            else if (hex >= QLatin1Char('A') && hex <= QLatin1Char('F')) value = hex.unicode() - 'A' + 10;
            else if (hex < QLatin1Char('0') || hex > QLatin1Char('9')) return false;
            codepoint = (codepoint << 4) | uint(value);
        }
        if (!appendCodepoint(decoded, codepoint))
            return false;
    }
    return false;
}

bool parseScalar(const QVector<Line>& lines, qsizetype close,
                 qsizetype* lineIndex, qsizetype valueColumn, Field* field, QString* error)
{
    const qsizetype first = *lineIndex;
    const Line& header = lines[first];
    const QString rawValue = header.text.sliced(valueColumn);
    const QString value = rawValue.trimmed();
    field->start = header.start + header.text.first(valueColumn).toUtf8().size();
    field->end = header.contentEnd;
    field->present = true;
    if (value.startsWith(QLatin1Char('|')) || value.startsWith(QLatin1Char('>'))) {
        const QString marker = stripPlainComment(value);
        QChar chomp;
        int explicitIndent = 0;
        for (qsizetype i = 1; i < marker.size(); ++i) {
            if ((marker[i] == QLatin1Char('+') || marker[i] == QLatin1Char('-')) && chomp.isNull())
                chomp = marker[i];
            else if (marker[i] >= QLatin1Char('1') && marker[i] <= QLatin1Char('9') && !explicitIndent)
                explicitIndent = marker[i].unicode() - '0';
            else
                return fail(error, "Invalid block scalar indicator");
        }
        qsizetype last = first + 1;
        qsizetype indent = explicitIndent;
        while (last < close && (lines[last].text.trimmed().isEmpty() ||
                               indentation(lines[last].text) > 0)) {
            const QString& text = lines[last].text;
            if (!text.trimmed().isEmpty() && !indent)
                indent = indentation(text);
            if (!text.trimmed().isEmpty() && indentation(text) < indent)
                return fail(error, "Invalid block scalar indentation");
            ++last;
        }
        QStringList content;
        QVector<bool> moreIndented;
        qsizetype lastNonblank = first;
        for (qsizetype i = first + 1; i < last; ++i) {
            const QString& text = lines[i].text;
            const bool blank = text.trimmed().isEmpty();
            content += blank ? QString() : text.sliced(indent);
            moreIndented += !blank && indentation(text) > indent;
            if (!blank)
                lastNonblank = i;
        }
        QString decoded;
        for (qsizetype i = 0; i < content.size(); ++i) {
            decoded += content[i];
            if (marker.front() == QLatin1Char('|') || i + 1 == content.size() ||
                moreIndented[i] || moreIndented[i + 1]) {
                decoded += QLatin1Char('\n');
            } else if (!content[i].isEmpty() && !content[i + 1].isEmpty()) {
                decoded += QLatin1Char(' ');
            } else if (content[i].isEmpty()) {
                decoded += QLatin1Char('\n');
            }
        }
        if (chomp != QLatin1Char('+')) {
            while (decoded.endsWith(QLatin1Char('\n')))
                decoded.chop(1);
            if (chomp != QLatin1Char('-') && lastNonblank > first)
                decoded += QLatin1Char('\n');
        }
        field->value = decoded;
        field->end = lines[lastNonblank].contentEnd;
        *lineIndex = last - 1;
        return true;
    }
    if (value.startsWith(QLatin1Char('\'')) || value.startsWith(QLatin1Char('"'))) {
        QString quoted = rawValue.trimmed();
        const QChar quote = quoted.front();
        qsizetype current = first;
        qsizetype consumed = 0;
        while (true) {
            if (decodeQuoted(quoted, quote, &field->value, &consumed)) {
                field->end = lines[current].contentEnd;
                *lineIndex = current;
                return true;
            }
            if (++current >= close || (!lines[current].text.trimmed().isEmpty() &&
                                       indentation(lines[current].text) == 0))
                return fail(error, "Invalid or unterminated quoted scalar");
            quoted += QLatin1Char('\n') + lines[current].text;
        }
    }
    if (!value.isEmpty() && QStringLiteral("[{}]&*!%@`").contains(value.front()))
        return fail(error, "Skill metadata must be a scalar string");
    QString plain = stripPlainComment(rawValue);
    qsizetype current = first + 1;
    qsizetype lastContent = first;
    qsizetype blanks = 0;
    while (current < close) {
        const QString& next = lines[current].text;
        if (next.trimmed().isEmpty()) {
            ++blanks;
            ++current;
            continue;
        }
        if (indentation(next) == 0 || next.trimmed().startsWith(QLatin1Char('#')))
            break;
        const QString continuation = stripPlainComment(next);
        if (!continuation.isEmpty()) {
            if (!plain.isEmpty())
                plain += blanks ? QString(blanks, QLatin1Char('\n')) : QStringLiteral(" ");
            plain += continuation;
        }
        blanks = 0;
        lastContent = current;
        ++current;
    }
    field->value = plain;
    field->end = lines[lastContent].contentEnd;
    *lineIndex = lastContent;
    return true;
}

bool readFrontmatter(const QByteArray& source, Frontmatter* fields, QString* error)
{
    if (source.size() > MaxDocumentBytes)
        return fail(error, "Skill document exceeds the size limit");
    if (!source.isValidUtf8())
        return fail(error, "Skill document is not valid UTF-8");
    QStringDecoder decoder(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
    const QString document = decoder.decode(source);
    if (decoder.hasError())
        return fail(error, "Skill document is not valid UTF-8");
    Q_UNUSED(document);
    QVector<Line> lines;
    qsizetype offset = 0;
    qsizetype close = -1;
    while (offset < source.size() && offset <= MaxFrontmatterBytes &&
           lines.size() < MaxFrontmatterLines) {
        const qsizetype newline = source.indexOf('\n', offset);
        const qsizetype end = newline < 0 ? source.size() : newline + 1;
        qsizetype contentEnd = newline < 0 ? end : newline;
        if (contentEnd > offset && source[contentEnd - 1] == '\r')
            --contentEnd;
        QString text = QString::fromUtf8(source.constData() + offset, contentEnd - offset);
        if (lines.isEmpty() && text.startsWith(QChar(0xfeff)))
            text.remove(0, 1);
        for (const QChar c : text) {
            if ((c.unicode() < 0x20 && c != QLatin1Char('\t')) || c == QChar(0x7f))
                return fail(error, "Control character in frontmatter");
        }
        lines += Line{offset, contentEnd, end, text};
        if (lines.size() == 1 && text.trimmed() != QLatin1String("---"))
            return fail(error, "Missing YAML frontmatter");
        if (lines.size() > 1 && (text == QLatin1String("---") || text == QLatin1String("..."))) {
            close = lines.size() - 1;
            break;
        }
        offset = end;
    }
    if (close < 0 || lines[close].end > MaxFrontmatterBytes)
        return fail(error, "Missing or oversized frontmatter terminator");
    bool unrelatedSequence = false;
    for (qsizetype i = 1; i < close; ++i) {
        const QString& text = lines[i].text;
        if (emptyOrComment(text) || indentation(text) > 0)
            continue;
        if (text.startsWith(QLatin1Char('\t')))
            return fail(error, "Tabs are not frontmatter indentation");
        // YAML permits an unindented sequence as the value of an unrelated
        // mapping key. Its entries are not top-level metadata fields.
        if (unrelatedSequence && (text == QLatin1String("-") ||
            (text.startsWith('-') && text.size() > 1 && text[1].isSpace())))
            continue;
        const qsizetype colon = text.indexOf(QLatin1Char(':'));
        if (colon <= 0 || (colon + 1 < text.size() && !text[colon + 1].isSpace()))
            return fail(error, "Invalid top-level frontmatter mapping");
        QString key = text.first(colon).trimmed();
        if (key.startsWith(QLatin1Char('\'')) || key.startsWith(QLatin1Char('"'))) {
            QString decoded;
            qsizetype consumed = 0;
            if (!decodeQuoted(key, key.front(), &decoded, &consumed))
                return fail(error, "Invalid quoted metadata key");
            key = decoded;
        }
        Field* field = nullptr;
        if (key == QLatin1String("name")) field = &fields->name;
        if (key == QLatin1String("description")) field = &fields->description;
        if (!field) {
            unrelatedSequence = stripPlainComment(text.sliced(colon + 1)).isEmpty();
            continue;
        }
        unrelatedSequence = false;
        if (field->present)
            return fail(error, "Duplicate name or description");
        qsizetype valueColumn = colon + 1;
        while (valueColumn < text.size() && text[valueColumn].isSpace())
            ++valueColumn;
        if (!parseScalar(lines, close, &i, valueColumn, field, error))
            return false;
    }
    if (!fields->name.present || !fields->description.present ||
        fields->name.value.trimmed().isEmpty() || fields->description.value.trimmed().isEmpty())
        return fail(error, "Skill frontmatter requires a nonempty name and description");
    return true;
}

QString shortDescription(const QString& description)
{
    if (description.size() <= MaxDescriptionLength)
        return description;
    qsizetype end = MaxDescriptionLength;
    if (description[end - 1].isHighSurrogate())
        --end;
    if (!description[end].isSpace() && !description[end - 1].isSpace()) {
        qsizetype boundary = end;
        while (boundary > 0 && !description[boundary - 1].isSpace())
            --boundary;
        if (boundary > 0)
            end = boundary;
    }
    return description.first(end).trimmed();
}

QByteArray quoteYaml(const QString& text)
{
    const QByteArray json = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return json.sliced(1, json.size() - 2);
}

} // namespace

bool SkillCatalog::parse(const QByteArray& source, SkillMetadata* metadata, QString* error)
{
    if (error)
        error->clear();
    if (!metadata)
        return fail(error, "Metadata output is required");
    Frontmatter fields;
    if (!readFrontmatter(source, &fields, error))
        return false;
    if (!canonical(fields.name.value))
        return fail(error, "Skill name must be a canonical identifier of at most 64 characters");
    if (fields.description.value.size() > MaxDescriptionLength)
        return fail(error, "Skill description exceeds 1024 characters");
    *metadata = {fields.name.value, fields.description.value};
    return true;
}

QByteArray SkillCatalog::normalize(const QByteArray& source, const QString& canonicalName, bool* ok)
{
    if (ok)
        *ok = false;
    Frontmatter fields;
    if (!canonical(canonicalName) || !readFrontmatter(source, &fields, nullptr))
        return {};
    struct Replacement { qsizetype start; qsizetype end; QByteArray bytes; };
    QVector<Replacement> replacements;
    if (fields.name.value != canonicalName)
        replacements += Replacement{fields.name.start, fields.name.end, canonicalName.toUtf8()};
    if (fields.description.value.size() > MaxDescriptionLength)
        replacements += Replacement{fields.description.start, fields.description.end,
                                    quoteYaml(shortDescription(fields.description.value))};
    std::sort(replacements.begin(), replacements.end(), [](const auto& a, const auto& b) {
        return a.start > b.start;
    });
    QByteArray normalized = source;
    for (const auto& replacement : replacements)
        normalized.replace(replacement.start, replacement.end - replacement.start, replacement.bytes);
    if (ok)
        *ok = true;
    return normalized;
}

} // namespace ua::core
