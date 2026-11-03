#pragma once

#include <QByteArray>
#include <QString>

namespace ua::core {

struct SkillMetadata {
    QString name;
    QString description;
};

class SkillCatalog
{
public:
    static bool parse(const QByteArray& source, SkillMetadata* metadata,
                      QString* error = nullptr);
    static QByteArray normalize(const QByteArray& source, const QString& canonicalName,
                                bool* ok = nullptr);
};

} // namespace ua::core
