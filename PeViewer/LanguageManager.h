#pragma once

#include <QObject>
#include <QString>
#include <memory>

class QTranslator;

class LanguageManager : public QObject {
    Q_OBJECT

   public:
    explicit LanguageManager(QObject* parent = nullptr);
    ~LanguageManager() override;

    QString language() const { return m_language; }
    bool setLanguage(const QString& language);

   signals:
    void languageChanged(const QString& language);

   private:
    QString m_language;
    std::unique_ptr<QTranslator> m_translator;
};
