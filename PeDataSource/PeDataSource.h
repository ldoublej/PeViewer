#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <type_traits>

class PeDataSourceError : public std::runtime_error {
   public:
    enum class Code { OutOfBounds, Io };

    PeDataSourceError(Code code, const QString& message, quint64 offset = 0,
                      std::size_t length = 0);

    Code code() const noexcept { return m_code; }
    quint64 offset() const noexcept { return m_offset; }
    std::size_t length() const noexcept { return m_length; }

   private:
    Code m_code;
    quint64 m_offset;
    std::size_t m_length;
};

// 按偏移量只读访问 PE 数据。实现类须支持并发读取，读取不足时报告 OutOfBounds。
class PeDataSource {
   public:
    virtual ~PeDataSource() = default;

    virtual std::optional<quint64> length() const = 0;
    bool isEmpty() const { return length() == 0; }
    virtual bool isFileAligned() const = 0;
    virtual quint64 imageBase() const = 0;
    virtual QString url() const = 0;
    virtual void readExact(quint64 offset, char* buffer,
                           std::size_t count) const = 0;

    quint16 readU16(quint64 offset) const;
    quint32 readU32(quint64 offset) const;
    quint64 readU64(quint64 offset) const;
    QByteArray readBytes(quint64 offset, std::size_t count) const;
    QString readAscii(quint64 offset, std::size_t maxLength) const;
    QString readUtf16(quint64 offset, std::size_t maxCodeUnits) const;

    template <std::size_t N>
    std::array<std::uint8_t, N> readArray(quint64 offset) const {
        std::array<std::uint8_t, N> result{};
        readExact(offset, reinterpret_cast<char*>(result.data()), N);
        return result;
    }

    // T 须为紧凑布局的 PE 结构体，且所有字段均能接受任意字节值。
    // 整数字段按主机字节序解释；跨平台读取标量时应使用 readU*。
    template <typename T>
    T readStruct(quint64 offset) const {
        static_assert(std::is_trivially_copyable<T>::value &&
                          std::is_standard_layout<T>::value,
                      "PE structures must have trivial, standard layout");
        T result{};
        readExact(offset, reinterpret_cast<char*>(&result), sizeof result);
        return result;
    }
};
