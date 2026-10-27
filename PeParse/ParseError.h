#pragma once

#include <QString>
#include <QtGlobal>
#include <optional>
#include <stdexcept>

class ParseError : public std::runtime_error {
   public:
    enum class Code {
        TruncatedData,
        InvalidSignature,
        UnsupportedFormat,
        UnsupportedLayout,
        InvalidHeader,
        InvalidRva,
        InvalidDirectory,
        UnterminatedString,
        LimitExceeded,
        DataSource
    };
    ParseError(Code code, const QString& message,
               std::optional<quint64> offset = std::nullopt,
               std::optional<int> directoryIndex = std::nullopt);
    Code code() const noexcept { return m_code; }
    std::optional<quint64> offset() const noexcept { return m_offset; }
    std::optional<int> directoryIndex() const noexcept { return m_directory; }

   private:
    Code m_code;
    std::optional<quint64> m_offset;
    std::optional<int> m_directory;
};
