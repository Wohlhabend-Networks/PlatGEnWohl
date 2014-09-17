/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2014 Vitaly Novichkov <admin@wohlnet.ru>
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

#include "lvlscene.h"
#include "../edit_level/level_edit.h"

#include "item_block.h"
#include "item_bgo.h"
#include "item_npc.h"
#include "item_water.h"
#include "item_door.h"

#include "../common_features/logger.h"

#include "../common_features/timecounter.h"

//static bool xxx=false;
QGraphicsItem * LvlScene::itemCollidesWith(QGraphicsItem * item, QList<QGraphicsItem *> *itemgrp)
{
    qreal leftA, leftB;
    qreal rightA, rightB;
    qreal topA, topB;
    qreal bottomA, bottomB;
    //qreal betweenZ;

    //xxx=!xxx;

    QList<QGraphicsItem *> collisions;

    //TimeCounter t;
    //t.start();
    //if(xxx)
    // ~15 ms on big maps
    if(itemgrp && !itemgrp->isEmpty())
        collisions = *itemgrp;
    else
        collisions = this->items(
                QRectF(item->scenePos().x()-10, item->scenePos().y()-10,
                item->data(9).toReal()+20, item->data(10).toReal()+20 ),
                Qt::IntersectsItemBoundingRect);


    //else

        // ~32 ms on big maps
        //collisions = item->collidingItems(Qt::IntersectsItemBoundingRect);

    //WriteToLog(QtDebugMsg, QString("Collision %1 in %2").arg(xxx).arg(t.current()));
    //t.stop();


    foreach (QGraphicsItem * it, collisions)
    {
            if(it == item)
                 continue;
            if(!it->isVisible())
                continue;
            if(item->data(0).toString()=="Water")
                return NULL;
            if(item->data(0).toString()=="Door_exit")
                return NULL;
            if(item->data(0).toString()=="Door_enter")
                return NULL;

            if(
               (it->data(0).toString()!="Block")&&
               (it->data(0).toString()!="BGO")&&
               (it->data(0).toString()!="NPC")
                    ) continue;

          if(item->data(0).toString()=="NPC")
          {
              if( item->data(8).toBool() ) // Disabled collisions with other NPCs
              {
                  if(item->data(1).toInt()!=it->data(1).toInt()) continue;
              }
              else
              {
                  if(
                          (
                           (it->data(0).toString()=="Block")
                           &&(!item->data(7).toBool()) //BlockCollision
                           )
                          ||
                          (
                           (it->data(0).toString()=="NPC")
                           &&(it->data(8).toBool()) //NpcCollision
                           )
                          ||
                          ((it->data(0).toString()!="NPC")&&(it->data(0).toString()!="Block"))
                     )
                            continue;
              }

          }
          else
          if(item->data(0).toString()=="Block")
          {
              if(
                      (
                       (it->data(0).toString()=="NPC")
                       &&(!it->data(7).toBool()) //BlockCollision
                       ) ||
                      ((it->data(0).toString()!="NPC")&&(it->data(0).toString()!="Block"))
                 )
                        continue;

          }
          else
                if(item->data(0).toString()!=it->data(0).toString()) continue;


          if(item->data(3).toString()=="sizable")
          {   // Don't collide with sizable block
              #ifdef _DEBUG_
              WriteToLog(QtDebugMsg, QString("sizable block") );
              #endif
              continue;
          }

          if(it->data(3).toString()=="sizable") continue; // Don't collide with sizable block

          if(item->data(0).toString()=="BGO")
            if(item->data(1).toInt()!=it->data(1).toInt()) continue;

          leftA = item->scenePos().x();
          rightA = item->scenePos().x()+item->data(9).toReal();
          topA = item->scenePos().y();
          bottomA = item->scenePos().y()+item->data(10).toReal();

          leftB = it->scenePos().x();
          rightB = it->scenePos().x()+it->data(9).toReal();
          topB = it->scenePos().y();
          bottomB = it->scenePos().y()+it->data(10).toReal();

          #ifdef _DEBUG_
          WriteToLog(QtDebugMsg, QString("Collision check -> Item1 L%1 | T%2 | R%3 |  B%4")
                     .arg(leftA).arg(topA).arg(rightA).arg(bottomA));
          WriteToLog(QtDebugMsg, QString("Collision check -> Item2 R%3 | B%4 | L%1 |  T%2")
                     .arg(leftB).arg(topB).arg(rightB).arg(bottomB));

          WriteToLog(QtDebugMsg, QString("Collision check -> Item1 W%1, H%2")
                     .arg(item->data(9).toReal()).arg(item->data(10).toReal()));
          WriteToLog(QtDebugMsg, QString("Collision check -> Item2 W%1, H%2")
                     .arg(it->data(9).toReal()).arg(it->data(10).toReal()));

          WriteToLog(QtDebugMsg, QString("Collision check -> B%1 <= T%2 -> %3")
                     .arg(bottomA).arg(topB).arg(bottomA <= topB));
          WriteToLog(QtDebugMsg, QString("Collision check -> T%1 >= B%2 -> %3")
                     .arg(topA).arg(bottomB).arg(topA >= bottomB));
          WriteToLog(QtDebugMsg, QString("Collision check -> R%1 <= L%2 -> %3")
                     .arg(rightA).arg(leftB).arg(rightA <= leftB));
          WriteToLog(QtDebugMsg, QString("Collision check -> L%1 >= R%2 -> %3")
                     .arg(leftA).arg(rightB).arg(leftA >= rightB));

          //Collision check -> Item1 L-199776 | T-200288 | R-199744 |  B-200256
          //Collision check -> Item2 R-199744 | B-200288 | L-199776 |  T-200320
          #endif

             if( bottomA <= topB )
             { continue; }
             if( topA >= bottomB )
             { continue; }
             if( rightA <= leftB )
             { continue; }
             if( leftA >= rightB )
             { continue; }

         return it; // Collision found!
    }
    return NULL;
}

QGraphicsItem * LvlScene::itemCollidesCursor(QGraphicsItem * item)
{
    QList<QGraphicsItem *> collisions = collidingItems(item, Qt::IntersectsItemBoundingRect);
    foreach (QGraphicsItem * it, collisions) {
            if (it == item)
                 continue;

            //skip invisible items
            if(!it->isVisible()) continue;

            //skip locked items
            if((it->data(0).toString()=="Block"))
            {
                if((lock_block)|| dynamic_cast<ItemBlock*>(it)->isLocked) continue;
            }
            else
            if((it->data(0).toString()=="BGO"))
            {
                if((lock_bgo)|| dynamic_cast<ItemBGO*>(it)->isLocked) continue;
            }
            else
            if((it->data(0).toString()=="NPC"))
            {
                if((lock_npc)|| dynamic_cast<ItemNPC*>(it)->isLocked) continue;
            }
            else
            if((it->data(0).toString()=="Water"))
            {
                if((lock_water)|| dynamic_cast<ItemWater*>(it)->isLocked) continue;
            }
            else
            if((it->data(0).toString()=="Door_enter")||(it->data(0).toString()=="Door_exit"))
            {
                if((lock_door)|| dynamic_cast<ItemDoor*>(it)->isLocked) continue;
            }

            if( (
                    (it->data(0).toString()=="Block")||
                    (it->data(0).toString()=="BGO")||
                    (it->data(0).toString()=="NPC")||
                    (it->data(0).toString()=="Door_exit")||
                    (it->data(0).toString()=="Door_enter")||
                    (it->data(0).toString()=="Water")||
                    (it->data(0).toString()=="player1")||
                    (it->data(0).toString()=="player2")
              )&&(it->isVisible() ) )
                return it;
    }
    return NULL;
}
