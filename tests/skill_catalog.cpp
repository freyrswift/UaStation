#include "core/skill_catalog.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSet>
#include <QTextStream>

using ua::core::SkillCatalog;
using ua::core::SkillMetadata;

namespace {

int failures = 0;

void check(bool condition, const QString& message)
{
    if (condition)
        return;
    QTextStream(stderr) << "FAIL: " << message << '\n';
    ++failures;
}

QByteArray skill(const QByteArray& name, const QByteArray& description)
{
    return "---\nname: " + name + "\ndescription: " + description + "\n---\n# Body\n";
}

QByteArray yamlQuote(const QString& text)
{
    const QByteArray array = QJsonDocument(QJsonArray{text}).toJson(QJsonDocument::Compact);
    return array.sliced(1, array.size() - 2);
}

void expectDescription(const QByteArray& description, const QString& expected, const QString& label)
{
    SkillMetadata metadata;
    QString error;
    check(SkillCatalog::parse(skill("scalar-test", description), &metadata, &error), label + ": " + error);
    check(metadata.description == expected, label + ": scalar value differs");
}

void scalarTests()
{
    expectDescription("plain text # comment", QStringLiteral("plain text"), "plain comment");
    expectDescription("one line\n  continued here", QStringLiteral("one line continued here"), "plain multiline");
    expectDescription("one\n\n  paragraph", QStringLiteral("one\nparagraph"), "plain paragraph");
    expectDescription("\n  delayed scalar", QStringLiteral("delayed scalar"), "implicit plain scalar");
    expectDescription("'it''s # literal'", QStringLiteral("it's # literal"), "single quote escapes");
    expectDescription("'first\n  second'", QStringLiteral("first second"), "single quoted multiline");
    expectDescription("\"first\n  second\"", QStringLiteral("first second"), "double quoted multiline");
    expectDescription("\"first\\\n  second\"", QStringLiteral("firstsecond"), "escaped line break");
    expectDescription("\"a\\nb\\t\\u03a9\\U0001f642\"",
                      QString::fromUtf8("a\nb\t\xce\xa9\xf0\x9f\x99\x82"), "double quoted escapes");
    expectDescription(">-\n  first\n  second\n\n  third", QStringLiteral("first second\nthird"), "folded strip");
    expectDescription(">\n  first\n  second", QStringLiteral("first second\n"), "folded clip");
    expectDescription("|-\n  first\n  second", QStringLiteral("first\nsecond"), "literal strip");
    expectDescription("|\n  first\n  second", QStringLiteral("first\nsecond\n"), "literal clip");
    expectDescription("|+\n  first\n\n", QStringLiteral("first\n\n\n"), "literal keep");
    expectDescription("|2- # indentation\n  first\n    deeper", QStringLiteral("first\n  deeper"), "explicit indentation");
    expectDescription(">-\n  first\n    indented\n  last", QStringLiteral("first\n  indented\nlast"), "folded extra indentation");

    SkillMetadata metadata;
    QString error;
    const QByteArray nested = "---\nname: scalar-test\nmetadata:\n  name: nested\n"
                              "  description: nested\ndescription: top-level\n---\nname: body\n";
    check(SkillCatalog::parse(nested, &metadata, &error) && metadata.description == QLatin1String("top-level"),
          "only top-level metadata is used: " + error);
    const QByteArray sequence = "---\nname: 'Legacy Name'\ntags:\n- first\n- name: nested\n"
                                "  description: nested\ndescription: top-level\n"
                                "references: # retained comment\n- one\n- two\n---\nbody\n";
    bool sequenceOk = false;
    const auto normalizedSequence = SkillCatalog::normalize(sequence, QStringLiteral("scalar-test"), &sequenceOk);
    QByteArray expectedSequence = sequence;
    expectedSequence.replace("name: 'Legacy Name'", "name: scalar-test");
    check(sequenceOk && normalizedSequence == expectedSequence &&
          SkillCatalog::parse(normalizedSequence, &metadata, &error) &&
          metadata.description == QLatin1String("top-level"),
          "unrelated indentless sequences are preserved and nested metadata ignored: " + error);
    const QByteArray crlf = "\xef\xbb\xbf---\r\n\"name\": 'scalar-test'\r\n"
                            "'description': \"a \\u03a9\"\r\n---\r\nbody\r\n";
    check(SkillCatalog::parse(crlf, &metadata, &error) && metadata.description == QString::fromUtf8("a \xce\xa9"),
          "BOM, CRLF and quoted keys: " + error);
    bool ok = false;
    check(SkillCatalog::normalize(crlf, QStringLiteral("scalar-test"), &ok) == crlf && ok,
          "normalization does not rewrite equivalent quoted values or CRLF");
}

void invalidTests()
{
    const QList<QByteArray> invalid = {
        {}, "name: example\ndescription: missing frontmatter\n", "---\nname: no-end\ndescription: missing\n",
        skill("Uppercase", "description"), skill("bad_name", "description"), skill("../path", "description"),
        skill("-leading", "description"), skill("trailing-", "description"), skill("two--hyphens", "description"),
        skill(QByteArray(65, 'a'), "description"), skill("''", "description"), skill("example", "''"),
        "---\nname: example\n---\n", "---\ndescription: example\n---\n",
        "---\nname: example\nname: other\ndescription: example\n---\n",
        "---\nname: example\n\"name\": other\ndescription: example\n---\n",
        "---\nname: example\ndescription: first\ndescription: second\n---\n",
        "---\nname: example\ndescription:\n- not-a-scalar\n---\n",
        "---\nname: example\n- unexpected-sequence\ndescription: text\n---\n",
        skill("example", "[not, scalar]"), skill("example", "{mapping: value}"),
        skill("example", "*alias"), skill("example", "!!str value"),
        skill("example", "\"bad\\qescape\""), skill("example", "\"unterminated"),
        skill("example", "'unterminated"), skill("example", "\"value\" unexpected"),
        skill("example", "|0\n  bad"), skill("example", "|--\n  bad"),
        skill("example", "|4\n  under-indented"), skill("example", QByteArray(1025, 'a')),
        skill("example", "description") + QByteArray::fromHex("c0af"),
        skill("example", "description") + QByteArray::fromHex("eda080"),
        skill("example", "description") + QByteArray::fromHex("f4908080"),
        skill("example", "description") + QByteArray::fromHex("e282"),
        QByteArray("---\nname: example\ndescription: a") + QByteArray(1, '\0') + "b\n---\n",
        "---\nname:example\ndescription: invalid mapping\n---\n"
    };
    for (qsizetype i = 0; i < invalid.size(); ++i) {
        SkillMetadata metadata{QStringLiteral("sentinel"), QStringLiteral("unchanged")};
        QString error;
        check(!SkillCatalog::parse(invalid[i], &metadata, &error) && !error.isEmpty(),
              QStringLiteral("invalid metadata rejected (%1)").arg(i));
        check(metadata.name == QLatin1String("sentinel") && metadata.description == QLatin1String("unchanged"),
              QStringLiteral("failed parse leaves output intact (%1)").arg(i));
    }
    SkillMetadata metadata;
    QString error;
    check(!SkillCatalog::parse(skill("example", "description"), nullptr, &error) && !error.isEmpty(),
          "null output rejected");
    check(SkillCatalog::parse(skill(QByteArray(64, 'a'), "description"), &metadata), "64-character name accepted");
    check(SkillCatalog::parse(skill("example", QByteArray(1024, 'a')), &metadata), "1024-character description accepted");
    const QByteArray hugeHeader = "---\n# " + QByteArray(64 * 1024, 'x') + "\nname: example\ndescription: text\n---\n";
    check(!SkillCatalog::parse(hugeHeader, &metadata), "oversized frontmatter rejected");
    const QByteArray manyLines = "---\n" + QByteArray("# comment\n").repeated(2048) +
                                "name: example\ndescription: text\n---\n";
    check(!SkillCatalog::parse(manyLines, &metadata), "frontmatter line budget enforced");
    const QByteArray hugeDocument = skill("example", "description") + QByteArray(8 * 1024 * 1024, 'x');
    check(!SkillCatalog::parse(hugeDocument, &metadata), "document size budget enforced");
}

void normalizationTests()
{
    const QByteArray body = "# Unchanged body\r\nname: body-only\r\n---\r\n"
                            "\xe4\xb8\xad\xe6\x96\x87\r\n";
    const QByteArray prefix = "\xef\xbb\xbf---\r\n# retained comment\r\n";
    const QByteArray other = "metadata:\r\n  name: nested-name\r\n  description: keep-exact\r\n"
                             "license: 'MIT'  # preserved\r\n";
    const QByteArray input = prefix + "name: 'Legacy Name'\r\n" + other + "description: short\r\n---\r\n" + body;
    const QByteArray expected = prefix + "name: canonical-name\r\n" + other + "description: short\r\n---\r\n" + body;
    bool ok = false;
    check(SkillCatalog::normalize(input, QStringLiteral("canonical-name"), &ok) == expected && ok,
          "only top-level name changes with byte-exact body and other metadata");

    const QString longText = QStringLiteral("word ").repeated(250);
    const QByteArray longInput = "---\nname: example\nextra: keep\ndescription: >-\n  " +
                                  longText.toUtf8() + "\nmetadata:\n  retained: value\n---\n" + body;
    const QByteArray normalized = SkillCatalog::normalize(longInput, QStringLiteral("example"), &ok);
    SkillMetadata metadata;
    check(ok && SkillCatalog::parse(normalized, &metadata) && metadata.description.size() <= 1024,
          "long folded description is shortened to the metadata limit");
    check(metadata.description == QStringLiteral("word ").repeated(205).trimmed() &&
          metadata.description.size() == 1024,
          "description is truncated at its final complete word");
    const auto midWord = SkillCatalog::normalize(skill("example", (QStringLiteral("start ") + longText).toUtf8()),
                                                  QStringLiteral("example"), &ok);
    SkillMetadata midWordMetadata;
    check(ok && SkillCatalog::parse(midWord, &midWordMetadata) &&
          midWordMetadata.description == QStringLiteral("start ") + QStringLiteral("word ").repeated(203).trimmed(),
          "description backs up when the limit falls inside a word");
    check(normalized.contains("extra: keep\n") && normalized.endsWith("metadata:\n  retained: value\n---\n" + body),
          "description rewrite preserves adjacent metadata and body");
    check(SkillCatalog::normalize(normalized, QStringLiteral("example"), &ok) == normalized && ok,
          "normalization is idempotent");

    const QString literalText = QStringLiteral("a\nb\t\"quoted\"\\ ").repeated(100);
    const QByteArray escaped = SkillCatalog::normalize(skill("example", yamlQuote(literalText)), QStringLiteral("example"), &ok);
    check(ok && SkillCatalog::parse(escaped, &metadata) && literalText.startsWith(metadata.description),
          "truncated quoted controls keep their decoded values");
    QString emojiText(1023, QLatin1Char('x'));
    emojiText += QString::fromUtf8("\xf0\x9f\x99\x82");
    const QByteArray emoji = SkillCatalog::normalize(skill("example", yamlQuote(emojiText)), QStringLiteral("example"), &ok);
    check(ok && SkillCatalog::parse(emoji, &metadata) && metadata.description.size() == 1023,
          "truncation never splits a UTF-16 surrogate pair");

    const QByteArray multilineName = "---\nname: 'Legacy\n  Name'\n# retain\ndescription: short\n---\nbody\n";
    check(SkillCatalog::normalize(multilineName, QStringLiteral("example"), &ok) ==
              QByteArray("---\nname: example\n# retain\ndescription: short\n---\nbody\n") && ok,
          "multiline name replacement preserves subsequent comment");
    check(SkillCatalog::normalize(input, QStringLiteral("Invalid Name"), &ok).isEmpty() && !ok,
          "invalid canonical replacement rejected");
    check(SkillCatalog::normalize("---\nname: x\nname: y\ndescription: text\n---\n",
                                  QStringLiteral("example"), &ok).isEmpty() && !ok,
          "normalization rejects duplicate metadata");
}

void bundledTests()
{
#ifdef UA_SOURCE_DIR
    const QString root = QString::fromUtf8(UA_SOURCE_DIR);
#else
    const QString root = QFileInfo(QString::fromUtf8(__FILE__)).absoluteDir().absoluteFilePath(QStringLiteral(".."));
#endif
    const QStringList roots = {root + "/resources/skills", root + "/resources/skills-upstream/rea",
                               root + "/resources/skills-upstream/reverse-skill"};
    QSet<QString> names;
    int count = 0;
    for (const QString& packageRoot : roots) {
        const QStringList packages = QDir(packageRoot).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString& package : packages) {
            const QString path = packageRoot + '/' + package + "/SKILL.md";
            QFile file(path);
            if (!file.exists())
                continue;
            ++count;
            check(!names.contains(package), "unique deployed package identifier: " + package);
            names.insert(package);
            check(file.open(QIODevice::ReadOnly), "open bundled skill: " + path);
            const QByteArray source = file.readAll();
            bool ok = false;
            const QByteArray deployed = SkillCatalog::normalize(source, package, &ok);
            SkillMetadata metadata;
            QString error;
            const bool parsed = ok && SkillCatalog::parse(deployed, &metadata, &error);
            check(parsed, "parse deployed bundled skill " + package + ": " + error);
            check(metadata.name == package && !metadata.description.trimmed().isEmpty() && metadata.description.size() <= 1024,
                  "canonical deployed bundled metadata: " + package);
            check(SkillCatalog::normalize(deployed, package, &ok) == deployed && ok,
                  "bundled normalization is idempotent: " + package);
            const qsizetype firstNewline = source.indexOf('\n');
            qsizetype bodyStart = firstNewline < 0 ? source.size() : firstNewline + 1;
            while (bodyStart < source.size()) {
                const qsizetype end = source.indexOf('\n', bodyStart);
                const qsizetype lineEnd = end < 0 ? source.size() : end;
                if (source.sliced(bodyStart, lineEnd - bodyStart).trimmed() == "---") {
                    bodyStart = end < 0 ? source.size() : end + 1;
                    break;
                }
                bodyStart = end < 0 ? source.size() : end + 1;
            }
            check(deployed.endsWith(source.sliced(bodyStart)), "byte-exact bundled body: " + package);
        }
    }
    check(count == 134, QStringLiteral("all 134 bundled packages exercised (found %1)").arg(count));
    QTextStream(stdout) << "Validated " << count << " bundled skill packages\n";
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    scalarTests();
    invalidTests();
    normalizationTests();
    bundledTests();
    if (!failures)
        QTextStream(stdout) << "Skill catalog tests passed\n";
    return failures ? 1 : 0;
}
