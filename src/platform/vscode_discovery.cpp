#include "vscode_discovery.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QStandardPaths>

#if defined(Q_OS_WIN)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <shlobj.h>
#endif

namespace ua::platform {

QStringList vscodeRunningExecutables()
{
    QSet<QString> paths;
#if defined(Q_OS_WIN)
    const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return {};
    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Process32FirstW(snapshot, &entry)) {
        do {
            const QString name = QString::fromWCharArray(entry.szExeFile);
            if (name.compare(QLatin1String("Code.exe"), Qt::CaseInsensitive) != 0 &&
                name.compare(QLatin1String("Code - Insiders.exe"), Qt::CaseInsensitive) != 0) continue;
            const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, entry.th32ProcessID);
            if (!process) continue;
            wchar_t path[32768];
            DWORD length = DWORD(sizeof(path) / sizeof(path[0]));
            if (QueryFullProcessImageNameW(process, 0, path, &length))
                paths.insert(QString::fromWCharArray(path, int(length)));
            CloseHandle(process);
        } while (Process32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
#endif
    return paths.values();
}

QStringList vscodeShortcutTargets()
{
    QSet<QString> targets;
#if defined(Q_OS_WIN)
    QStringList roots = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
    const auto appendKnownFolder = [&roots](const KNOWNFOLDERID& folder) {
        PWSTR path = nullptr;
        if (SUCCEEDED(SHGetKnownFolderPath(folder, KF_FLAG_DONT_VERIFY, nullptr, &path)) && path)
            roots.append(QDir::fromNativeSeparators(QString::fromWCharArray(path)));
        CoTaskMemFree(path);
    };
    appendKnownFolder(FOLDERID_CommonPrograms);
    roots += QStandardPaths::standardLocations(QStandardPaths::DesktopLocation);
    appendKnownFolder(FOLDERID_PublicDesktop);
    roots.removeDuplicates();

    // Count every visited entry so desktop project trees cannot make the scan unbounded.
    constexpr int maxDepth = 4;
    int remainingEntries = 4096;
    int remainingShortcuts = 1024;
    struct PendingDirectory { QString path; int depth; };
    for (const QString& root : roots) {
        QList<PendingDirectory> pending{{root, 0}};
        for (qsizetype index = 0; index < pending.size() && remainingEntries > 0 && remainingShortcuts > 0; ++index) {
            const PendingDirectory directory = pending.at(index);
            QDirIterator iterator(directory.path, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
            while (remainingEntries > 0 && remainingShortcuts > 0 && iterator.hasNext()) {
                --remainingEntries;
                const QFileInfo entry(iterator.next());
                if (entry.isDir()) {
                    if (directory.depth < maxDepth && !entry.isSymLink() && !entry.isJunction())
                        pending.append({entry.absoluteFilePath(), directory.depth + 1});
                } else if (entry.isFile() && entry.suffix().compare(QLatin1String("lnk"), Qt::CaseInsensitive) == 0) {
                    --remainingShortcuts;
                    const QString target = entry.symLinkTarget();
                    if (!target.isEmpty()) targets.insert(target);
                }
            }
        }
        if (remainingEntries == 0 || remainingShortcuts == 0) break;
    }
#endif
    return targets.values();
}

} // namespace ua::platform
