/*
 *
Copyright (C) 2023  Gabriele Salvato

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include <QtGlobal>
#include <QtNetwork>
#include <QtWidgets>
#include <QProcess>
#include <QWebSocket>
#include <QVBoxLayout>
#include <QSettings>
#include <QDebug>


#include "slidewindow.h"
#include "scorepanel.h"
#include "utility.h"
#include "panelorientation.h"
#include "volleyapplication.h"


#define SERVER_PORT           45454


ScorePanel::ScorePanel(QFile *myLogFile, QWidget *parent)
    : QMainWindow(parent)
    , isMirrored(false)
    , isScoreOnly(false)
    , pPanelServerSocket(new QWebSocket())
    , logFile(myLogFile)
    , videoPlayer(nullptr)
    , cameraPlayer(nullptr)
    , iCurrentSpot(0)
    , iCurrentSlide(0)
    , pMySlideWindow(new SlideWindow())
    , pPanel(nullptr)
#ifdef Q_OS_WINDOWS
    , sPlayer(QString("ffplay.exe"))
#else
    , sPlayer(QString("/usr/bin/ffplay"))
#endif

{
    QList<QScreen*> screens = QApplication::screens();
    if(screens.count() < 2) {
        QMessageBox::critical(nullptr,
                              tr("Secondo Monitor non connesso"),
                              tr("Connettilo e ritenta"),
                              QMessageBox::Abort);
        exit(0);
    }
    // Move the Panel on the Secondary Display
    QPoint point = QPoint(screens.at(1)->geometry().x(),
                          screens.at(1)->geometry().y());
    move(point);

    // We want the cursor set for all widgets,
    // even when outside the window then:
    qApp->setOverrideCursor(Qt::BlankCursor);
    // We don't want windows decorations
    setWindowFlags(Qt::CustomizeWindowHint);

    serverUrl = QString("ws://localhost:%1").arg(SERVER_PORT);

    pSettings = new QSettings("Gabriele Salvato", "Score Panel");
    isScoreOnly = pSettings->value("panel/scoreOnly",  false).toBool();
    isMirrored  = pSettings->value("panel/orientation",  false).toBool();

    // Connect the RefreshTimer timeout() with its SLOT()
    connect(&refreshTimer, SIGNAL(timeout()),
            this, SLOT(onTimeToRefreshStatus()));

    QString sBaseDir;
    sBaseDir = QDir::homePath();
    if(!sBaseDir.endsWith(QString("/"))) sBaseDir+= QString("/");

    sSpotDir = QString("%1spots/").arg(sBaseDir);
    sSlideDir= QString("%1slides/").arg(sBaseDir);

    // Camera management
    initCamera();

    // We are Ready to Connect to the Panel Server
    pPanelServerSocket->ignoreSslErrors(); // To silent some warnings
    // Events from the Panel Server WebSocket
    connect(pPanelServerSocket, SIGNAL(connected()),
            this, SLOT(onPanelServerConnected()));
    connect(pPanelServerSocket, SIGNAL(error(QAbstractSocket::SocketError)),
            this, SLOT(onPanelServerSocketError(QAbstractSocket::SocketError)));

    // Let's Start to try to connect to Panel Server
    connect(&connectionTimer, SIGNAL(timeout()),
            this, SLOT(onConnectionTimeExipred()));
    connectionTimer.start(1000);
}


ScorePanel::~ScorePanel() {
    refreshTimer.disconnect();
    refreshTimer.stop();
    if(pPanelServerSocket)
        pPanelServerSocket->disconnect();
    if(pSettings) delete pSettings;
    pSettings = Q_NULLPTR;

    doProcessCleanup();

    if(pPanelServerSocket)
        delete pPanelServerSocket;
    pPanelServerSocket = Q_NULLPTR;
}


void
ScorePanel::buildLayout() {
    QWidget* oldPanel = pPanel;
    pPanel = new QWidget(this);
    QVBoxLayout *panelLayout = new QVBoxLayout();
    panelLayout->addLayout(createPanel());
    pPanel->setLayout(panelLayout);
    setCentralWidget(pPanel);
    if(oldPanel != nullptr)
        delete oldPanel;

}


//==================
// Panel management
//==================
void
ScorePanel::setScoreOnly(bool bScoreOnly) {
    isScoreOnly = bScoreOnly;
    if(isScoreOnly) {
        // Terminate, if running, Videos, Slides and Camera
        if(pMySlideWindow) {
            pMySlideWindow->close();
        }
        if(videoPlayer) {
#ifdef LOG_MESG
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Closing Video Player..."));
#endif
            videoPlayer->disconnect();
            videoPlayer->close();
            videoPlayer->waitForFinished(3000);
            videoPlayer->deleteLater();
            videoPlayer = Q_NULLPTR;
        }
        if(cameraPlayer) {
            cameraPlayer->close();
            cameraPlayer->waitForFinished(3000);
            cameraPlayer->deleteLater();
            cameraPlayer = Q_NULLPTR;
        }
    }
}


bool
ScorePanel::getScoreOnly() {
    return isScoreOnly;
}


void
ScorePanel::onConnectionTimeExipred() {
    // Try to (Re)Open the Server socket to talk to
    pPanelServerSocket->open(QUrl(serverUrl));
}


void
ScorePanel::onPanelServerConnected() {
    connectionTimer.stop();
    connect(pPanelServerSocket, SIGNAL(disconnected()),
            this, SLOT(onPanelServerDisconnected()));
    QString sMessage = QString("<getStatus>%1</getStatus>")
                          .arg(QHostInfo::localHostName());
    qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Unable to ask the initial status"));
    }
    bStillConnected = false;
    refreshTimer.start(rand()%2000+3000);
}


void
ScorePanel::onTimeToRefreshStatus() {
    if(!bStillConnected) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Panel Server Disconnected"));
#endif
        if(pPanelServerSocket)
            pPanelServerSocket->deleteLater();
        pPanelServerSocket =Q_NULLPTR;
        doProcessCleanup();
        close();
        emit panelClosed();
        return;
    }
    QString sMessage;
    sMessage = QString("<getStatus>%1</getStatus>").arg(QHostInfo::localHostName());
    qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
    if(bytesSent != sMessage.length()) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Unable to refresh the Panel status"));
#endif
        if(pPanelServerSocket)
            pPanelServerSocket->deleteLater();
        pPanelServerSocket =Q_NULLPTR;
        doProcessCleanup();
        close();
        emit panelClosed();
    }
    bStillConnected = false;
}


void
ScorePanel::onPanelServerDisconnected() {
    refreshTimer.stop();
    disconnect(pPanelServerSocket, SIGNAL(disconnected()),
               this, SLOT(onPanelServerDisconnected()));
    connectionTimer.start(1000);
}


void
ScorePanel::doProcessCleanup() {
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Cleaning all processes"));
#endif
    connectionTimer.disconnect();
    refreshTimer.disconnect();
    connectionTimer.stop();
    refreshTimer.stop();

    if(pMySlideWindow) {
        pMySlideWindow->close();
    }
    if(videoPlayer) {
        videoPlayer->disconnect();
        videoPlayer->close();
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Closing Video Player..."));
        videoPlayer->waitForFinished(3000);
        videoPlayer->deleteLater();
        videoPlayer = Q_NULLPTR;
    }
    if(cameraPlayer) {
        cameraPlayer->close();
        cameraPlayer->waitForFinished(3000);
        cameraPlayer->deleteLater();
        cameraPlayer = Q_NULLPTR;
    }
}


void
ScorePanel::onPanelServerSocketError(QAbstractSocket::SocketError error) {
    Q_UNUSED(error)
    refreshTimer.stop();
    if(pPanelServerSocket->isValid())
        pPanelServerSocket->close();
    connectionTimer.start(1000);
}



void
ScorePanel::closeEvent(QCloseEvent *event) {
    pSettings->setValue("panel/orientation", isMirrored);
    doProcessCleanup();
    event->accept();
}


void
ScorePanel::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        if(pPanelServerSocket) {
            pPanelServerSocket->disconnect();
            pPanelServerSocket->close(QWebSocketProtocol::CloseCodeNormal,
                                      tr("Il Client ha chiuso il collegamento"));
        }
        close();
    }
}


void
ScorePanel::onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(videoPlayer) {
        videoPlayer->disconnect();
        videoPlayer->close();// Closes all communication with the process and kills it.
        delete videoPlayer;
        videoPlayer = Q_NULLPTR;
        QString sMessage = "<closed_spot>1</closed_spot>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Sent %1")
                       .arg(sMessage));
        }
#endif
    } // if(videoPlayer)
    showFullScreen(); // Restore the Score Panel
}


void
ScorePanel::onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    if(cameraPlayer) {
        cameraPlayer->disconnect();
        cameraPlayer->close();
        delete cameraPlayer;
        cameraPlayer = Q_NULLPTR;
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Sent %1")
                       .arg(sMessage));
        }
#endif
    } // if(cameraPlayer)
    showFullScreen(); // Restore the Score Panel
}


void
ScorePanel::onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitCode);
    Q_UNUSED(exitStatus);
    showFullScreen(); // Ripristina lo Score Panel
    // Update spot list just in case we are updating the spot list...
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
    spotDir.setNameFilters(nameFilter);
    spotDir.setFilter(QDir::Files);
    spotList = spotDir.entryInfoList();
    if(spotList.count() == 0) {
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("No spots available !"));
#endif
        if(videoPlayer) {
            videoPlayer->disconnect();
            delete videoPlayer;
            videoPlayer = Q_NULLPTR;
            QString sMessage = "<closed_spot>1</closed_spot>";
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send %1")
                           .arg(sMessage));
            }
        }
        return;
    }

    iCurrentSpot = iCurrentSpot % spotList.count();
    if(!videoPlayer) {
        videoPlayer = new QProcess(this);
        connect(videoPlayer, SIGNAL(finished(int,QProcess::ExitStatus)),
                this, SLOT(onStartNextSpot(int,QProcess::ExitStatus)));
    }

    QStringList sArguments;
    sArguments = QStringList{"-noborder",
                             "-sn",
                             "-autoexit",
                             "-fs"
                            };
    QList<QScreen*> screens = QApplication::screens();
    if(screens.count() > 1) {
        QRect screenres = screens.at(1)->geometry();
        sArguments.append(QString("-left"));
        sArguments.append(QString("%1").arg(screenres.x()));
        sArguments.append(QString("-top"));
        sArguments.append(QString("%1").arg(screenres.y()));
        sArguments.append(QString("-x"));
        sArguments.append(QString("%1").arg(screenres.width()));
        sArguments.append(QString("-y"));
        sArguments.append(QString("%1").arg(screenres.height()));
    }
    sArguments.append(spotList.at(iCurrentSpot).absoluteFilePath());

    videoPlayer->start(sPlayer, sArguments);
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Now playing: %1")
               .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
    iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
    if(!videoPlayer->waitForStarted(3000)) {
        videoPlayer->close();
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Impossibile mandare lo spot"));
        videoPlayer->disconnect();
        delete videoPlayer;
        videoPlayer = Q_NULLPTR;
        return;
    }
    hide();
}


void
ScorePanel::onBinaryMessageReceived(QByteArray baMessage) {
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Received %1 bytes").arg(baMessage.size()));
}


void
ScorePanel::onTextMessageReceived(QString sMessage) {
    refreshTimer.start(rand()%2000+3000);
    bStillConnected = true;
    QString sToken;
    bool ok;
    int iVal;
    QString sNoData = QString("NoData");

    sToken = XML_Parse(sMessage, "kill");
    if(sToken != sNoData) {
        iVal = sToken.toInt(&ok);
        if(!ok || iVal<0 || iVal>1)
            iVal = 0;
        if(iVal == 1) {
            pPanelServerSocket->disconnect();
            #ifdef Q_PROCESSOR_ARM
            system("sudo halt");
            #endif
            close();// emit the QCloseEvent that is responsible
                    // to clean up all pending processes
        }
    }// kill

    sToken = XML_Parse(sMessage, "spotdir");
    if(sToken != sNoData){
        sSpotDir = sToken;
    }// slideshow

    sToken = XML_Parse(sMessage, "spotloop");
    if(sToken != sNoData && !isScoreOnly) {
        startSpotLoop();
    }// spotloop

    sToken = XML_Parse(sMessage, "endspotloop");
    if(sToken != sNoData) {
        stopSpotLoop();
    }// endspoloop

    sToken = XML_Parse(sMessage, "slidedir");
    if(sToken != sNoData){
        sSlideDir = sToken;
    }// slideshow

    sToken = XML_Parse(sMessage, "slideshow");
    if(sToken != sNoData && !isScoreOnly){
        startSlideShow();
    }// slideshow

    sToken = XML_Parse(sMessage, "endslideshow");
    if(sToken != sNoData) {
        stopSlideShow();
    }// endslideshow

    sToken = XML_Parse(sMessage, "live");
    if(sToken != sNoData && !isScoreOnly) {
        startLiveCamera();
    }// live

    sToken = XML_Parse(sMessage, "endlive");
    if(sToken != sNoData) {
        stopLiveCamera();
    }// endlive

    sToken = XML_Parse(sMessage, "pan");
    if(sToken != sNoData) {
    }// pan

    sToken = XML_Parse(sMessage, "tilt");
    if(sToken != sNoData) {
    }// tilt

    sToken = XML_Parse(sMessage, "getPanTilt");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
        }
    }// getPanTilt

    sToken = XML_Parse(sMessage, "getOrientation");
    if(sToken != sNoData) {
        if(pPanelServerSocket->isValid()) {
            QString sMessage;
            if(isMirrored)
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Reflected));
            else
                sMessage = QString("<orientation>%1</orientation>").arg(static_cast<int>(PanelOrientation::Normal));
            qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
            if(bytesSent != sMessage.length()) {
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Unable to send orientation value."));
            }
        }
    }// getOrientation

    sToken = XML_Parse(sMessage, "setOrientation");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        try {
            PanelOrientation newOrientation = static_cast<PanelOrientation>(iVal);
            if(newOrientation == PanelOrientation::Reflected)
                isMirrored = true;
            else
                isMirrored = false;
        } catch(...) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal orientation value received: %1")
                               .arg(sToken));
            return;
        }
        pSettings->setValue("panel/orientation", isMirrored);
        buildLayout();
    }// setOrientation

    sToken = XML_Parse(sMessage, "getScoreOnly");
    if(sToken != sNoData) {
        getPanelScoreOnly();
    }// getScoreOnly

    sToken = XML_Parse(sMessage, "setScoreOnly");
    if(sToken != sNoData) {
        bool ok;
        int iVal = sToken.toInt(&ok);
        if(!ok) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Illegal value fo ScoreOnly received: %1")
                               .arg(sToken));
            return;
        }
        if(iVal==0) {
            setScoreOnly(false);
        }
        else {
            setScoreOnly(true);
        }
        pSettings->setValue("panel/scoreOnly", isScoreOnly);
    }// setScoreOnly

    sToken = XML_Parse(sMessage, "language");
    if(sToken != sNoData) {
        VolleyApplication* application = static_cast<VolleyApplication *>(QApplication::instance());

        QCoreApplication::removeTranslator(&application->Translator);
        if(sToken == QString("English")) {
            if(application->Translator.load(":/panelChooser_en"))
                QCoreApplication::installTranslator(&application->Translator);
        }
        else {
            sToken = QString("Italiano");
        }
        pSettings->setValue("language/current", sToken);
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("New language: %1")
                       .arg(sToken));
#endif
    }// language
}


void
ScorePanel::initCamera() {
}


void
ScorePanel::startLiveCamera() {
    startSpotLoop();
}


void
ScorePanel::stopLiveCamera() {
    if(cameraPlayer) {
        cameraPlayer->disconnect();
        connect(cameraPlayer, SIGNAL(finished(int,QProcess::ExitStatus)),
                this, SLOT(onLiveClosed(int,QProcess::ExitStatus)));
        cameraPlayer->terminate();
#ifdef LOG_VERBOSE
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Live Show has been closed."));
#endif
    }
    else {
        QString sMessage = "<closed_live>1</closed_live>";
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send %1")
                       .arg(sMessage));
        }
#ifdef LOG_VERBOSE
        else {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Sent %1")
                       .arg(sMessage));
        }
#endif
        stopSpotLoop();
    }
}


void
ScorePanel::getPanelScoreOnly() {
    if(pPanelServerSocket->isValid()) {
        QString sMessage;
        sMessage = QString("<isScoreOnly>%1</isScoreOnly>").arg(static_cast<int>(getScoreOnly()));
        qint64 bytesSent = pPanelServerSocket->sendTextMessage(sMessage);
        if(bytesSent != sMessage.length()) {
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Unable to send scoreOnly configuration"));
        }
    }
}


void
ScorePanel::startSpotLoop() {
    QDir spotDir(sSpotDir);
    spotList = QFileInfoList();
    if(spotDir.exists()) {
        QStringList nameFilter(QStringList() << "*.mp4" << "*.MP4");
        spotDir.setNameFilters(nameFilter);
        spotDir.setFilter(QDir::Files);
        spotList = spotDir.entryInfoList();
    }
#ifdef LOG_VERBOSE
    logMessage(logFile,
               Q_FUNC_INFO,
               QString("Found %1 spots").arg(spotList.count()));
#endif
    if(!spotList.isEmpty()) {
        iCurrentSpot = iCurrentSpot % spotList.count();
        if(!videoPlayer) {
            videoPlayer = new QProcess(this);
            connect(videoPlayer, SIGNAL(finished(int,QProcess::ExitStatus)),
                    this, SLOT(onStartNextSpot(int,QProcess::ExitStatus)));

            QStringList sArguments;
            sArguments = QStringList{"-noborder",
                                     "-sn",
                                     "-autoexit",
                                     "-fs"
                                    };
            QList<QScreen*> screens = QApplication::screens();
            if(screens.count() > 1) {
                QRect screenres = screens.at(1)->geometry();
                sArguments.append(QString("-left"));
                sArguments.append(QString("%1").arg(screenres.x()));
                sArguments.append(QString("-top"));
                sArguments.append(QString("%1").arg(screenres.y()));
                sArguments.append(QString("-x"));
                sArguments.append(QString("%1").arg(screenres.width()));
                sArguments.append(QString("-y"));
                sArguments.append(QString("%1").arg(screenres.height()));
            }
            sArguments.append(spotList.at(iCurrentSpot).absoluteFilePath());

            videoPlayer->start(sPlayer, sArguments);
#ifdef LOG_VERBOSE
            logMessage(logFile,
                       Q_FUNC_INFO,
                       QString("Now playing: %1")
                       .arg(spotList.at(iCurrentSpot).absoluteFilePath()));
#endif
            iCurrentSpot = (iCurrentSpot+1) % spotList.count();// Prepare Next Spot
            if(!videoPlayer->waitForStarted(3000)) {
                videoPlayer->close();
                logMessage(logFile,
                           Q_FUNC_INFO,
                           QString("Impossibile mandare lo spot."));
                videoPlayer->disconnect();
                delete videoPlayer;
                videoPlayer = Q_NULLPTR;
                return;
            }
            hide(); // Hide the Score Panel
        } // if(!videoPlayer)
    }
}


void
ScorePanel::stopSpotLoop() {
    if(videoPlayer) {
        videoPlayer->disconnect();
        connect(videoPlayer, SIGNAL(finished(int,QProcess::ExitStatus)),
                this, SLOT(onSpotClosed(int,QProcess::ExitStatus)));
        videoPlayer->terminate();
    }
}


void
ScorePanel::startSlideShow() {
    if(videoPlayer || cameraPlayer)
        return;// No Slide Show if movies are playing or camera is active
    if(pMySlideWindow) {
        pMySlideWindow->setSlideDir(sSlideDir);
        pMySlideWindow->showFullScreen();
        hide(); // Hide the Score Panel
        pMySlideWindow->startSlideShow();
    }
    else {
        logMessage(logFile,
                   Q_FUNC_INFO,
                   QString("Invalid Slide Window"));
    }
}


void
ScorePanel::stopSlideShow() {
    if(pMySlideWindow) {
        pMySlideWindow->stopSlideShow();
        showFullScreen(); // Show the Score Panel
        pMySlideWindow->hide();
    }
}


QGridLayout*
ScorePanel::createPanel() {
    return new QGridLayout();
}
