#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QVector>
#include <optional>

// Windows 10+ API Set namespace (version 6). No DLL code is loaded or executed.
class ApiSetMap {
public:
    static std::optional<ApiSetMap> parse(const QByteArray& bytes);
    static std::optional<ApiSetMap> load(const QString& schemaPath);

    // An empty result means the contract cannot be resolved by this map.
    QString resolveHost(const QString& dllName, const QString& importer = {}) const;
    // Also useful for supplying an offline schema or an explicit override.
    // An empty importer is the default; named importers override that default.
    void addMapping(const QString& contract, const QString& host, const QString& importer = {});
    bool isEmpty() const { return m_hosts.isEmpty(); }

private:
    static QString lookupKey(QString name);
    static QString importerKey(QString name);
    struct Host {
        QString importer;
        QString name;
    };
    QHash<QString, QVector<Host>> m_hosts;
};
