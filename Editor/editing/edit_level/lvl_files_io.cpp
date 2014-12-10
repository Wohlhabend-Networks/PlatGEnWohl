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

#include <QtWidgets>

#include "level_edit.h"
#include <ui_leveledit.h>

#include "../../common_features/app_path.h"

#include "../../file_formats/file_formats.h"
#include "../_scenes/level/lvl_scene.h"

#include "../../common_features/logger.h"
#include "../../common_features/util.h"

#include "../../common_features/mainwinconnect.h"
#include "../../audio/music_player.h"
#include "../../main_window/global_settings.h"
#include "../_dialogs/savingnotificationdialog.h"


void LevelEdit::newFile(dataconfigs &configs, LevelEditingSettings options)
{
    static int sequenceNumber = 1;

    isUntitled = true;
    curFile = tr("Untitled %1").arg(sequenceNumber++);
    setWindowTitle(QString(curFile).replace("&", "&&&"));
    LvlData = FileFormats::dummyLvlDataArray();
    LvlData.metaData.script = new ScriptHolder;
    LvlData.untitled = true;
    StartLvlData = LvlData;

    ui->graphicsView->setBackgroundBrush(QBrush(Qt::darkGray));

    //Check if data configs exists
    if( configs.check() )
    {
        WriteToLog(QtCriticalMsg, QString("Error! *.INI configs not loaded"));

        QMessageBox::warning(this, tr("Configurations not loaded"),
                             tr("Cannot create level file:\nbecause object configurations are not loaded\n."
                                "Please check that the ""config/SMBX"" directory exists and contains the *.INI files with object settings."));

        WriteToLog(QtCriticalMsg, QString(" << close subWindow"));

        this->close();

        WriteToLog(QtCriticalMsg, QString(" << closed, return false"));
        return;
    }

    scene = new LvlScene(ui->graphicsView, configs, LvlData);
    scene->opts = options;

    scene->InitSection(0);
    scene->setPlayerPoints();
    scene->drawSpace();
    scene->buildAnimators();

    if(!sceneCreated)
    {
        ui->graphicsView->setScene(scene);
        sceneCreated = true;
        connect(scene, SIGNAL(screenshotSizeCaptured()), this, SLOT(ExportingReady()));
    }

    if(options.animationEnabled) scene->startBlockAnimation();
    setAutoUpdateTimer(31);
}


bool LevelEdit::save(bool savOptionsDialog)
{
    if (isUntitled) {
        return saveAs(savOptionsDialog);
    } else {
        return saveFile(curFile);
    }
}

namespace lvl_file_io
{
    bool isSMBX64limit=false;
}

bool LevelEdit::saveAs(bool savOptionsDialog)
{
    using namespace lvl_file_io;
    bool makeCustomFolder = false;

    if(savOptionsDialog){
        SavingNotificationDialog* sav = new SavingNotificationDialog(false);
        sav->setSavingTitle(tr("Please enter a level title for '%1'!").arg(userFriendlyCurrentFile()));
        sav->setWindowTitle(tr("Saving ") + userFriendlyCurrentFile());
        QLineEdit* lvlNameBox = new QLineEdit();
        QCheckBox* mkDirCustom = new QCheckBox();
        mkDirCustom->setText(QString(""));
        sav->addUserItem(tr("Level title: "),lvlNameBox);
        sav->addUserItem(tr("Make custom folder"), mkDirCustom);
        sav->setAdjustSize(400,150);
        lvlNameBox->setText(LvlData.LevelName);
        if(sav->exec() == QDialog::Accepted){
            LvlData.LevelName = lvlNameBox->text();
            makeCustomFolder = mkDirCustom->isChecked();
            lvlNameBox->deleteLater();
            mkDirCustom->deleteLater();
            sav->deleteLater();
            if(sav->savemode == SavingNotificationDialog::SAVE_CANCLE){
                return false;
            }
        }else{
            return false;
        }
    }

    bool isNotDone=true;
    QString fileName = (isUntitled)?GlobalSettings::savePath+QString("/")+
                                    (LvlData.LevelName.isEmpty()?curFile:util::filePath(LvlData.LevelName)):curFile;

    QString fileSMBX64="SMBX64 (1.3) Level file (*.lvl)";
    QString filePGEX="Extended Level file (*.lvlx)";

    QString selectedFilter;
    if(fileName.endsWith(".lvlx", Qt::CaseInsensitive))
        selectedFilter = filePGEX;
    else
        selectedFilter = fileSMBX64;

    QString filter =
            fileSMBX64+";;"+
            filePGEX;

    bool ret;

    RetrySave:

    isSMBX64limit=false;
    isNotDone=true;
    while(isNotDone)
    {
        fileName = QFileDialog::getSaveFileName(this, tr("Save As"), fileName, filter, &selectedFilter);

        if (fileName.isEmpty())
            return false;

        if( (!fileName.endsWith(".lvl", Qt::CaseInsensitive)) && (!fileName.endsWith(".lvlx", Qt::CaseInsensitive)) )
        {
            QMessageBox::warning(this, tr("Extension is not set"),
               tr("File Extension isn't defined, please enter file extension!"), QMessageBox::Ok);
            continue;
        }

        if(makeCustomFolder)
        {
            QDir dir = fileName.section("/",0,-2);
            dir.mkdir(fileName.section("/",-1,-1).section(".",0,0));
        }
        isNotDone=false;
    }

    ret = saveFile(fileName);
    if(isSMBX64limit) goto RetrySave;

    return ret;
}

bool LevelEdit::saveFile(const QString &fileName, const bool addToRecent)
{
    using namespace lvl_file_io;

    if( (!fileName.endsWith(".lvl", Qt::CaseInsensitive)) && (!fileName.endsWith(".lvlx", Qt::CaseInsensitive)) )
    {
        QMessageBox::warning(this, tr("Extension is not set"),
           tr("File Extension isn't defined, please enter file extension!"), QMessageBox::Ok);
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    //Mark stars
    for(int q=0; q< LvlData.npc.size(); q++)
    {
       if( LvlData.npc[q].id < (unsigned long) MainWinConnect::pMainWin->configs.index_npc.size() )
           LvlData.npc[q].is_star =
                   MainWinConnect::pMainWin->configs.
                   main_npc[MainWinConnect::pMainWin->configs.index_npc[LvlData.npc[q].id].gi-1].is_star;
    }

    // ////////////////////// Write SMBX64 LVL //////////////////////////////
    if(fileName.endsWith(".lvl", Qt::CaseInsensitive))
    {
        //SMBX64 Standard check

        isSMBX64limit=false;
        //Blocks limit
        if(LvlData.blocks.size()>16384)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 blocks\n"
                "The maximum number of blocks is %2.\n\n"
                "Please remove excess blocks from this level or save file into LVLX format.")
             .arg(LvlData.blocks.size()).arg(16384), QMessageBox::Ok);
            isSMBX64limit=true;
        }
        //BGO limits
        if(LvlData.bgo.size()>8000)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 Background Objects\n"
                "The maximum number of Background Objects is %2.\n\n"
                "Please remove excess Background Objects from this level or save file into LVLX format.")
             .arg(LvlData.bgo.size()).arg(8000), QMessageBox::Ok);
            isSMBX64limit=true;
        }
        //NPC limits
        if(LvlData.npc.size()>5000)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 Non-Playable Characters\n"
                "The maximum number of Non-Playable Characters is %2.\n\n"
                "Please remove excess Non-Playable Characters from this level or save file into LVLX format.")
             .arg(LvlData.npc.size()).arg(5000), QMessageBox::Ok);
            isSMBX64limit=true;
        }
        //Warps limits
        if(LvlData.doors.size()>199)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 Warps\n"
                "The maximum number of Warps is %2.\n\n"
                "Please remove excess Warps from this level or save file into LVLX format.")
             .arg(LvlData.doors.size()).arg(199), QMessageBox::Ok);
            isSMBX64limit=true;
        }
        //Physical Environment zones
        if(LvlData.physez.size()>450)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 Water Boxes\n"
                "The maximum number of Water Boxes is %2.\n\n"
                "Please remove excess Water Boxes from this level or save file into LVLX format.")
             .arg(LvlData.physez.size()).arg(450), QMessageBox::Ok);
            isSMBX64limit=true;
        }
        //Layers limits
        if(LvlData.layers.size()>100)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 Layers\n"
                "The maximum number of Layers is %2.\n\n"
                "Please remove excess Layers from this level or save file into LVLX format.")
             .arg(LvlData.layers.size()).arg(100), QMessageBox::Ok);
            isSMBX64limit=true;
        }
        //Events limits
        if(LvlData.events.size()>100)
        {
            QMessageBox::warning(this, tr("The SMBX64 limit has been exceeded"),
             tr("SMBX64 standard isn't allow to save %1 Events\n"
                "The maximum number of Events is %2.\n\n"
                "Please remove excess Events from this level or save file into LVLX format.")
             .arg(LvlData.events.size()).arg(100), QMessageBox::Ok);
            isSMBX64limit=true;
        }

        if(isSMBX64limit)
        {
            QApplication::restoreOverrideCursor();
            return false;
        }

        //set SMBX64 specified option to BGO
        for(int q=0; q< LvlData.bgo.size(); q++)
        {
            if(LvlData.bgo[q].smbx64_sp < 0)
            {
                if( LvlData.bgo[q].id < (unsigned long) MainWinConnect::pMainWin->configs.index_bgo.size() )
                    LvlData.bgo[q].smbx64_sp_apply = MainWinConnect::pMainWin->configs.index_bgo[LvlData.bgo[q].id].smbx64_sp;
            }
            else
                LvlData.bgo[q].smbx64_sp_apply = LvlData.bgo[q].smbx64_sp;
            //WriteToLog(QtDebugMsg, QString("BGO SMBX64 sort -> ID-%1 SORT-%2").arg(LvlData.bgo[q].id).arg(LvlData.bgo[q].smbx64_sp) );
        }
        LvlData.smbx64strict = true; //Enable SMBX64 standard strict mode

        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Text))
        {
            QMessageBox::warning(this, tr("File save error"),
                                 tr("Cannot save file %1:\n%2.")
                                 .arg(fileName)
                                 .arg(file.errorString()));
            return false;
        }

        QTextStream out(&file);
        out << FileFormats::WriteSMBX64LvlFile(LvlData);
        file.close();

        //save additional meta data
        if(!LvlData.metaData.bookmarks.isEmpty())
        {
            file.setFileName(fileName+".meta");
            if (!file.open(QFile::WriteOnly | QFile::Text))
            {
                QMessageBox::warning(this, tr("File save error"),
                                     tr("Cannot save file %1:\n%2.")
                                     .arg(fileName+".meta")
                                     .arg(file.errorString()));
                return false;
            }
            QTextStream out(&file);
            out.setCodec("UTF-8");
            out << FileFormats::WriteNonSMBX64MetaData(LvlData.metaData);
            file.close();
        }

        GlobalSettings::savePath = QFileInfo(fileName).path();
    }
    // //////////////////////////////////////////////////////////////////////

    // ////////////////// Write Extended LVL file (LVLX)/////////////////////
    else if(fileName.endsWith(".lvlx", Qt::CaseInsensitive))
    {
        LvlData.smbx64strict = false; //Disable strict mode

        QFile file(fileName);
        if (!file.open(QFile::WriteOnly | QFile::Text)) {
            QMessageBox::warning(this, tr("File save error"),
                                 tr("Cannot save file %1:\n%2.")
                                 .arg(fileName)
                                 .arg(file.errorString()));
            return false;
        }
        QTextStream out(&file);
        out.setCodec("UTF-8");
        out << FileFormats::WriteExtendedLvlFile(LvlData);
        file.close();
        GlobalSettings::savePath = QFileInfo(fileName).path();
    }
    // //////////////////////////////////////////////////////////////////////

    QApplication::restoreOverrideCursor();
    setCurrentFile(fileName);


    LvlData.modified = false;
    LvlData.untitled = false;
    if(addToRecent)
    {
        MainWinConnect::pMainWin->AddToRecentFiles(fileName);
        MainWinConnect::pMainWin->SyncRecentFiles();
    }

    return true;
}


bool LevelEdit::loadFile(const QString &fileName, LevelData FileData, dataconfigs &configs, LevelEditingSettings options)
{
    QFile file(fileName);
    LvlData = FileData;
    LvlData.metaData.script = new ScriptHolder;
    LvlData.modified = false;
    LvlData.untitled = false;
    if (!file.open(QFile::ReadOnly | QFile::Text)) {
        QMessageBox::warning(this, tr("Read file error"),
                             tr("Cannot read file %1:\n%2.")
                             .arg(fileName)
                             .arg(file.errorString()));
        return false;
    }
    StartLvlData = LvlData; //Save current history for made reset
    setCurrentFile(fileName);

    ui->graphicsView->setBackgroundBrush(QBrush(Qt::darkGray));

    //Check if data configs exists
    if( configs.check() )
    {
        WriteToLog(QtCriticalMsg, QString("Error! *.INI configs not loaded"));

        QMessageBox::warning(this, tr("Configurations not loaded"),
                             tr("Cannot open level file %1:\nbecause object configurations are not loaded\n."
                                "Please check that the ""config/SMBX"" directory exists and contains the *.INI files with object settings.")
                             .arg(fileName));

        WriteToLog(QtCriticalMsg, QString(" << close subWindow"));

        this->close();

        WriteToLog(QtCriticalMsg, QString(" << closed, return false"));
        return false;
    }

    WriteToLog(QtDebugMsg, QString(">>Starting to load file"));

    //Declaring of the scene
    scene = new LvlScene(ui->graphicsView, configs, LvlData);

    scene->opts = options;

    int DataSize=0;

    DataSize += 3;
    DataSize += 6; /*LvlData.sections.size()*2;
    DataSize += LvlData.bgo.size();
    DataSize += LvlData.blocks.size();
    DataSize += LvlData.npc.size();
    DataSize += LvlData.water.size();
    DataSize += LvlData.doors.size();*/

    QProgressDialog progress(tr("Loading level data"), tr("Abort"), 0, DataSize, this);
         progress.setWindowTitle(tr("Loading level data"));
         progress.setWindowModality(Qt::WindowModal);
         progress.setWindowFlags(Qt::Window | Qt::WindowTitleHint | Qt::CustomizeWindowHint | Qt::WindowStaysOnTopHint);
         progress.setFixedSize(progress.size());
         progress.setGeometry(QStyle::alignedRect(Qt::LeftToRight, Qt::AlignCenter, progress.size(), qApp->desktop()->availableGeometry()));
         progress.setMinimumDuration(500);
         //progress.setCancelButton(0);

    if(! DrawObjects(progress) )
    {
        LvlData.modified = false;
        this->close();
        return false;
    }

    QApplication::setOverrideCursor(Qt::WaitCursor);

    if( !progress.wasCanceled() )
        progress.close();

    QApplication::restoreOverrideCursor();

    setAutoUpdateTimer(31);

    setCurrentFile(fileName);
    LvlData.modified = false;
    LvlData.untitled = false;

    progress.deleteLater();

    return true;
}


void LevelEdit::documentWasModified()
{
    //LvlData.modified = true;
}

bool LevelEdit::maybeSave()
{
    if (LvlData.modified)
    {
        SavingNotificationDialog* sav = new SavingNotificationDialog(true);
        sav->setSavingTitle(tr("'%1' has been modified.\n"
                               "Do you want to save your changes?").arg(userFriendlyCurrentFile()));
        sav->setWindowTitle(userFriendlyCurrentFile()+tr(" not saved"));
        QLineEdit* lvlNameBox = new QLineEdit();
        sav->addUserItem(tr("Level title: "),lvlNameBox);
        sav->setAdjustSize(400,150);
        lvlNameBox->setText(LvlData.LevelName);
        if(sav->exec() == QDialog::Accepted){
            LvlData.LevelName = lvlNameBox->text();
            lvlNameBox->deleteLater();
            sav->deleteLater();
            if(sav->savemode == SavingNotificationDialog::SAVE_SAVE){
                return save(false);
            }else if(sav->savemode == SavingNotificationDialog::SAVE_CANCLE){
                return false;
            }
        }else{
            return false;
        }
    }

    return true;
}


//////// Common
QString LevelEdit::userFriendlyCurrentFile()
{
    return strippedName(curFile);
}

void LevelEdit::closeEvent(QCloseEvent *event)
{
    if(!sceneCreated)
    {
        event->accept();
        return;
    }
    else
        MainWinConnect::pMainWin->on_actionSelect_triggered();

    if(maybeSave()) {
        stopAutoUpdateTimer();

        if(MainWinConnect::pMainWin->subWins()<=1) //Stop music only if this subwindow - last
        {
            LvlMusPlay::musicForceReset = true;
            MainWinConnect::pMainWin->setMusicButton(false);
            MainWinConnect::pMainWin->setMusic(false);
        }

        scene->setMessageBoxItem(false);
        scene->clear();
        WriteToLog(QtDebugMsg, "!<-Cleared->!");
        scene->uBGOs.clear();
        scene->uBGs.clear();
        scene->uBlocks.clear();
        scene->uNPCs.clear();

        WriteToLog(QtDebugMsg, "!<-Delete animators->!");
        while(! scene->animates_BGO.isEmpty() )
        {
            SimpleAnimator* tmp = scene->animates_BGO.first();
            scene->animates_BGO.pop_front();
            if(tmp!=NULL) delete tmp;
        }
        while(! scene->animates_Blocks.isEmpty() )
        {
            SimpleAnimator* tmp = scene->animates_Blocks.first();
            scene->animates_Blocks.pop_front();
            if(tmp!=NULL) delete tmp;
        }
        while(! scene->animates_NPC.isEmpty() )
        {
            AdvNpcAnimator* tmp = scene->animates_NPC.first();
            scene->animates_NPC.pop_front();
            if(tmp!=NULL) delete tmp;
        }
        WriteToLog(QtDebugMsg, "!<-Delete scene->!");
        delete scene;
        sceneCreated=false;
        WriteToLog(QtDebugMsg, "!<-Deleted->!");
        //ui->graphicsView->cl
        event->accept();
    } else {
        event->ignore();
    }
}

void LevelEdit::setCurrentFile(const QString &fileName)
{
    curFile = QFileInfo(fileName).canonicalFilePath();
    isUntitled = false;
    LvlData.path = QFileInfo(fileName).absoluteDir().absolutePath();
    LvlData.filename = QFileInfo(fileName).baseName();
    LvlData.untitled = false;
    //document()->setModified(false);
    setWindowModified(false);
    setWindowTitle(QString(LvlData.LevelName=="" ? userFriendlyCurrentFile() : LvlData.LevelName).replace("&", "&&&"));
}

QString LevelEdit::strippedName(const QString &fullFileName)
{
    return QFileInfo(fullFileName).fileName();
}

