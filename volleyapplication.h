#pragma once

#include <QApplication>
#include <QTranslator>
#include <QTimer>


QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(VolleyPanel)
QT_FORWARD_DECLARE_CLASS(QFile)


class VolleyApplication : public QApplication
{
    Q_OBJECT
public:
    VolleyApplication(int& argc, char** argv);

private:
    bool PrepareLogFile();

public:
    QTranslator        Translator;

private:
    QSettings         *pSettings;
    QFile             *logFile;
    VolleyPanel*      pScorePanel;
    QString            sLanguage;
    QString            logFileName;
};
