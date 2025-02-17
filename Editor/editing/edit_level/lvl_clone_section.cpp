/*
 * Platformer Game Engine by Wohlstand, a free platform for game making
 * Copyright (c) 2014-2025 Vitaly Novichkov <admin@wohlnet.ru>
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

#include <QMessageBox>
#include <QtDebug>

#include <common_features/util.h>
#include <PGE_File_Formats/file_formats.h>

#include "lvl_clone_section.h"
#include "ui_lvl_clone_section.h"

LvlCloneSection::LvlCloneSection(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::LvlCloneSection)
{
    ui->setupUi(this);
}


void LvlCloneSection::addLevelList(QList<LevelEdit * > _levels, LevelEdit *active)
{
    levels = _levels;
    int ActiveIndex = 0;

    ui->FileList_src->clear();
    ui->FileList_dst->clear();

    foreach(LevelEdit *x, levels)
    {
        ui->FileList_src->addItem(x->windowTitle(), QVariant::fromValue(x));
        ui->FileList_dst->addItem(x->windowTitle(), QVariant::fromValue(x));

        if(active == x)
            ActiveIndex = ui->FileList_src->count() - 1;
    }

    ui->FileList_src->setCurrentIndex(ActiveIndex);
    ui->FileList_dst->setCurrentIndex(ActiveIndex);
}



LvlCloneSection::~LvlCloneSection()
{
    delete ui;

}

void LvlCloneSection::on_FileList_src_currentIndexChanged(int index)
{
    ui->SectionList_src->setEnabled(index >= 0);
    if(index < 0) return;
    LevelEdit *x = ui->FileList_src->currentData().value<LevelEdit *>();
    util::memclear(ui->SectionList_src);
    QListWidgetItem *item = nullptr;

    for(LevelSection &y : x->LvlData.sections)
    {
        item = new QListWidgetItem(tr("Section") +
                                   QString(" %1 %2")
                                   .arg(y.id)
                                   .arg(
                                       ((y.size_bottom == 0) && (y.size_left == 0) &&
                                        (y.size_top == 0) && (y.size_right == 0)) ?
                                       tr("[Uninitialized]") : ""
                                   )
                                  );
        item->setData(3, QString::number(y.id));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        ui->SectionList_src->addItem(item);
    }
}


void LvlCloneSection::on_FileList_dst_currentIndexChanged(int index)
{
    ui->SectionList_dst->setEnabled(index >= 0);
    if(index < 0) return;
    LevelEdit *x = ui->FileList_dst->currentData().value<LevelEdit *>();
    util::memclear(ui->SectionList_dst);
    QListWidgetItem *item = nullptr;

    for(LevelSection &y : x->LvlData.sections)
    {
        item = new QListWidgetItem(tr("Section") + QString(" %1 %2").arg(y.id)
                                   .arg(
                                       ((y.size_bottom == 0) && (y.size_left == 0) && (y.size_top == 0) && (y.size_right == 0)) ?
                                       tr("") :
                                       tr("[Used]")
                                   )
                                  );
        item->setData(3, QString::number(y.id));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);

        ui->SectionList_dst->addItem(item);
    }

    //Init new section
    {
        item = new QListWidgetItem(tr("Initialize new section") + QString(" %1").arg(x->LvlData.sections.size()));
        item->setData(3, QString::number(x->LvlData.sections.size()));
        item->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        ui->SectionList_dst->addItem(item);
    }

}



void LvlCloneSection::on_buttonBox_accepted()
{
    if(ui->SectionList_src->selectedItems().isEmpty() ||
       ui->SectionList_dst->selectedItems().isEmpty())
    {
        QMessageBox::warning(this,
                             tr("Sections aren't selected"),
                             tr("Source and Destination sections should be selected!"));
        return;
    }

    clone_margin = ui->padding_src->value();

    LevelSection tmps;
    tmps = FileFormats::CreateLvlSection();
    tmps.id = -1;

    clone_source = ui->FileList_src->currentData().value<LevelEdit *>();
    clone_source_id = ui->SectionList_src->selectedItems().first()->data(3).toInt();

    for(LevelSection &x : clone_source->LvlData.sections)
    {
        if(x.id == clone_source_id)
        {
            tmps = x;
            break;
        }
    }

    //        qDebug() << "source " << clone_source_id << tmps.size_bottom << tmps.size_left
    //                                                 << tmps.size_right << tmps.size_top;

    if((tmps.size_bottom == 0) &&
       (tmps.size_left == 0) &&
       (tmps.size_top == 0) &&
       (tmps.size_right == 0))
    {
        QMessageBox::warning(this,
                             tr("Empty section"),
                             tr("Source section is empty!\nPlease select another section, or clear."));
        return;
    }

    clone_target = ui->FileList_dst->currentData().value<LevelEdit *>();
    clone_target_id = ui->SectionList_dst->selectedItems().first()->data(3).toInt();

    if(clone_target_id >= clone_target->LvlData.sections.size())
    {
        tmps = FileFormats::CreateLvlSection();
        tmps.id = clone_target_id;
        clone_target->LvlData.sections.push_back(tmps);
    }
    else
        for(LevelSection &x : clone_target->LvlData.sections)
        {
            if(x.id == clone_target_id)
            {
                tmps = x;
                break;
            }
        }
    //        qDebug() << "target " << clone_target_id << tmps.size_bottom << tmps.size_left
    //                                                 << tmps.size_right << tmps.size_top;

    if((tmps.size_bottom != 0) ||
       (tmps.size_left != 0) ||
       (tmps.size_top != 0) ||
       (tmps.size_right != 0))
    {
        int reply = QMessageBox::warning(this,
                                         tr("Section is used"),
                                         tr("Destination section is in use, therefore it will be "
                                            "overridden with removing of all it's objects.\n"
                                            "Do you want to continue?"),
                                         QMessageBox::Yes | QMessageBox::Abort);
        if(reply != QMessageBox::Yes)
            return;
        override_destinition = true;
    }

    this->accept();
}
