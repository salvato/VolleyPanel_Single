#include <QNetworkInterface>
#include <QFile>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <QSettings>

#include "volleyapplication.h"
#include "volleypanel.h"

#define NETWORK_CHECK_TIME    3000 // In msec


VolleyApplication::VolleyApplication(int &argc, char **argv)
    : QApplication(argc, argv)
    , logFile(nullptr)
    , pScorePanel(nullptr)
{
    pSettings = new QSettings("Gabriele Salvato", "Volley Panel");
    sLanguage = pSettings->value("language/current",  QString("Italiano")).toString();
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Initial Language: %1").arg(sLanguage));
#endif
    if(sLanguage == QString("English")) {
        if(Translator.load(":/VolleyPanel_en_US.ts"))
            QCoreApplication::installTranslator(&Translator);
    }

    // Initialize the random number generator
    QTime time(QTime::currentTime());
    srand(uint(time.msecsSinceStartOfDay()));

    QString sBaseDir;
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");
    logFileName = QString("%1volley_panel.txt").arg(sBaseDir);
    PrepareLogFile();

    pScorePanel = new VolleyPanel(logFile);
    pScorePanel->showFullScreen();
}


bool
VolleyApplication::PrepareLogFile() {
#ifdef LOG_MESG
    QFileInfo checkFile(logFileName);
    if(checkFile.exists() && checkFile.isFile()) {
        QDir renamed;
        renamed.remove(logFileName+QString(".bkp"));
        renamed.rename(logFileName, logFileName+QString(".bkp"));
    }
    logFile = new QFile(logFileName);
    if (!logFile->open(QIODevice::WriteOnly)) {
        QMessageBox::information(Q_NULLPTR, "Segnapunti Volley",
                                 QString("Impossibile aprire il file %1: %2.")
                                 .arg(logFileName).arg(logFile->errorString()));
        delete logFile;
        logFile = Q_NULLPTR;
    }
#endif
    return true;
}
