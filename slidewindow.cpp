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
#include <QDir>
#include <QDebug>
#include <QPainter>
#include <QApplication>

#include "slidewindow.h"
#include "utility.h"


#define STEADY_SHOW_TIME       5000 // Change slide time
#define TRANSITION_TIME        3000 // Transition duration
#define TRANSITION_GRANULARITY 30   // Steps to complete transition


SlideWindow::SlideWindow(QWidget *parent)
    : QLabel(tr("Nessuna Slide Presente"))
    , pPresentImage(nullptr)
    , pNextImage(nullptr)
    , pPresentImageToShow(nullptr)
    , pNextImageToShow(nullptr)
    , pShownImage(nullptr)
    , iCurrentSlide(0)
    , steadyShowTime(STEADY_SHOW_TIME)
    , transitionTime(TRANSITION_TIME)
    , transitionGranularity(TRANSITION_GRANULARITY)
    , transitionStepNumber(0)
//    , transitionType(transition_Abrupt)
//    , transitionType(transition_FromLeft)
    , transitionType(transition_Fade)
    , bRunning(false)
{
    Q_UNUSED(parent);

    sSlideDir = QDir::homePath();// Just to have a default location
    setAlignment(Qt::AlignCenter);
    setMinimumSize(QSize(320, 240));

    connect(&transitionTimer, SIGNAL(timeout()),
            this, SLOT(onTransitionTimeElapsed()));
    connect(&showTimer, SIGNAL(timeout()),
            this, SLOT(onNewSlideTimer()));

    panelPalette = QWidget::palette();
    panelGradient = QLinearGradient(0.0, 0.0, 0.0, height());
    panelGradient.setColorAt(0, QColor(0, 0, START_GRADIENT));
    panelGradient.setColorAt(1, QColor(0, 0, END_GRADIENT));
    panelBrush = QBrush(panelGradient);
    panelPalette.setBrush(QPalette::Active,   QPalette::Window, panelBrush);
    panelPalette.setBrush(QPalette::Inactive, QPalette::Window, panelBrush);
    panelPalette.setColor(QPalette::WindowText,    Qt::yellow);
    panelPalette.setColor(QPalette::Base,          Qt::black);
    panelPalette.setColor(QPalette::AlternateBase, Qt::blue);
    panelPalette.setColor(QPalette::Text,          Qt::yellow);
    panelPalette.setColor(QPalette::BrightText,    Qt::white);
    setPalette(panelPalette);
    QList<QScreen*> screens = QApplication::screens();
    if(screens.count() > 1) {
        QRect screenres = screens.at(1)->geometry();
        QPoint point = QPoint(screenres.x(), screenres.y());
        move(point);
    }
}


SlideWindow::~SlideWindow() {
    if(pPresentImageToShow) delete pPresentImageToShow;
    if(pNextImageToShow)    delete pNextImageToShow;
    if(pShownImage)         delete pShownImage;
    if(pPresentImage)       delete pPresentImage;
    if(pNextImage)          delete pNextImage;
}


void
SlideWindow::setSlideDir(QString sNewDir) {
    if(sNewDir != sSlideDir) {
        sSlideDir = sNewDir;
        updateSlideList();
        iCurrentSlide = 0;
        if(pPresentImage) delete pPresentImage;
        pPresentImage = nullptr;
        if(pNextImage) delete pNextImage;
        pNextImage = nullptr;
    }
}


bool
SlideWindow::isReady() {
    return (pPresentImage != nullptr && pNextImage != nullptr);
}


void
SlideWindow::updateSlideList() {
    slideList = QFileInfoList();
    QDir slideDir(sSlideDir);
    if(slideDir.exists()) {
        QStringList nameFilter = QStringList()
                << "*.jpg" << "*.jpeg" << "*.png"
                << "*.JPG" << "*.JPEG" << "*.PNG";
        slideDir.setNameFilters(nameFilter);
        slideDir.setFilter(QDir::Files);
        slideList = slideDir.entryInfoList();
    }
}


void
SlideWindow::startSlideShow() {
    if(bRunning) // Already Running...Nothing to do
        return;
    updateSlideList();
    if(slideList.count() == 0) {
        showTimer.start(steadyShowTime);
        bRunning = true;
        return;
    }
    if(pPresentImage == nullptr) {// That's the first image...
        iCurrentSlide = iCurrentSlide % slideList.count();
        QImage* pImage = new QImage(slideList.at(iCurrentSlide).absoluteFilePath());
        if(pImage == nullptr)
            return;
        addFirstImage(*pImage);
        iCurrentSlide++;
        iCurrentSlide= iCurrentSlide % slideList.count();
        QImage* pNextImage = new QImage(slideList.at(iCurrentSlide).absoluteFilePath());
        if(pNextImage)
            addNewImage(*pNextImage);
        else
            addNewImage(*pImage);
    }
    showTimer.start(steadyShowTime);
    bRunning = true;
}


void
SlideWindow::addFirstImage(QImage image) {
    QImage* pImage = new QImage(image);
    pPresentImage = pImage;
    pNextImage    = nullptr;
}


void
SlideWindow::addNewImage(QImage image) {
    QImage* pImage = new QImage(image);
    if(pNextImage == nullptr) {
        pNextImage = pImage;
        QImage scaledPresentImage, scaledNextImage;
        if(pPresentImage)
            scaledPresentImage = pPresentImage->scaled(size(), Qt::KeepAspectRatio);
        if(pNextImage)
            scaledNextImage    = pNextImage->scaled(size(), Qt::KeepAspectRatio);

        if(pPresentImageToShow) delete pPresentImageToShow;
        if(pNextImageToShow)    delete pNextImageToShow;
        if(pShownImage)         delete pShownImage;

        pPresentImageToShow = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        pNextImageToShow    = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        pShownImage         = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

        int x = (size().width()-scaledPresentImage.width())/2;
        int y = (size().height()-scaledPresentImage.height())/2;

        QPainter presentPainter(pPresentImageToShow);
        presentPainter.setCompositionMode(QPainter::CompositionMode_Source);
        presentPainter.fillRect(rect(), Qt::white);
        presentPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        presentPainter.drawImage(x, y, scaledPresentImage);
        presentPainter.end();

        x = (size().width()-scaledNextImage.width())/2;
        y = (size().height()-scaledNextImage.height())/2;

        QPainter nextPainter(pNextImageToShow);
        nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
        nextPainter.fillRect(rect(), Qt::white);
        nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        nextPainter.drawImage(x, y, scaledNextImage);
        nextPainter.end();

        computeRegions(&rectSourcePresent, &rectDestinationPresent,
                       &rectSourceNext,    &rectDestinationNext);

        QPainter painter(pShownImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
        painter.end();

        setPixmap(QPixmap::fromImage(*pShownImage));
    }
    else {
        delete pPresentImage;
        pPresentImage = pNextImage;
        pNextImage = pImage;
    }
}


/*!
 * \brief SlideWindow::stopSlideShow
 */
void
SlideWindow::stopSlideShow() {
    showTimer.stop();
    transitionTimer.stop();
    bRunning = false;
}


/*!
 * \brief SlideWindow::pauseSlideShow
 */
void
SlideWindow::pauseSlideShow() {
    showTimer.stop();
    transitionTimer.stop();
    bRunning = false;
}


/*!
 * \brief SlideWindow::isRunning
 * \return
 */
bool
SlideWindow::isRunning() {
    return bRunning;
}


/*!
 * \brief SlideWindow::computeRegions
 * \param sourcePresent
 * \param destinationPresent
 * \param sourceNext
 * \param destinationNext
 */
void
SlideWindow::computeRegions(QRect* sourcePresent, QRect* destinationPresent,
                            QRect* sourceNext,    QRect* destinationNext)
{
    double percent = double(transitionStepNumber)/double(transitionGranularity);
    *sourcePresent = QRect(0, 0,
                           int(width()*(1.0-percent)+0.5), height());
    *destinationPresent = *sourcePresent;
    destinationPresent->translate(width()*percent, 0);

    *sourceNext = QRect(int(double(width())*(1.0-percent)+0.5), 0,
                        int(width()*percent+0.5), height());
    *destinationNext = QRect(0, 0,
                             int(width()*percent+0.5), height());
}


/*!
 * \brief SlideWindow::keyPressEvent
 * \param event
 */
void
SlideWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Escape) {
        close();
    }
    if(event->key() == Qt::Key_F1) {
        showNormal();
        event->accept();
        return;
    }
}


/*!
 * \brief SlideWindow::resizeEvent
 * \param event
 */
void
SlideWindow::resizeEvent(QResizeEvent *event) {
    mySize = event->size();
    if(pPresentImage==nullptr || pNextImage==nullptr) {
        event->accept();
        return;
    }
    QImage scaledPresentImage = pPresentImage->scaled(size(), Qt::KeepAspectRatio);
    QImage scaledNextImage    = pNextImage->scaled(size(), Qt::KeepAspectRatio);

    if(pPresentImageToShow) delete pPresentImageToShow;
    if(pNextImageToShow)    delete pNextImageToShow;
    if(pShownImage)         delete pShownImage;

    pPresentImageToShow = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
    pNextImageToShow    = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
    pShownImage         = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

    int x = (size().width()-scaledPresentImage.width())/2;
    int y = (size().height()-scaledPresentImage.height())/2;

    QPainter presentPainter(pPresentImageToShow);
    presentPainter.setCompositionMode(QPainter::CompositionMode_Source);
    presentPainter.fillRect(rect(), Qt::white);
    presentPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    presentPainter.drawImage(x, y, scaledPresentImage);
    presentPainter.end();

    x = (size().width()-scaledNextImage.width())/2;
    y = (size().height()-scaledNextImage.height())/2;

    QPainter nextPainter(pNextImageToShow);
    nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
    nextPainter.fillRect(rect(), Qt::white);
    nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    nextPainter.drawImage(x, y, scaledNextImage);
    nextPainter.end();

    computeRegions(&rectSourcePresent, &rectDestinationPresent,
                   &rectSourceNext,    &rectDestinationNext);

    QPainter painter(pShownImage);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
    painter.end();

    setPixmap(QPixmap::fromImage(*pShownImage));
}


/*!
 * \brief SlideWindow::onNewSlideTimer
 */
void
SlideWindow::onNewSlideTimer() {
    updateSlideList();
    if(slideList.count() == 0) {// Still no slides !
        return;
    }
    if(pPresentImage == nullptr) {// That's the first image...
        addNewImage(QImage(slideList.at(0).absoluteFilePath()));
        iCurrentSlide = 0;
        if(slideList.count() > 1) {
            addNewImage(QImage(slideList.at(1).absoluteFilePath()));
            iCurrentSlide = 1;
        }
        else {// Only one image is in the directory
            addNewImage(*pPresentImage);
        }
    }
    if(transitionType == transition_FromLeft) {
        showTimer.stop();
        transitionStepNumber = 0;
        transitionTimer.start(int(double(transitionTime)/double(transitionGranularity)));
    }
    else if(transitionType == transition_Abrupt) {
        transitionStepNumber = 0;
        if(pPresentImageToShow) delete pPresentImageToShow;
        *pPresentImage = *pNextImage;
        pPresentImageToShow = pNextImageToShow;
        if(slideList.count() == 0) {
            return;
        }
        iCurrentSlide += 1;
        iCurrentSlide = iCurrentSlide % slideList.count();
        addNewImage(QImage(slideList.at(iCurrentSlide).absoluteFilePath()));
        QImage scaledNextImage = pNextImage->scaled(size(), Qt::KeepAspectRatio);
        pNextImageToShow = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

        if(pShownImage) delete pShownImage;
        pShownImage = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        int x = (size().width()-scaledNextImage.width())/2;
        int y = (size().height()-scaledNextImage.height())/2;

        QPainter nextPainter(pNextImageToShow);
        nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
        nextPainter.fillRect(rect(), Qt::white);
        nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        nextPainter.drawImage(x, y, scaledNextImage);
        nextPainter.end();

        computeRegions(&rectSourcePresent, &rectDestinationPresent,
                       &rectSourceNext,    &rectDestinationNext);
        QPainter painter(pShownImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
        painter.end();

        setPixmap(QPixmap::fromImage(*pShownImage));
    }
    else if (transitionType == transition_Fade) {
        showTimer.stop();
        transitionStepNumber = 0;
        transitionTimer.start(int(double(transitionTime)/double(transitionGranularity)));
    }
    // else if (transitionType == other types...
}


/*!
 * \brief SlideWindow::onTransitionTimeElapsed
 */
void
SlideWindow::onTransitionTimeElapsed() {
    if(pPresentImage == nullptr || pNextImage == nullptr || pShownImage == nullptr)
        return;
    transitionStepNumber++;
    if(transitionStepNumber > transitionGranularity) {
        transitionTimer.stop();
        transitionStepNumber = 0;
        if(pPresentImageToShow) delete pPresentImageToShow;
        *pPresentImage = *pNextImage;
        pPresentImageToShow = pNextImageToShow;
        updateSlideList();
        if(slideList.count() == 0) {
            return;
        }
        iCurrentSlide++;
        iCurrentSlide = iCurrentSlide % slideList.count();
        addNewImage(QImage(slideList.at(iCurrentSlide).absoluteFilePath()));

        QImage scaledNextImage = pNextImage->scaled(size(), Qt::KeepAspectRatio);
        pNextImageToShow = new QImage(size(), QImage::Format_ARGB32_Premultiplied);

        if(pShownImage) delete pShownImage;
        pShownImage = new QImage(size(), QImage::Format_ARGB32_Premultiplied);
        int x = (size().width()-scaledNextImage.width())/2;
        int y = (size().height()-scaledNextImage.height())/2;

        QPainter nextPainter(pNextImageToShow);
        nextPainter.setCompositionMode(QPainter::CompositionMode_Source);
        nextPainter.fillRect(rect(), Qt::white);
        nextPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        nextPainter.drawImage(x, y, scaledNextImage);
        nextPainter.end();

        showTimer.start(steadyShowTime);
    }
    if(transitionType == transition_FromLeft) {
        computeRegions(&rectSourcePresent, &rectDestinationPresent,
                       &rectSourceNext,    &rectDestinationNext);
        QPainter painter(pShownImage);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(rectDestinationNext, *pNextImageToShow, rectSourceNext);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(rectDestinationPresent, *pPresentImageToShow, rectSourcePresent);
        painter.end();
    }
    else if (transitionType == transition_Fade) {
        QPainter painter(pShownImage);
        qreal opacity = qreal(transitionStepNumber)/qreal(transitionGranularity);
        painter.setOpacity(opacity);
        painter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.drawImage(0, 0, *pNextImageToShow);
        painter.setOpacity(1.0-opacity);
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
        painter.drawImage(0, 0, *pPresentImageToShow);
        painter.end();
    }
    setPixmap(QPixmap::fromImage(*pShownImage));
}
