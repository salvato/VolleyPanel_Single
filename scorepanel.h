/*
 *
Copyright (C) 2016  Gabriele Salvato

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
#pragma once

#include <QObject>
#include <QMainWindow>
#include <QProcess>
#include <QFileInfoList>
#include <QUrl>
#include <QtGlobal>
#include <QTranslator>
#include <QTimer>
#include <QAbstractSocket>

#include "slidewindow.h"

#if (QT_VERSION < QT_VERSION_CHECK(5, 11, 0))
    #define horizontalAdvance width
#endif


QT_BEGIN_NAMESPACE
QT_FORWARD_DECLARE_CLASS(QSettings)
QT_FORWARD_DECLARE_CLASS(QFile)
QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QWebSocket)
QT_FORWARD_DECLARE_CLASS(SlideWindow)
QT_FORWARD_DECLARE_CLASS(QGridLayout)
QT_FORWARD_DECLARE_CLASS(UpdaterThread)
QT_FORWARD_DECLARE_CLASS(FileUpdater)
QT_END_NAMESPACE


class ScorePanel : public QMainWindow
{
    Q_OBJECT

public:
    ScorePanel(QFile *myLogFile, QWidget *parent = Q_NULLPTR);
    ~ScorePanel();
    void keyPressEvent(QKeyEvent *event);
    void closeEvent(QCloseEvent *event);
    void setScoreOnly(bool bScoreOnly);
    bool getScoreOnly();

signals:
    void panelClosed(); /*!< \brief emitted to signal that the Panel has been closed */

protected slots:
    void onTextMessageReceived(QString sMessage);
    void onBinaryMessageReceived(QByteArray baMessage);


private slots:
    void onConnectionTimeExipred();
    void onPanelServerConnected();
    void onPanelServerDisconnected();
    void onPanelServerSocketError(QAbstractSocket::SocketError error);
    void onTimeToRefreshStatus();
    void onSpotClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onLiveClosed(int exitCode, QProcess::ExitStatus exitStatus);
    void onStartNextSpot(int exitCode, QProcess::ExitStatus exitStatus);

protected:
    virtual QGridLayout* createPanel();
    void buildLayout();
    void doProcessCleanup();

protected:
    QString            serverUrl;
    bool               isMirrored;
    bool               isScoreOnly;
    QWebSocket        *pPanelServerSocket;
    QFile             *logFile;
    QTranslator        Translator;
    QTimer             connectionTimer;

private:
    bool               bStillConnected;
    QTimer             refreshTimer;
    QProcess          *videoPlayer;
    QProcess          *cameraPlayer;
    QString            sProcess;
    QString            sProcessArguments;

    // Spots management
    QString            sSpotDir;
    QFileInfoList      spotList;
    struct spot {
        QString spotFilename;
        qint64  spotFileSize;
    };
    QList<spot>        availabeSpotList;
    int                iCurrentSpot;

    // Slides management
    QString            sSlideDir;
    QFileInfoList      slideList;
    struct slide {
        QString slideFilename;
        qint64  slideFileSize;
    };
    QList<slide>       availabeSlideList;
    int                iCurrentSlide;

    QString            logFileName;

    SlideWindow       *pMySlideWindow;

private:
    void               initCamera();
    void               startLiveCamera();
    void               stopLiveCamera();
    void               startSpotLoop();
    void               stopSpotLoop();
    void               startSlideShow();
    void               stopSlideShow();
    void               getPanelScoreOnly();

private:
    QSettings         *pSettings;
    QWidget           *pPanel;
    QString            sPlayer;
};
