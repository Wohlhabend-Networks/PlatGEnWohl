/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2014 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifndef ITEM_DOOR_H
#define ITEM_DOOR_H

#include <QGraphicsItem>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSceneContextMenuEvent>
#include <QString>
#include <QPoint>
#include <QObject>
#include <QGraphicsItem>
#include <QPainter>
#include <QTimer>
#include <math.h>
#include <QMenu>

#include "lvlscene.h"
#include "../file_formats/lvl_filedata.h"

class ItemDoor : public QObject, public QGraphicsRectItem
{
    Q_OBJECT
public:
    ItemDoor(QGraphicsRectItem *parent=0);
    ~ItemDoor();

    void setDoorData(LevelDoors inD, int doorDir, bool init=false);
    void setContextMenu(QMenu &menu);
    void setScenePoint(LvlScene *theScene);

    int direction;
    enum doorDirect{
        D_Entrance=0,
        D_Exit
    };

    QRectF boundingRect() const;

    QMenu *ItemMenu;
//    QGraphicsScene * scene;
//    QGraphicsRectItem * image;

    void setLayer(QString layer);

    void arrayApply();
    void removeFromArray();

    LevelDoors doorData;

    int gridSize;
    int gridOffsetX;
    int gridOffsetY;
    QSize waterSize;

    //Locks
    bool isLocked;
    void setLocked(bool lock);

protected:
    virtual void contextMenuEvent( QGraphicsSceneContextMenuEvent * event );
    virtual void mousePressEvent ( QGraphicsSceneMouseEvent * mouseEvent );
    //virtual void mouseReleaseEvent( QGraphicsSceneMouseEvent * event);

private:
    LvlScene * scene;

    QGraphicsItemGroup * grp;
    QGraphicsTextItem * doorLabel;

};

#endif // ITEM_DOOR_H
