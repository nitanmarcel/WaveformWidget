#ifndef WAVEFORMWIDGET_H
#define WAVEFORMWIDGET_H

#include "AudioUtil.h"
#include "MathUtil.h"

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <vector>

#include <sndfile.h>

#include <QSize>
#include <QColor>
#include <QWidget>
#include <QAbstractSlider>
#include <QPainter>
#include <QPaintEvent>
#include <QDebug>
#include <QPoint>
#include <QResizeEvent>
#include <QProcess>
#include <QFileInfo>
#include <QMouseEvent>
#include <QPixmap>
#include <QLabel>
#include <QTimer>

/*!
    \file WaveformWidget.h
    \brief WaveformWidget header file.
*/

using namespace std;

/*!
\brief A Qt widget to display the waveform of an audio file.


*/
class WaveformWidget : public QAbstractSlider
{
    Q_PROPERTY( QColor waveformColor MEMBER m_waveformColor );
    Q_PROPERTY( QColor waveformProgressColor MEMBER m_progressColor );
    Q_OBJECT
public:
    WaveformWidget(QWidget *parent = nullptr);
    ~WaveformWidget();
    void setSource(QFileInfo *fileName);
    void setFfmpegConvertToMono(bool convert);
    void resetFile(QFileInfo *fileName);
    enum FileHandlingMode {FULL_CACHE, DISK_MODE};
    void setColor(QColor color);
    void setFileHandlingMode(FileHandlingMode mode);
    void setFfmpegPath(QString path);
    void setClickable(bool clickable);
    FileHandlingMode getFileHandlingMode();

protected:
    virtual void resizeEvent(QResizeEvent *);
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event);


private:
    AudioUtil *srcAudioFile;
    FileHandlingMode currentFileHandlingMode;
    vector<double> peakVector;
    vector<double> dataVector;
    QString audioFilePath;
    double max_peak;
    double padding;
    bool ffmpegConvertToMono;
    QSize lastSize;
    QColor m_waveformColor { Qt::blue };
    QColor m_progressColor { QColor(246, 134, 86) };

    QProcess *convert_process;

    double scaleFactor;

    QString ffmpeg_path;

    void recalculatePeaks();
    void overviewDraw();
    QPixmap pixMap;
    QLabel *pixMapLabel;

    void convertAudio(QFileInfo *fileName);
    void setSourceFromConverted();
    int mouseEventPosition(const QMouseEvent *event) const;

    bool is_clickable;
    bool first_draw;
    qreal lastDrawnValue;
    QPaintEvent *lastPaintEvent;
    QTimer *paintTimer;
    bool shouldRecalculatePeaks;
signals:
  void barClicked(int);
};

#endif // WAVEFORMWIDGET_H
