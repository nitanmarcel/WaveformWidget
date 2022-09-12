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
    Q_PROPERTY( QColor waveformBackgroundColor MEMBER m_waveformBackgroundColor );
    Q_OBJECT
public:
    WaveformWidget(QWidget *parent = nullptr);
    ~WaveformWidget();
    void setSource(QFileInfo *fileName);
    void resetFile(QFileInfo *fileName);
    enum FileHandlingMode {FULL_CACHE, DISK_MODE};
    void setColor(QColor color);
    void setFileHandlingMode(FileHandlingMode mode);
    void setClickable(bool clickable);
    FileHandlingMode getFileHandlingMode();

protected:
    virtual void resizeEvent(QResizeEvent *);
    void mouseMoveEvent(QMouseEvent *event) override;
    void mousePressEvent(QMouseEvent *event);


private:
    AudioUtil *m_srcAudioFile;
    FileHandlingMode m_currentFileHandlingMode;
    vector<double> m_peakVector;
    vector<double> m_dataVector;
    QString m_audioFilePath;
    double m_padding;
    QSize m_lastSize;
    QColor m_waveformColor { Qt::blue };
    QColor m_progressColor { QColor(246, 134, 86) };
    QColor m_waveformBackgroundColor { Qt::transparent };
    double m_scaleFactor;
    QPixmap m_pixMap;
    QLabel *m_pixMapLabel;
    bool m_is_clickable;
    qreal m_lastDrawnValue;
    QTimer *m_paintTimer;
    bool m_shouldRecalculatePeaks;
    bool m_isRecalculatingPeaks;
    bool m_isClickHold;

    void recalculatePeaks();
    void overviewDraw();
    int mouseEventPosition(const QMouseEvent *event) const;
signals:
  void barClicked(int);
};

#endif // WAVEFORMWIDGET_H
