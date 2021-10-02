#pragma once
#ifndef IMAGE_CALIBRATOR_H
#define IMAGE_CALIBRATOR_H

#include <QDialog>
#include <QPixmap>
#include <QImage>
#include <QList>
#include <QGraphicsRectItem>
#include <QGraphicsPixmapItem>
#include "main/calibration.h"

namespace Ui
{
    class ImageCalibrator;
}

class CalibrationMain;
class Matrix;

class ImageCalibrator : public QDialog
{
    Q_OBJECT

    QList<QVector<CalibrationFrame > > m_imgOffsets;
    Calibration *m_conf = nullptr;
    CalibrationMain *m_mw = nullptr;

    Matrix *m_matrix = nullptr;
public:
    explicit ImageCalibrator(Calibration *conf, QWidget *parent = nullptr);
    ~ImageCalibrator();

    bool init(QString imgPath);
    void unInit();
    QString m_targetPath;

    bool hitboxModified() const;
    bool hitboxNeedSave() const;

protected:
    void closeEvent(QCloseEvent *e);

private slots:
    void on_FrameX_valueChanged(int arg1);
    void on_FrameY_valueChanged(int arg1);
    void frameSelected(int x, int y);
    void referenceSelected(int x, int y);

    void on_OffsetX_valueChanged(int arg1);
    void on_OffsetY_valueChanged(int arg1);

    void on_Matrix_clicked();
    void on_Reset_clicked();
    void makeBackup();
    void on_WritePNG_GIF_clicked();
    void on_WritePNG_clicked();
    void on_WriteGIF_clicked();
    void updateControls();
    void updateScene();
    void saveCalibrates();
    void loadCalibrates();
    QPixmap generateTarget();
    QPixmap getFrame(int x, int y, int oX, int oY, int cW, int cH);

    void on_CropW_valueChanged(int arg1);
    void on_CropH_valueChanged(int arg1);

    void on_refOpacity_sliderMoved(int position);
    void on_refClear_clicked();

private:
    int m_frmX;
    int m_frmY;

    QPixmap m_sprite;
    QPixmap m_spriteOrig;

    QString m_pngPath;
    QString m_gifPath;
    QString m_gifPathM;
    QString m_backupPath;
    QString m_iniPath;

    bool m_hitboxModified = false;
    bool m_hitboxNeedSave = false;

    Ui::ImageCalibrator *ui;
    bool m_lockUI;
};

#endif // IMAGE_CALIBRATOR_H
