/*
 * Playble Character Calibrator, a free tool for playable character sprite setup tune
 * This is a part of the Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2017-2020 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once
#ifndef ANIMATE_H
#define ANIMATE_H

#include <QDialog>
#include "calibration_main.h"
#include "animation_scene.h"
#include "main/calibration.h"

namespace Ui {
class Animator;
}

class Animator : public QDialog
{
    Q_OBJECT

    Calibration *m_conf = nullptr;

    QString m_aniStyle;
    int     m_aniDir = 1;

    AnimationScene *m_aniScene = nullptr;

    void aniFindSet();

    void rebuildAnimationsList();

public:
    explicit Animator(Calibration &conf, QWidget *parent = nullptr);
    ~Animator();

protected:
    void keyPressEvent(QKeyEvent *e);

private slots:
    void on_EditAnimationBtn_clicked();

    void on_directLeft_clicked();
    void on_directRight_clicked();

    void on_FrameSpeed_valueChanged(int arg1);
    void on_animationsList_currentItemChanged(QListWidgetItem *item, QListWidgetItem *);

private:
    Ui::Animator *ui;
};

#endif // ANIMATE_H
