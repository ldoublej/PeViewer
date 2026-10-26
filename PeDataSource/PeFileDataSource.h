#pragma once

#include <QFile>
#include <QMutex>

#include "PeDataSource.h"

class PeFileDataSource final : public PeDataSource {
   public:
    explicit PeFileDataSource(const QString& path);

    quint64 length() const override;
    bool isFileAligned() const override { return true; }
    quint64 imageBase() const override { return 0; }
    QString url() const override { return m_file.fileName(); }
    void readExact(quint64 offset, char* buffer,
                   std::size_t count) const override;

   private:
    mutable QMutex m_mutex;
    mutable QFile m_file;
};
