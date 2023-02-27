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
#ifndef TIMEOUTWINDOW_H
#define TIMEOUTWINDOW_H

#include <QObject>
#include <QWidget>
#include <QTimer>
#include <QLabel>

class TimeoutWindow : public QWidget
{
    Q_OBJECT

public:
    explicit TimeoutWindow(QWidget *parent = nullptr);
    ~TimeoutWindow();

public:
    void startTimeout(int msecTime);
    void stopTimeout();

public slots:
    void updateTime();

signals:
    void doneTimeout();

private:
    QPalette           panelPalette;
    QLinearGradient    panelGradient;
    QBrush             panelBrush;
    QLabel             myLabel;
    QTimer             TimerTimeout;
    QTimer             TimerUpdate;
};

#endif // TIMEOUTWINDOW_H
