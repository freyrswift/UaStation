#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSet>
#include <QTemporaryDir>
#include <QTextStream>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace {

int checks = 0;
int failures = 0;

void check(bool passed, const char* name)
{
    ++checks;
    if (!passed) ++failures;
    QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << name << '\n';
}

struct CliResult {
    bool completed = false;
    bool normalExit = false;
    int exitCode = -1;
    QByteArray out;
    QByteArray err;
};

CliResult runCli(const QString& executable, const QStringList& arguments)
{
    QProcess process;
    process.setProcessChannelMode(QProcess::SeparateChannels);
#if defined(Q_OS_WIN)
    process.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments* arguments) {
        arguments->flags |= CREATE_NO_WINDOW;
    });
#endif
    process.start(executable, arguments);
    CliResult result;
    if (process.waitForStarted(5000)) {
        process.closeWriteChannel();
        result.completed = process.waitForFinished(5000);
    }
    if (!result.completed && process.state() != QProcess::NotRunning) {
        process.kill();
        process.waitForFinished(5000);
    }
    result.normalExit = result.completed && process.exitStatus() == QProcess::NormalExit;
    result.exitCode = process.exitCode();
    result.out = process.readAllStandardOutput();
    result.err = process.readAllStandardError();
    return result;
}

void putFile(const QString& path, const QByteArray& bytes)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) qFatal("Fixture directory setup failed");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size())
        qFatal("Fixture file setup failed");
}

bool linkPath(const QString& target, const QString& path, bool directory)
{
#if defined(Q_OS_WIN)
    const QString nativeTarget = QDir::toNativeSeparators(target);
    const QString nativePath = QDir::toNativeSeparators(path);
    const DWORD flags = (directory ? SYMBOLIC_LINK_FLAG_DIRECTORY : 0) | 0x2;
    if (CreateSymbolicLinkW(reinterpret_cast<LPCWSTR>(nativePath.utf16()),
                            reinterpret_cast<LPCWSTR>(nativeTarget.utf16()), flags) != 0) return true;
    if (!directory) return false;
    QProcess junction;
    junction.start(qEnvironmentVariable("ComSpec", QStringLiteral("cmd.exe")),
                   {QStringLiteral("/c"), QStringLiteral("mklink"), QStringLiteral("/J"), nativePath, nativeTarget});
    if (!junction.waitForStarted(5000)) return false;
    if (!junction.waitForFinished(5000)) {
        junction.kill();
        junction.waitForFinished(5000);
        return false;
    }
    return junction.exitStatus() == QProcess::NormalExit && junction.exitCode() == 0;
#else
    Q_UNUSED(directory);
    return QFile::link(target, path);
#endif
}

void removeLink(const QString& path, bool directory)
{
#if defined(Q_OS_WIN)
    if (directory) {
        const QString nativePath = QDir::toNativeSeparators(path);
        if (!RemoveDirectoryW(reinterpret_cast<LPCWSTR>(nativePath.utf16())))
            qFatal("Fixture directory link removal failed");
        return;
    }
#else
    Q_UNUSED(directory);
#endif
    if (!QFile::remove(path)) qFatal("Fixture link removal failed");
}

class Client {
public:
    QProcess process;
    QByteArray pending;
    QString failure;
    int nextId = 1;

    Client(const QString& executable, const QString& root)
    {
        process.setProgram(executable);
        process.setArguments({QStringLiteral("--root"), root});
        process.setProcessChannelMode(QProcess::SeparateChannels);
        process.start();
        if (!process.waitForStarted(5000)) failure = process.errorString();
    }

    ~Client()
    {
        process.closeWriteChannel();
        if (!process.waitForFinished(1000)) {
            process.kill();
            process.waitForFinished(5000);
        }
    }

    bool write(const QByteArray& data)
    {
        if (process.state() != QProcess::Running || process.write(data) != data.size()) {
            failure = QStringLiteral("server input write failed");
            return false;
        }
        QElapsedTimer timer;
        timer.start();
        while (process.bytesToWrite() && timer.elapsed() < 5000) {
            if (!process.waitForBytesWritten(100)) {
                if (process.state() != QProcess::Running) break;
            }
        }
        return process.bytesToWrite() == 0;
    }

    QJsonObject read(int timeout = 5000)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < timeout) {
            pending += process.readAllStandardOutput();
            const qsizetype newline = pending.indexOf('\n');
            if (newline >= 0) {
                if (newline > 2 * 1024 * 1024) {
                    failure = QStringLiteral("unbounded response");
                    return {};
                }
                const QByteArray line = pending.left(newline);
                pending.remove(0, newline + 1);
                QJsonParseError error;
                const QJsonDocument document = QJsonDocument::fromJson(line, &error);
                if (error.error != QJsonParseError::NoError || !document.isObject()) {
                    failure = QStringLiteral("stdout is not a JSON object: ") + QString::fromUtf8(line.left(200));
                    return {};
                }
                const QJsonObject object = document.object();
                if (object.value(QStringLiteral("jsonrpc")) != QStringLiteral("2.0"))
                    failure = QStringLiteral("missing JSON-RPC version");
                return object;
            }
            if (pending.size() > 2 * 1024 * 1024) {
                failure = QStringLiteral("unbounded response");
                return {};
            }
            if (process.state() == QProcess::NotRunning) break;
            process.waitForReadyRead(100);
        }
        failure = QStringLiteral("missing newline-delimited JSON response; stderr: ") +
                  QString::fromUtf8(process.readAllStandardError());
        return {};
    }

    QJsonObject request(const QString& method, const QJsonObject& params = {})
    {
        const int id = nextId++;
        const QJsonObject object{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                 {QStringLiteral("id"), id},
                                 {QStringLiteral("method"), method},
                                 {QStringLiteral("params"), params}};
        if (!write(QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n')) return {};
        const QJsonObject response = read();
        if (response.value(QStringLiteral("id")) != id)
            failure = QStringLiteral("unexpected response id or notification reply");
        return response;
    }

    void notify(const QString& method)
    {
        write(QJsonDocument(QJsonObject{{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
                                       {QStringLiteral("method"), method}})
                  .toJson(QJsonDocument::Compact) + '\n');
    }

    QJsonObject initialize(const QString& version = QStringLiteral("2025-11-25"))
    {
        const QJsonObject response = request(QStringLiteral("initialize"),
            {{QStringLiteral("protocolVersion"), version},
             {QStringLiteral("capabilities"), QJsonObject{}},
             {QStringLiteral("clientInfo"), QJsonObject{{QStringLiteral("name"), QStringLiteral("ua-mcp-test")},
                                                        {QStringLiteral("version"), QStringLiteral("1.0")}}}});
        notify(QStringLiteral("notifications/initialized"));
        return response;
    }

    QJsonObject tool(const QString& name, const QJsonObject& arguments = {})
    {
        return request(QStringLiteral("tools/call"),
                       {{QStringLiteral("name"), name}, {QStringLiteral("arguments"), arguments}});
    }
};

QJsonObject result(const QJsonObject& response)
{
    return response.value(QStringLiteral("result")).toObject();
}

QJsonObject structured(const QJsonObject& response)
{
    return result(response).value(QStringLiteral("structuredContent")).toObject();
}

bool toolError(const QJsonObject& response)
{
    return result(response).value(QStringLiteral("isError")).toBool() ||
           response.value(QStringLiteral("error")).isObject();
}

int errorCode(const QJsonObject& response)
{
    return response.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toInt();
}

bool containsAsset(const QJsonArray& assets, const QString& path)
{
    for (const QJsonValue& value : assets) {
        if (value.toString() == path) return true;
    }
    return false;
}

bool rejectedRoot(const QString& executable, const QString& root)
{
    QProcess process;
    process.start(executable, {QStringLiteral("--root"), root});
    if (!process.waitForStarted(5000)) return true;
    process.closeWriteChannel();
    if (!process.waitForFinished(5000)) {
        process.kill();
        process.waitForFinished(5000);
        return false;
    }
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() != 0;
}

} // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QString executable;
    if (argc > 1) executable = QString::fromLocal8Bit(argv[1]);
#if defined(UA_MCP_EXECUTABLE)
    else executable = QString::fromUtf8(UA_MCP_EXECUTABLE);
#else
    else executable = QCoreApplication::applicationDirPath() + QStringLiteral("/ua-skills-mcp");
#endif
#if defined(Q_OS_WIN)
    if (!executable.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) executable += QStringLiteral(".exe");
#endif
    if (!QFileInfo(executable).isExecutable()) qFatal("MCP executable is missing");
    QTemporaryDir fixture(QFileInfo(QDir::tempPath()).canonicalFilePath() + QStringLiteral("/ua-mcp-XXXXXX"));
    if (!fixture.isValid()) qFatal("Temporary fixture setup failed");
    const QString root = fixture.path() + QStringLiteral("/skills");
    const QByteArray skill = "---\nname: fixture\ndescription: Fixture description\n---\n# Fixture\nBody preserved.\n";
    const QByteArray reference = QByteArray("first: ") + QByteArray::fromHex("e4bda0e5a5bd") +
                                 "\nsecond: " + QByteArray::fromHex("f09f9880") + "\n";
    putFile(root + "/fixture/SKILL.md", skill);
    putFile(root + "/fixture/references/nested/guide.md", reference);
    putFile(root + "/fixture/assets/picture.bin", QByteArray::fromHex("0001fffe8000"));
    putFile(root + "/fixture/.secret", "hidden");
    putFile(root + "/fixture/.hidden/guide.md", "hidden nested");
    putFile(root + "/zeta/SKILL.md", "---\nname: zeta\ndescription: Second fixture\n---\nZeta\n");
    putFile(root + "/missing-metadata/SKILL.md", "# Missing frontmatter\n");
    putFile(root + "/invalid-utf8/SKILL.md", skill + QByteArray::fromHex("c080"));
    putFile(root + "/wrong-name/SKILL.md", skill);
    const QString outside = fixture.path() + "/outside";
    putFile(outside + "/private.md", "outside sentinel");
    putFile(outside + "/SKILL.md", "---\nname: linked\ndescription: Outside fixture\n---\nOutside\n");

    const CliResult version = runCli(executable, {QStringLiteral("--version")});
    check(version.normalExit && version.exitCode == 0 && version.out.startsWith("ua-skills-mcp ") &&
          version.out.endsWith('\n') && version.out.count('\n') == 1 && version.err.isEmpty(),
          "--version exits normally without a console and writes version only to stdout");
    const CliResult help = runCli(executable, {QStringLiteral("--help")});
    check(help.normalExit && help.exitCode == 0 && help.out.contains("Usage:") &&
          help.out.contains("--root") && help.err.isEmpty(),
          "--help exits normally without a console and writes usage only to stdout");
    const CliResult invalidOption = runCli(executable, {QStringLiteral("--ua-invalid-option")});
    check(invalidOption.normalExit && invalidOption.exitCode == 2 && invalidOption.out.isEmpty() &&
          !invalidOption.err.trimmed().isEmpty() && invalidOption.err.endsWith('\n'),
          "invalid option exits 2 without a console and writes diagnostics only to stderr");
    const CliResult missingRoot = runCli(executable, {QStringLiteral("--root")});
    check(missingRoot.normalExit && missingRoot.exitCode == 2 && missingRoot.out.isEmpty() &&
          !missingRoot.err.trimmed().isEmpty() && missingRoot.err.endsWith('\n'),
          "--root without a value exits 2 and writes diagnostics only to stderr");
    check(rejectedRoot(executable, QStringLiteral("relative/skills")), "relative --root is rejected");
    check(rejectedRoot(executable, fixture.path() + "/absent"), "missing --root is rejected");

    {
        Client client(executable, root);
        check(client.failure.isEmpty(), "MCP process starts");
        check(errorCode(client.request(QStringLiteral("tools/list"))) == -32002,
              "tool discovery requires initialization");
        const QJsonObject initialized = result(client.initialize());
        check(initialized.value(QStringLiteral("protocolVersion")) == QStringLiteral("2025-11-25") &&
              initialized.value(QStringLiteral("capabilities")).toObject().contains(QStringLiteral("tools")) &&
              initialized.value(QStringLiteral("serverInfo")).isObject(), "initialize negotiates protocol and read-only tools");
        const QJsonObject pong = client.request(QStringLiteral("ping"));
        check(pong.value(QStringLiteral("result")).isObject() && result(pong).isEmpty() && client.failure.isEmpty(),
              "initialized notification has no reply and ping succeeds");

        const QJsonArray tools = result(client.request(QStringLiteral("tools/list"))).value(QStringLiteral("tools")).toArray();
        QSet<QString> names;
        bool schemas = true;
        bool readOnly = true;
        for (const QJsonValue& value : tools) {
            const QJsonObject tool = value.toObject();
            names.insert(tool.value(QStringLiteral("name")).toString());
            schemas = schemas && tool.value(QStringLiteral("inputSchema")).toObject().value(QStringLiteral("type")) == QStringLiteral("object");
            const QJsonObject annotations = tool.value(QStringLiteral("annotations")).toObject();
            readOnly = readOnly && annotations.value(QStringLiteral("readOnlyHint")).toBool() &&
                       annotations.contains(QStringLiteral("destructiveHint")) &&
                       !annotations.value(QStringLiteral("destructiveHint")).toBool(true);
        }
        check(names == QSet<QString>{QStringLiteral("list_skills"), QStringLiteral("read_skill"), QStringLiteral("read_asset")} && schemas && readOnly,
              "tools/list exposes exactly three read-only tools and schemas");

        const QJsonObject listedResponse = client.tool(QStringLiteral("list_skills"));
        const QJsonArray listed = structured(listedResponse).value(QStringLiteral("skills")).toArray();
        check(listed.size() == 2 && listed.at(0).toObject().value(QStringLiteral("name")) == QStringLiteral("fixture") &&
              listed.at(1).toObject().value(QStringLiteral("name")) == QStringLiteral("zeta"),
              "list_skills returns canonical valid metadata and skips malformed or mismatched skills");
        const QJsonObject firstSkill = listed.isEmpty() ? QJsonObject{} : listed.at(0).toObject();
        const QJsonArray assets = firstSkill.value(QStringLiteral("assets")).toArray();
        check(containsAsset(assets, QStringLiteral("references/nested/guide.md")) &&
              !containsAsset(assets, QStringLiteral(".secret")) && !containsAsset(assets, QStringLiteral(".hidden/guide.md")),
              "list_skills discovers nested assets but excludes hidden paths");
        const QJsonArray content = result(listedResponse).value(QStringLiteral("content")).toArray();
        check(!content.isEmpty() && content.at(0).toObject().value(QStringLiteral("type")) == QStringLiteral("text") &&
              QJsonDocument::fromJson(content.at(0).toObject().value(QStringLiteral("text")).toString().toUtf8()).object() == structured(listedResponse),
              "tool results include matching JSON text and structuredContent");
        const QJsonObject page = structured(client.tool(QStringLiteral("list_skills"), {{QStringLiteral("limit"), 1}}));
        const qint64 nextOffset = page.value(QStringLiteral("nextOffset")).toInteger(-1);
        const QJsonObject nextPage = structured(client.tool(QStringLiteral("list_skills"),
            {{QStringLiteral("limit"), 1}, {QStringLiteral("offset"), nextOffset}}));
        check(page.value(QStringLiteral("skills")).toArray().size() == 1 && nextOffset > 0 &&
              nextPage.value(QStringLiteral("skills")).toArray().size() == 1 &&
              page.value(QStringLiteral("skills")).toArray().at(0) != nextPage.value(QStringLiteral("skills")).toArray().at(0),
              "list_skills offset pagination advances");

        const QJsonObject read = structured(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), QStringLiteral("fixture")}}));
        check(read.value(QStringLiteral("text")).toString().toUtf8() == skill &&
              read.value(QStringLiteral("name")) == QStringLiteral("fixture") &&
              read.value(QStringLiteral("path")) == QStringLiteral("SKILL.md") &&
              read.value(QStringLiteral("total")).toInteger() == skill.size(), "read_skill returns exact document with its identity");
        const QJsonObject referenceRead = structured(client.tool(QStringLiteral("read_asset"),
            {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("path"), QStringLiteral("references/nested/guide.md")}}));
        check(referenceRead.value(QStringLiteral("text")).toString().toUtf8() == reference &&
              referenceRead.value(QStringLiteral("total")).toInteger() == QString::fromUtf8(reference).toUcs4().size(),
              "read_asset preserves nested UTF-8 reference");
        check(toolError(client.tool(QStringLiteral("read_asset"),
            {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("path"), QStringLiteral("assets/picture.bin")}})),
              "non-UTF-8 binary assets report a tool error");

        QByteArray joined;
        qint64 offset = 0;
        bool pagesValid = true;
        for (int i = 0; i < 100; ++i) {
            const QJsonObject part = structured(client.tool(QStringLiteral("read_asset"),
                {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("path"), QStringLiteral("references/nested/guide.md")},
                 {QStringLiteral("offset"), offset}, {QStringLiteral("limit"), 8}}));
            if (part.value(QStringLiteral("offset")).toInteger(-1) != offset || part.isEmpty() ||
                part.value(QStringLiteral("text")).toString().toUcs4().size() > 8) {
                pagesValid = false;
                break;
            }
            joined += part.value(QStringLiteral("text")).toString().toUtf8();
            if (part.value(QStringLiteral("nextOffset")).isNull()) break;
            const qint64 next = part.value(QStringLiteral("nextOffset")).toInteger(-1);
            if (next <= offset) { pagesValid = false; break; }
            offset = next;
        }
        check(pagesValid && joined == reference, "UTF-8 pagination preserves scalar boundaries and makes progress");

        const QStringList invalidNames{QStringLiteral("../fixture"), QStringLiteral("Fixture"), QStringLiteral("fixture/name"),
                                       QStringLiteral("fixture\\name"), QStringLiteral("."), QString(65, QLatin1Char('a')), QString()};
        bool namesRejected = true;
        for (const QString& name : invalidNames)
            namesRejected = toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), name}})) && namesRejected;
        check(namesRejected, "read_skill rejects noncanonical names and traversal");
        const QStringList invalidPaths{QStringLiteral("../outside/private.md"), QStringLiteral("references/../../private.md"),
            QStringLiteral("/private.md"), QStringLiteral("C:/private.md"), QStringLiteral("C:private.md"),
            QStringLiteral("references\\nested\\guide.md"), QStringLiteral("//server/share/file"),
            QStringLiteral(".secret"), QStringLiteral(".hidden/guide.md"), QStringLiteral("references//nested/guide.md"),
            QStringLiteral("references/./nested/guide.md"), QStringLiteral("references/nested/guide.md:stream"),
            QStringLiteral("references/nested/guide.md/"), QString()};
        bool pathsRejected = true;
        for (const QString& path : invalidPaths)
            pathsRejected = toolError(client.tool(QStringLiteral("read_asset"),
                {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("path"), path}})) && pathsRejected;
        check(pathsRejected, "read_asset rejects traversal, absolute, hidden, alternate stream and malformed paths");
        bool invalidSkillsRejected = true;
        for (const QString& name : {QStringLiteral("missing-metadata"), QStringLiteral("invalid-utf8"), QStringLiteral("wrong-name"), QStringLiteral("unknown")})
            invalidSkillsRejected = toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), name}})) && invalidSkillsRejected;
        check(invalidSkillsRejected, "read_skill rejects missing, invalid UTF-8 and inconsistent metadata");
        check(toolError(client.tool(QStringLiteral("read_asset"), {{QStringLiteral("name"), QStringLiteral("fixture")},
              {QStringLiteral("path"), QStringLiteral("missing.md")}})), "missing asset reports tool error");
        check(toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), 7}})) &&
              toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("offset"), -1}})) &&
              toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("offset"), 0.5}})) &&
              toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), QStringLiteral("fixture")}, {QStringLiteral("limit"), "8"}})) &&
              toolError(client.tool(QStringLiteral("list_skills"), {{QStringLiteral("limit"), 101}})) &&
              toolError(client.tool(QStringLiteral("list_skills"), {{QStringLiteral("offset"), "1"}})),
              "tool argument types and ranges are validated");
        check(errorCode(client.tool(QStringLiteral("execute"))) == -32602, "unknown tool returns invalid params");
        check(errorCode(client.request(QStringLiteral("unknown/method"))) == -32601, "unknown method returns method not found");
        client.write("{broken json}\n");
        check(errorCode(client.read()) == -32700, "malformed JSON returns parse error");
        client.write("{\"jsonrpc\":\"2.0\",\"id\":{},\"method\":\"ping\"}\n");
        check(errorCode(client.read()) == -32600, "invalid request id returns invalid request");
        client.write("[]\n");
        check(errorCode(client.read()) == -32600, "JSON-RPC batch is rejected");
        client.write("{\"jsonrpc\":\"2.0\",\"id\":\"string-id\",\"method\":\"ping\"}\n");
        const QJsonObject stringPong = client.read();
        check(stringPong.value(QStringLiteral("id")) == QStringLiteral("string-id") &&
              stringPong.value(QStringLiteral("result")).isObject(), "string request ids are preserved");
        client.write(QByteArray("{\"x\":\"") + QByteArray::fromHex("c080") + "\"}\n");
        check(errorCode(client.read()) == -32700, "invalid UTF-8 JSON is rejected");
        const QJsonObject recoveredPong = client.request(QStringLiteral("ping"));
        check(recoveredPong.value(QStringLiteral("result")).isObject() && result(recoveredPong).isEmpty() && client.failure.isEmpty(),
              "protocol remains synchronized after malformed requests");

        putFile(root + "/fixture/assets/large.bin", QByteArray(4 * 1024 * 1024 + 1, 'x'));
        check(toolError(client.tool(QStringLiteral("read_asset"), {{QStringLiteral("name"), QStringLiteral("fixture")},
              {QStringLiteral("path"), QStringLiteral("assets/large.bin")}})), "oversize files are rejected before read");
        check(toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), QStringLiteral("fixture")},
              {QStringLiteral("limit"), 32769}})), "read response page limit is bounded");

        const bool directoryLink = linkPath(outside, root + "/linked", true);
        const bool assetLink = linkPath(outside + "/private.md", root + "/fixture/references/linked.md", false);
        const bool nestedLink = linkPath(outside, root + "/fixture/references/linked-dir", true);
        if (directoryLink || assetLink || nestedLink) {
            bool linksRejected = true;
            if (directoryLink)
                linksRejected = toolError(client.tool(QStringLiteral("read_skill"), {{QStringLiteral("name"), QStringLiteral("linked")}})) && linksRejected;
            if (assetLink)
                linksRejected = toolError(client.tool(QStringLiteral("read_asset"), {{QStringLiteral("name"), QStringLiteral("fixture")},
                    {QStringLiteral("path"), QStringLiteral("references/linked.md")}})) && linksRejected;
            if (nestedLink)
                linksRejected = toolError(client.tool(QStringLiteral("read_asset"), {{QStringLiteral("name"), QStringLiteral("fixture")},
                    {QStringLiteral("path"), QStringLiteral("references/linked-dir/private.md")}})) && linksRejected;
            check(linksRejected, "symlink or junction boundaries reject linked skills, files and intermediate directories");
        } else {
            QTextStream(stdout) << "SKIP symlink fixture creation is unavailable on this host\n";
        }
        const bool rootLink = linkPath(root, fixture.path() + "/linked-root", true);
        if (rootLink) check(rejectedRoot(executable, fixture.path() + "/linked-root"), "symlink --root is rejected");
        const bool ancestorLink = linkPath(fixture.path(), fixture.path() + "/linked-parent", true);
        if (ancestorLink)
            check(rejectedRoot(executable, fixture.path() + "/linked-parent/skills"), "symlink ancestor of --root is rejected");
        if (rootLink) removeLink(fixture.path() + "/linked-root", true);
        if (ancestorLink) removeLink(fixture.path() + "/linked-parent", true);
        if (directoryLink) removeLink(root + "/linked", true);
        if (assetLink) removeLink(root + "/fixture/references/linked.md", false);
        if (nestedLink) removeLink(root + "/fixture/references/linked-dir", true);
    }
    {
        Client earlier(executable, root);
        const QJsonObject response = result(earlier.initialize(QStringLiteral("2024-11-05")));
        check(!response.value(QStringLiteral("protocolVersion")).toString().isEmpty() &&
              !earlier.request(QStringLiteral("tools/list")).contains(QStringLiteral("error")),
              "earlier MCP protocol clients can initialize and discover tools");
    }
    {
        Client oversized(executable, root);
        oversized.initialize();
        oversized.write(QByteArray(4 * 1024 * 1024, ' ') + "{}\n");
        const QJsonObject response = oversized.read();
        check(response.contains(QStringLiteral("error")) || oversized.process.state() == QProcess::NotRunning,
              "oversize request is rejected or closes the transport");
    }
    QTextStream(stdout) << checks << " checks, " << failures << " failures\n";
    return failures == 0 ? 0 : 1;
}
