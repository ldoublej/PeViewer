#include "LanguageManager.h"

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

LanguageManager::LanguageManager(QObject* parent) : QObject(parent) {
    const QString defaultLanguage =
        QLocale::system().language() == QLocale::Chinese
            ? QStringLiteral("zh_CN")
            : QStringLiteral("en");
    const QString savedLanguage =
        QSettings()
            .value(QStringLiteral("ui/language"), defaultLanguage)
            .toString();
    if (!setLanguage(savedLanguage)) setLanguage(defaultLanguage);
}

LanguageManager::~LanguageManager() = default;

bool LanguageManager::setLanguage(const QString& language) {
    if (language != QStringLiteral("en") && language != QStringLiteral("zh_CN"))
        return false;
    if (language == m_language) return true;

    std::unique_ptr<QTranslator> translator;
    if (language != QStringLiteral("en")) {
        translator = std::make_unique<QTranslator>();
        if (!translator->load(
                QStringLiteral(":/i18n/PeViewer_%1.qm").arg(language)))
            return false;
        if (!QCoreApplication::installTranslator(translator.get())) return false;
    }

    if (m_translator) QCoreApplication::removeTranslator(m_translator.get());
    m_translator = std::move(translator);
    m_language = language;
    QSettings().setValue(QStringLiteral("ui/language"), language);
    emit languageChanged(language);
    return true;
}
