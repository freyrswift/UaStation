#include "skills_mcp_server.h"

#include "core/skill_catalog.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QStringDecoder>
#include <QVector>

#include <algorithm>
#include <cmath>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef UA_VERSION_STR
#define UA_VERSION_STR "0.0.0"
#endif

namespace ua::mcp {
namespace {

constexpr int MaxEntries = 16384;
constexpr int MaxAssets = 4096;
constexpr int MaxDepth = 16;
constexpr int DefaultPageSize = 50;
constexpr int MaxPageSize = 100;
constexpr int DefaultReadSize = 16384;
constexpr int MaxReadSize = 32768;

bool fail(QString* error, const char* message)
{
    if (error) *error = QString::fromLatin1(message);
    return false;
}

bool canonicalName(const QString& name)
{
    static const QRegularExpression pattern(QStringLiteral("^[a-z0-9]+(?:-[a-z0-9]+)*$"));
    return name.size() <= 64 && pattern.match(name).hasMatch();
}

bool safeComponent(const QString& part)
{
    if (part.isEmpty() || part == QLatin1String(".") || part == QLatin1String("..") ||
        part.size() > 255 || part.endsWith('.') || part.endsWith(' ')) return false;
    for (const QChar c : part) {
        if (c.unicode() < 0x20 || c.unicode() == 0x7f ||
            QStringLiteral("/\\:<>\"|?*").contains(c)) return false;
    }
    static const QRegularExpression reserved(QStringLiteral("^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])$"),
                                              QRegularExpression::CaseInsensitiveOption);
    return !reserved.match(part.section('.', 0, 0)).hasMatch();
}

bool safeRelative(const QString& path)
{
    if (path.isEmpty() || path.size() > 2048 || QDir::isAbsolutePath(path)) return false;
    const auto parts = path.split('/');
    if (parts.size() > MaxDepth) return false;
    return std::all_of(parts.cbegin(), parts.cend(), [](const QString& part) {
        return !part.startsWith('.') && safeComponent(part);
    });
}

bool plain(const QFileInfo& info)
{
    if (!info.exists() || info.isSymLink() || info.isJunction() || info.isAlias()) return false;
#if defined(Q_OS_WIN)
    // Reject every reparse tag, not only the symbolic-link and mount-point tags.
    const QString path = QDir::toNativeSeparators(info.absoluteFilePath());
    const DWORD attributes = GetFileAttributesW(reinterpret_cast<LPCWSTR>(path.utf16()));
    if (attributes == INVALID_FILE_ATTRIBUTES || attributes & FILE_ATTRIBUTE_REPARSE_POINT) return false;
#endif
    return true;
}

bool plainAbsolute(const QString& path, bool directory, QString* error)
{
    if (path.isEmpty() || path.size() > 4096 || path.contains(QChar::Null) ||
        path.contains('\\') || !QDir::isAbsolutePath(path) || path.startsWith(QLatin1String("//")))
        return fail(error, "Root must be an absolute local directory");
    const auto components = path.split('/');
    for (qsizetype i = 0; i < components.size(); ++i) {
        const auto& component = components.at(i);
        if (component == QLatin1String(".") || component == QLatin1String(".."))
            return fail(error, "Root must not contain dot segments");
        if (!component.isEmpty() && !(i == 0 && component.size() == 2 && component[1] == ':') &&
            !safeComponent(component))
            return fail(error, "Invalid root path component");
    }
    QFileInfo info(path);
    if (directory && !info.isDir()) return fail(error, "Skills root is not a directory");
    for (;;) {
        if (!plain(info)) return fail(error, "Linked or missing path boundary");
        if (info.isRoot()) return true;
        const auto parent = info.absolutePath();
        if (parent == info.absoluteFilePath()) return fail(error, "Invalid path boundary");
        info.setFile(parent);
    }
}

bool safePath(const QString& root, const QString& relative, bool directory, QString* error)
{
    if (!safeRelative(relative)) return fail(error, "Invalid relative path");
    if (!plainAbsolute(root, true, error)) return false;
    QString path = root;
    const auto parts = relative.split('/');
    for (qsizetype i = 0; i < parts.size(); ++i) {
        path = QDir(path).filePath(parts.at(i));
        const QFileInfo info(path);
        if (!plain(info)) return fail(error, "Linked or missing path boundary");
        const bool expectDirectory = i + 1 < parts.size() || directory;
        if (expectDirectory ? !info.isDir() : !info.isFile())
            return fail(error, "Unexpected file type");
    }
    return true;
}

// Open each directory without following links. Keeping the handles open also
// prevents ancestor replacement on Windows while the final file is read.
bool readConfined(const QString& root, const QString& relative, QByteArray* data, QString* error)
{
    if (!safePath(root, relative, false, error)) return false;
    const QString absolute = QDir(root).filePath(relative);
    QStringList paths;
    QFileInfo info(absolute);
    for (;;) {
        paths.prepend(info.absoluteFilePath());
        if (info.isRoot()) break;
        info.setFile(info.absolutePath());
    }
    QFile file;
#if defined(Q_OS_WIN)
    struct Handles {
        QVector<HANDLE> values;
        ~Handles() { for (HANDLE handle : values) CloseHandle(handle); }
    } handles;
    for (qsizetype i = 0; i < paths.size(); ++i) {
        const bool last = i + 1 == paths.size();
        const QString native = QDir::toNativeSeparators(paths.at(i));
        HANDLE handle = CreateFileW(reinterpret_cast<LPCWSTR>(native.utf16()),
                                    last ? GENERIC_READ : FILE_READ_ATTRIBUTES,
                                    FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                    FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
        if (handle == INVALID_HANDLE_VALUE) return fail(error, "File is unavailable");
        handles.values.append(handle);
        BY_HANDLE_FILE_INFORMATION details{};
        if (!GetFileInformationByHandle(handle, &details) ||
            details.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT ||
            bool(details.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == last)
            return fail(error, "Linked or invalid file boundary");
        if (last) {
            const quint64 size = (quint64(details.nFileSizeHigh) << 32) | details.nFileSizeLow;
            if (size > quint64(SkillsMcpServer::MaxFileBytes)) return fail(error, "File exceeds size limit");
            data->resize(qsizetype(size));
            DWORD read = 0;
            if (!ReadFile(handle, data->data(), DWORD(size), &read, nullptr) || read != size)
                return fail(error, "File read failed");
            char extra;
            if (!ReadFile(handle, &extra, 1, &read, nullptr) || read != 0)
                return fail(error, "File changed during read");
        }
    }
#else
    struct Descriptor {
        int value = -1;
        ~Descriptor() { if (value >= 0) ::close(value); }
    } current;
    current.value = ::open("/", O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (current.value < 0) return fail(error, "Root is unavailable");
    const auto parts = absolute.split('/', Qt::SkipEmptyParts);
    for (qsizetype i = 0; i < parts.size(); ++i) {
        const bool last = i + 1 == parts.size();
        const auto component = QFile::encodeName(parts.at(i));
        const int next = ::openat(current.value, component.constData(),
            O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK | (last ? 0 : O_DIRECTORY));
        if (next < 0) return fail(error, "File is unavailable");
        ::close(current.value);
        current.value = next;
        struct stat details{};
        if (::fstat(next, &details) != 0 || (last ? !S_ISREG(details.st_mode) : !S_ISDIR(details.st_mode)))
            return fail(error, "Unexpected file type");
        if (last && (details.st_size < 0 || details.st_size > SkillsMcpServer::MaxFileBytes))
            return fail(error, "File exceeds size limit");
    }
    if (!file.open(current.value, QIODevice::ReadOnly, QFileDevice::DontCloseHandle))
        return fail(error, "File is unavailable");
    *data = file.read(SkillsMcpServer::MaxFileBytes + 1);
    if (file.error() != QFileDevice::NoError || data->size() > SkillsMcpServer::MaxFileBytes)
        return fail(error, "File read exceeds size limit or failed");
    file.close();
#endif
    return safePath(root, relative, false, error);
}

bool decode(const QByteArray& data, QString* result)
{
    if (!data.isValidUtf8()) return false;
    QStringDecoder decoder(QStringDecoder::Utf8);
    *result = decoder(data);
    return !decoder.hasError() && !result->contains(QChar::Null);
}

bool metadata(const QString& root, const QString& name, ua::core::SkillMetadata* result, QString* error)
{
    if (!canonicalName(name)) return fail(error, "Invalid skill name");
    QByteArray bytes;
    if (!readConfined(root, name + QStringLiteral("/SKILL.md"), &bytes, error)) return false;
    if (!ua::core::SkillCatalog::parse(bytes, result, error)) return false;
    if (result->name != name) return fail(error, "Skill metadata name must match its directory");
    return true;
}

QJsonObject rpcError(const QJsonValue& id, int code, const QString& message)
{
    return {{"jsonrpc", "2.0"}, {"id", id}, {"error", QJsonObject{{"code", code}, {"message", message}}}};
}

QJsonObject toolError(const QString& message)
{
    return {{"isError", true}, {"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", message}}}}};
}

QJsonObject toolResult(const QJsonObject& data)
{
    return {{"content", QJsonArray{QJsonObject{{"type", "text"}, {"text", QString::fromUtf8(
        QJsonDocument(data).toJson(QJsonDocument::Compact))}}}}, {"structuredContent", data}};
}

QByteArray encodeResponse(const QJsonObject& response)
{
    QByteArray bytes = QJsonDocument(response).toJson(QJsonDocument::Compact);
    if (bytes.size() > SkillsMcpServer::MaxResponseBytes) {
        bytes = QJsonDocument(rpcError(response.value("id"), -32603,
            QStringLiteral("Response exceeds size limit"))).toJson(QJsonDocument::Compact);
    }
    return bytes + '\n';
}

bool onlyKeys(const QJsonObject& object, const QStringList& allowed)
{
    for (auto it = object.begin(); it != object.end(); ++it)
        if (!allowed.contains(it.key())) return false;
    return true;
}

bool integer(const QJsonObject& arguments, const char* key, int fallback, int minimum, int maximum, int* result)
{
    const auto value = arguments.value(QLatin1String(key));
    if (value.isUndefined()) { *result = fallback; return true; }
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < minimum || number > maximum || std::floor(number) != number)
        return false;
    *result = int(number);
    return true;
}

QJsonObject schema(const QJsonObject& properties, const QJsonArray& required = {})
{
    return {{"type", "object"}, {"properties", properties}, {"required", required}, {"additionalProperties", false}};
}

QJsonObject numberSchema(int minimum, int maximum, int fallback)
{
    return {{"type", "integer"}, {"minimum", minimum}, {"maximum", maximum}, {"default", fallback}};
}

QJsonObject tool(const char* name, const char* description, const QJsonObject& input)
{
    return {{"name", QLatin1String(name)}, {"description", QLatin1String(description)}, {"inputSchema", input},
            {"annotations", QJsonObject{{"readOnlyHint", true}, {"destructiveHint", false},
                                        {"idempotentHint", true}, {"openWorldHint", false}}}};
}

QJsonArray tools()
{
    const QJsonObject name{{"type", "string"}, {"pattern", "^[a-z0-9]+(-[a-z0-9]+)*$"}, {"maxLength", 64}};
    const QJsonObject offset = numberSchema(0, int(SkillsMcpServer::MaxFileBytes), 0);
    const QJsonObject length = numberSchema(1, MaxReadSize, DefaultReadSize);
    return {
        tool("list_skills", "List skill metadata and relative asset paths with offset/limit pagination.",
             schema({{"offset", numberSchema(0, MaxEntries, 0)},
                     {"limit", numberSchema(1, MaxPageSize, DefaultPageSize)}})),
        tool("read_skill", "Read a UTF-8 skill document, paginated by Unicode code points.",
             schema({{"name", name}, {"offset", offset}, {"limit", length}}, {"name"})),
        tool("read_asset", "Read a UTF-8 companion file within a skill, paginated by Unicode code points.",
             schema({{"name", name}, {"path", QJsonObject{{"type", "string"}, {"maxLength", 2048}}},
                     {"offset", offset}, {"limit", length}}, {"name", "path"}))
    };
}

bool assets(const QString& root, const QString& name, QJsonArray* result, QString* error)
{
    if (!safePath(root, name, true, error)) return false;
    const QString base = QDir(root).filePath(name);
    QStringList paths;
    QStringList pending{QString()};
    int visited = 0;
    while (!pending.isEmpty()) {
        const auto subdir = pending.takeLast();
        const auto relative = subdir.isEmpty() ? name : name + '/' + subdir;
        if (!safePath(root, relative, true, error)) return false;
        const auto path = subdir.isEmpty() ? base : QDir(base).filePath(subdir);
        QDirIterator it(path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
        while (it.hasNext()) {
            it.next();
            if (++visited > MaxAssets) return fail(error, "Skill asset count exceeds limit");
            const QFileInfo info = it.fileInfo();
            const auto asset = subdir.isEmpty() ? info.fileName() : subdir + '/' + info.fileName();
            if (!safeRelative(asset) || !plain(info)) continue;
            if (info.isDir()) {
                if (asset.count('/') + 1 >= MaxDepth) return fail(error, "Skill asset depth exceeds limit");
                pending.append(asset);
            } else if (info.isFile() && asset != QLatin1String("SKILL.md") &&
                       info.size() <= SkillsMcpServer::MaxFileBytes) {
                if (!safePath(root, name + '/' + asset, false, error)) return false;
                paths.append(asset);
            }
        }
    }
    paths.sort();
    *result = QJsonArray::fromStringList(paths);
    return true;
}

QJsonObject listSkills(const QString& root, const QJsonObject& arguments)
{
    int offset, limit;
    if (!onlyKeys(arguments, {"offset", "limit"}) ||
        !integer(arguments, "offset", 0, 0, MaxEntries, &offset) ||
        !integer(arguments, "limit", DefaultPageSize, 1, MaxPageSize, &limit))
        return toolError(QStringLiteral("Invalid list_skills arguments"));
    QString error;
    if (!plainAbsolute(root, true, &error)) return toolError(error);
    QStringList names;
    QDirIterator entries(root, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    int visited = 0;
    while (entries.hasNext()) {
        entries.next();
        if (++visited > MaxEntries) return toolError(QStringLiteral("Library entry count exceeds limit"));
        const QFileInfo info = entries.fileInfo();
        if (canonicalName(info.fileName()) && plain(info) && info.isDir()) names.append(info.fileName());
    }
    names.sort();
    QJsonArray records;
    int scanned = std::min(offset, int(names.size()));
    while (scanned < names.size() && records.size() < limit) {
        const auto& name = names.at(scanned++);
        ua::core::SkillMetadata details;
        if (!metadata(root, name, &details, &error)) continue;
        QJsonArray files;
        if (!assets(root, name, &files, &error)) return toolError(error);
        const QJsonObject record{{"name", details.name}, {"description", details.description}, {"assets", files}};
        // Text content duplicates structuredContent. Reserve space for JSON
        // escaping, the wrapper and a particularly large individual record.
        QJsonArray candidate = records;
        candidate.append(record);
        if (QJsonDocument(candidate).toJson(QJsonDocument::Compact).size() > 192 * 1024) {
            if (records.isEmpty()) return toolError(QStringLiteral("Skill metadata exceeds response limit"));
            --scanned;
            break;
        }
        records.append(record);
    }
    return toolResult({{"skills", records}, {"offset", offset},
                       {"nextOffset", scanned < names.size() ? QJsonValue(scanned) : QJsonValue(QJsonValue::Null)}});
}

QJsonObject readText(const QString& root, const QJsonObject& arguments, bool asset)
{
    int offset, limit;
    if (!onlyKeys(arguments, asset ? QStringList{"name", "path", "offset", "limit"} :
                                    QStringList{"name", "offset", "limit"}) ||
        !arguments.value("name").isString() ||
        (asset && !arguments.value("path").isString()) ||
        !integer(arguments, "offset", 0, 0, int(SkillsMcpServer::MaxFileBytes), &offset) ||
        !integer(arguments, "limit", DefaultReadSize, 1, MaxReadSize, &limit))
        return toolError(QStringLiteral("Invalid read arguments"));
    const auto name = arguments.value("name").toString();
    const auto path = asset ? arguments.value("path").toString() : QStringLiteral("SKILL.md");
    if (!canonicalName(name) || !safeRelative(path)) return toolError(QStringLiteral("Invalid skill name or asset path"));
    QString error;
    ua::core::SkillMetadata details;
    if (!metadata(root, name, &details, &error)) return toolError(error);
    QByteArray bytes;
    if (!readConfined(root, name + '/' + path, &bytes, &error)) return toolError(error);
    QString text;
    if (!decode(bytes, &text)) return toolError(QStringLiteral("File must contain valid UTF-8 text without NUL"));
    // Unicode code-point offsets never split surrogate pairs or UTF-8 sequences.
    const auto points = text.toStdU32String();
    const auto pointCount = qsizetype(points.size());
    if (offset > pointCount) return toolError(QStringLiteral("Offset exceeds text length"));
    const auto count = std::min(qsizetype(limit), pointCount - offset);
    const auto slice = QString::fromUcs4(points.data() + offset, count);
    const auto next = offset + count;
    return toolResult({{"name", name}, {"path", path}, {"text", slice}, {"offset", offset},
                       {"total", int(pointCount)},
                       {"nextOffset", next < pointCount ? QJsonValue(int(next)) : QJsonValue(QJsonValue::Null)}});
}

} // namespace

SkillsMcpServer::SkillsMcpServer(const QString& root)
    : m_root(QDir::fromNativeSeparators(root))
{
}

bool SkillsMcpServer::validateRoot(QString* error) const
{
    if (error) error->clear();
    return plainAbsolute(m_root, true, error);
}

QByteArray SkillsMcpServer::processLine(const QByteArray& line)
{
    const auto nullId = QJsonValue(QJsonValue::Null);
    if (line.size() > MaxRequestBytes)
        return encodeResponse(rpcError(nullId, -32600, QStringLiteral("Request exceeds size limit")));
    QString decoded;
    if (!decode(line, &decoded))
        return encodeResponse(rpcError(nullId, -32700, QStringLiteral("Invalid UTF-8 JSON")));
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(line, &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return encodeResponse(rpcError(nullId, -32700, QStringLiteral("Parse error")));
    if (!document.isObject())
        return encodeResponse(rpcError(nullId, -32600, QStringLiteral("Expected JSON-RPC object")));
    const auto request = document.object();
    const bool hasId = request.contains("id");
    const auto id = hasId ? request.value("id") : nullId;
    if ((hasId && (!id.isString() && !id.isDouble())) ||
        (id.isString() && id.toString().size() > 256) ||
        (id.isDouble() && (std::floor(id.toDouble()) != id.toDouble() || std::abs(id.toDouble()) > 9007199254740991.0)) ||
        request.value("jsonrpc") != QJsonValue("2.0") || !request.value("method").isString() ||
        request.value("method").toString().isEmpty())
        return encodeResponse(rpcError(nullId, -32600, QStringLiteral("Invalid JSON-RPC request")));
    const auto method = request.value("method").toString();
    const auto parameters = request.value("params");
    if (!parameters.isUndefined() && !parameters.isObject()) {
        return hasId ? encodeResponse(rpcError(id, -32602, QStringLiteral("Expected object params"))) : QByteArray();
    }
    const auto params = parameters.toObject();
    if (!hasId) {
        if (method == QLatin1String("notifications/initialized") && m_initialized) m_ready = true;
        return {};
    }
    const auto result = [&](const QJsonObject& value) {
        return encodeResponse({{"jsonrpc", "2.0"}, {"id", id}, {"result", value}});
    };
    const auto error = [&](int code, const char* message) {
        return encodeResponse(rpcError(id, code, QString::fromLatin1(message)));
    };
    if (method == QLatin1String("ping")) return result({});
    if (method == QLatin1String("initialize")) {
        if (m_initialized) return error(-32600, "Already initialized");
        if (!params.value("protocolVersion").isString() || !params.value("capabilities").isObject() ||
            !params.value("clientInfo").isObject() ||
            !params.value("clientInfo").toObject().value("name").isString() ||
            !params.value("clientInfo").toObject().value("version").isString())
            return error(-32602, "Invalid initialize parameters");
        const auto requested = params.value("protocolVersion").toString();
        static const QStringList supported{"2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05"};
        const auto version = supported.contains(requested) ? requested : supported.first();
        m_initialized = true;
        return result({{"protocolVersion", version}, {"capabilities", QJsonObject{{"tools", QJsonObject{{"listChanged", false}}}}},
                       {"serverInfo", QJsonObject{{"name", "ua-skills-mcp"}, {"version", UA_VERSION_STR}}}});
    }
    if (!m_initialized || !m_ready) return error(-32002, "Server is not initialized");
    if (method == QLatin1String("tools/list")) {
        if (!onlyKeys(params, {"_meta"})) return error(-32602, "Unexpected tools/list parameters");
        return result({{"tools", tools()}});
    }
    if (method == QLatin1String("tools/call")) {
        if (!onlyKeys(params, {"name", "arguments", "_meta"}) || !params.value("name").isString() ||
            (!params.value("arguments").isUndefined() && !params.value("arguments").isObject()))
            return error(-32602, "Invalid tool call");
        const auto name = params.value("name").toString();
        const auto arguments = params.value("arguments").toObject();
        if (name == QLatin1String("list_skills")) return result(listSkills(m_root, arguments));
        if (name == QLatin1String("read_skill")) return result(readText(m_root, arguments, false));
        if (name == QLatin1String("read_asset")) return result(readText(m_root, arguments, true));
        return error(-32602, "Unknown tool");
    }
    return error(-32601, "Method not found");
}

} // namespace ua::mcp
