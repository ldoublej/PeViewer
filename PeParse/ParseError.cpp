#include "ParseError.h"

ParseError::ParseError(Code code, const QString& message, std::optional<quint64> offset,
                       std::optional<int> directoryIndex)
    : std::runtime_error(message.toUtf8().constData()),
      m_code(code),
      m_offset(offset),
      m_directory(directoryIndex) {}
