#include "skills_mcp_server.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>

#include <cstdio>

#if defined(Q_OS_WIN)
#include <fcntl.h>
#include <io.h>
#endif

#ifndef UA_VERSION_STR
#define UA_VERSION_STR "0.0.0"
#endif

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("ua-skills-mcp"));
    QCoreApplication::setApplicationVersion(QStringLiteral(UA_VERSION_STR));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Read-only shared skills MCP server"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("root"), QStringLiteral("Absolute shared skills directory"),
                      QStringLiteral("directory"), QDir::homePath() + QStringLiteral("/.agents/skills")});
    const auto print = [](FILE* stream, const QByteArray& message) {
        return std::fwrite(message.constData(), 1, size_t(message.size()), stream) ==
            size_t(message.size()) && std::fflush(stream) == 0;
    };
    if (!parser.parse(app.arguments())) {
        print(stderr, parser.errorText().toUtf8() + '\n');
        return 2;
    }
    if (parser.isSet(QStringLiteral("help")) || parser.optionNames().contains(QStringLiteral("help-all")))
        return print(stdout, parser.helpText().toUtf8()) ? 0 : 1;
    if (parser.isSet(QStringLiteral("version")))
        return print(stdout, (QCoreApplication::applicationName() + QLatin1Char(' ') +
            QCoreApplication::applicationVersion() + QLatin1Char('\n')).toUtf8()) ? 0 : 1;
    if (!parser.positionalArguments().isEmpty()) {
        std::fputs("Unexpected positional argument\n", stderr);
        return 2;
    }

    ua::mcp::SkillsMcpServer server(parser.value(QStringLiteral("root")));
    QString error;
    if (!server.validateRoot(&error)) {
        const QByteArray message = error.toUtf8() + '\n';
        std::fwrite(message.constData(), 1, size_t(message.size()), stderr);
        return 2;
    }

#if defined(Q_OS_WIN)
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    QByteArray line;
    line.reserve(int(ua::mcp::SkillsMcpServer::MaxRequestBytes + 1));
    bool oversized = false;
    const auto reply = [&]() {
        const QByteArray response = server.processLine(line);
        if (!response.isEmpty() &&
            (std::fwrite(response.constData(), 1, size_t(response.size()), stdout) !=
                 size_t(response.size()) || std::fflush(stdout) != 0)) return false;
        line.clear();
        oversized = false;
        return true;
    };
    for (int c; (c = std::fgetc(stdin)) != EOF;) {
        if (c == '\n') {
            if (!reply()) return 1;
        } else if (!oversized) {
            line.append(char(c));
            oversized = line.size() > ua::mcp::SkillsMcpServer::MaxRequestBytes;
        }
    }
    // A final unterminated line is not a newline-framed MCP message.
    return std::ferror(stdin) ? 1 : 0;
}
