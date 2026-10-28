#pragma once

#include <QByteArray>
#include <QHash>
#include <QString>
#include <QStringList>
#include <optional>

// Windows 10+ API Set namespace (version 6). No DLL code is loaded or executed.
class ApiSetMap {
public:
    static std::optional<ApiSetMap> parse(const QByteArray& bytes);
    static std::optional<ApiSetMap> load(const QString& schemaPath);

    // An empty result means the contract cannot be resolved by this map.
    QString resolveHost(const QString& dllName) const;
    // Also useful for supplying an offline schema or an explicit override.
    void addMapping(const QString& contract, const QString& host);
    bool isEmpty() const { return m_hosts.isEmpty(); }

private:
    static QString lookupKey(QString name);
    QHash<QString, QStringList> m_hosts;
};
