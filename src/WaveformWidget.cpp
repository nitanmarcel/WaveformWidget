#include "WaveformWidget.h"

#include <QStandardPaths>
#include <QDir>
#include <QToolTip>
#include <QDebug>
#include <QThread>
#include <QtConcurrent>

#define DEFAULT_PADDING 0.3
#define LINE_WIDTH 1
#define POINT_SIZE 5
#define DEFAULT_COLOR Qt::blue
#define INDIVIDUAL_SAMPLE_DRAW_TOGGLE_POINT 9.0

/*!
\file WaveformWidget.cpp
\brief WaveformWidget implementation file.
*/

/*!
    \mainpage WaveformWidget

    \brief This library provides a Qt widget for visualizing audio waveforms.<br><br>

    It depends on Erik de Castro Lopo's <a href='http://www.mega-nerd.com/libsndfile/'>libsndfile</a> and
    the <a href='http://qt.nokia.com/'>Qt framework</a>.  It has been tested on Linux (kernel version 2.6.31-21) under Gnome with
    version 4.6 of the Qt framework, but could well function in other environments.
    <br><br>
    A source archive can be found <a href='http://www.columbia.edu/~naz2106/WaveformWidget.tar.gz'>here</a>.
    <br><br>For build instructions, see the README.txt file contained in the top level directory of the source archive.
*/

/*!
\brief Constructs an instance of WaveformWidget.
@param filePath Valid path to a WAV file.
*/
WaveformWidget::WaveformWidget(QWidget *parent) : QAbstractSlider(parent),
    m_is_clickable(false)
{
    clearFocus();
    setFocusPolicy(Qt::NoFocus);

    this->m_srcAudioFile = new AudioUtil();
    this->m_pixMapLabel = new QLabel(this);
    this->m_pixMapLabel->show();
    this->m_shouldRecalculatePeaks = true;
    this->m_isRecalculatingPeaks = false;
    this->m_paintTimer = new QTimer(this);
    connect(this->m_paintTimer, &QTimer::timeout, this, &WaveformWidget::overviewDraw);
    m_paintTimer->setInterval(100);
    m_paintTimer->start();
}

/*The AudioUtil instance "m_srcAudioFile" is our only dynamically allocated object*/
WaveformWidget::~WaveformWidget()
{
    delete this->m_srcAudioFile;
}


void WaveformWidget::setSource(QFileInfo *fileName)
{
    if (this->m_hasBreakPoint)
        this->resetBreakPoint();
    this->m_currentFileHandlingMode = FULL_CACHE;
    this->resetFile(fileName);
    this->m_scaleFactor = -1.0;
    this->m_lastSize = this->size();
    this->m_padding = DEFAULT_PADDING;
}

void WaveformWidget::resetBreakPoint()
{
    m_updateBreakPointRequired = true;
    m_hasBreakPoint = false;
}

void WaveformWidget::setBreakPoint(int pos)
{
    m_breakPointPos = pos / (maximum() / width());
    m_updateBreakPointRequired = true;
}

int WaveformWidget::getBreakPoint()
{
    if (this->m_hasBreakPoint)
        return m_breakPointPos * (maximum() / width());
    else
        return 0;
}


void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
  if ((event->button() == Qt::RightButton) && m_is_clickable)
      if (event->x() != this->m_breakPointPos || event->x() > 5)
      {
          m_breakPointPos = event->x();
          this->m_hasBreakPoint = true;
          emit breakPointSet(mouseEventPosition(event));
          this->m_updateBreakPointRequired = true;
      }
      else
       {
          this->m_hasBreakPoint = false;
          this->m_breakPointPos = 0;
          emit breakPointRemoved();
          this->m_updateBreakPointRequired = true;
      }
  else if ((event->button() == Qt::LeftButton) && m_is_clickable)
      emit barClicked(event->x() > 5 ? mouseEventPosition(event) : 0);

  event->accept();
}


// Returns the position in milliseconds corresponding to the mouse position on the progress bar where the event occured
int WaveformWidget::mouseEventPosition(const QMouseEvent *event) const
{
  return event->x() * (maximum() / width());
}

void WaveformWidget::mouseMoveEvent(QMouseEvent *event)
{
  event->accept();
}

void WaveformWidget::setClickable(bool clickable)
{
  if (clickable)
    setCursor(Qt::PointingHandCursor);
  else {
    setCursor(Qt::ForbiddenCursor);
    if (underMouse())
      QToolTip::hideText();
  }

  setMouseTracking(clickable);

  m_is_clickable = clickable;
}



/*!
\brief Reset the audio file to be visualized by this instance of WaveformWidget.

An important consideration when invoking this function is the file-handling mode that you
have set for the current instance of WaveformWidget.  If the instance of WaveformWidget is in FULL_CACHE mode,
this function will take considerably longer to execute (posssibly as long as a few seconds for an audio file of several
minutes' duration) as the entirety of the audio file to be visualized
by the widget must be loaded into memory.
@param fileName Valid path to a WAV file
*/
void WaveformWidget::resetFile(QFileInfo *fileName)
{
    this->m_audioFilePath = fileName->canonicalFilePath();
    this->m_srcAudioFile->setFile(m_audioFilePath);

    switch(this->m_currentFileHandlingMode)
    {
        case FULL_CACHE:
            this->m_srcAudioFile->setFileHandlingMode(AudioUtil::FULL_CACHE);

        case DISK_MODE:
            this->m_srcAudioFile->setFileHandlingMode(AudioUtil::DISK_MODE);
    }

    this->m_peakVector.clear();
    this->m_dataVector.clear();
    this->m_shouldRecalculatePeaks = true;
    this->repaint();
 }

/*!
  \brief Mutator for the file-handling mode of a given instance of WaveformWidget.

An instance of WaveformWidget relies on an AudioUtil object to do much of the analysis of the audio file
that it visualizes.  This AudioUtil object can function in one of two modes: DISK_MODE or FULL_CACHE.
For a comprehensive outline of the benefits and drawbacks of each mode, see the documentation for
AudioUtil::setFileHandlingMode(FileHandlingMode mode).

@param mode The desired file-handling mode.  Valid options: WaveformWidget::FULL_CACHE, WaveformWidget::DISK_MODE
*/
void WaveformWidget::setFileHandlingMode(FileHandlingMode mode)
{
    this->m_currentFileHandlingMode = mode;

    switch (this->m_currentFileHandlingMode)
    {
        case FULL_CACHE:
            this->m_srcAudioFile->setFileHandlingMode(AudioUtil::FULL_CACHE);

        case DISK_MODE:
            this->m_srcAudioFile->setFileHandlingMode(AudioUtil::DISK_MODE);
    }
}

/*!
\brief Accessor for the file-handling mode of a given instance of WaveformWidget.

@return The file-handling mode of a given instance of WaveformWidget.
*/
WaveformWidget::FileHandlingMode WaveformWidget::getFileHandlingMode()
{
    return this->m_currentFileHandlingMode;
}

void WaveformWidget::recalculatePeaks()
{
    if (this->m_srcAudioFile->getSndFIleNotEmpty() && !this->m_isRecalculatingPeaks)
    {
        this->m_isRecalculatingPeaks = true;
        this->m_shouldRecalculatePeaks = false;
        /*calculate scale factor*/
        vector<double> normPeak = m_srcAudioFile->calculateNormalizedPeaks();
        double peak = MathUtil::getVMax(normPeak);
        this->m_scaleFactor = 1.0/peak;
        this->m_scaleFactor = m_scaleFactor - m_scaleFactor * this->m_padding;

        /*calculate frame-grab increments*/
        int totalFrames = m_srcAudioFile->getTotalFrames();
        int frameIncrement = totalFrames/this->width();
            if(m_srcAudioFile->getNumChannels() == 2)
            {
                this->m_peakVector.clear();

                vector<double> regionMax;

                /*
                  Populate the m_peakVector.at with peak values for each region of the source audio
                  file to be represented by a single pixel of the widget.
                */

                for(int i = 0; i < totalFrames; i += frameIncrement)
                {
                    regionMax = m_srcAudioFile->peakForRegion(i, i+frameIncrement);
                    if (regionMax.size() == 2)
                    {
                        double frameAbsL = fabs(regionMax[0]);
                        double frameAbsR = fabs(regionMax[1]);
                        this->m_peakVector.push_back(frameAbsL);
                        this->m_peakVector.push_back(frameAbsR);
                    }
                    else
                    {
                        double frameAbsL = fabs(0.0);
                        double frameAbsR = fabs(0.0);
                        this->m_peakVector.push_back(frameAbsL);
                        this->m_peakVector.push_back(frameAbsR);

                    }
                }
            }

            if(this->m_srcAudioFile->getNumChannels() == 1)
            {

                this->m_peakVector.clear();
                vector<double> regionMax;

                /*
                  Populate the m_peakVector.at with peak values for each region of the source audio
                  file to be represented by a single pixel of the widget.
                */

                for(int i = 0; i < totalFrames; i += frameIncrement)
                {
                    regionMax = m_srcAudioFile->peakForRegion(i, i+frameIncrement);
                    double frameAbs = fabs(regionMax[0]);

                    this->m_peakVector.push_back(frameAbs);
                }
            }
            this->m_isRecalculatingPeaks = false;
    }
}

/*
    The overview drawing function works with the m_peakVector.at, which contains the peak value
    for every region (and each channel) of the source audio file to be represented by a single
    pixel of the widget.  The function steps through this vector and draws two vertical bars
    for each such value -- one above the Y-axis midpoint for the channel, and one below.
*/
void WaveformWidget::overviewDraw()
{
    if (!this->m_srcAudioFile->getSndFIleNotEmpty())
        return;
    if (this->m_isRecalculatingPeaks)
        return;
    if ((qreal)value() / maximum() * width() == m_lastDrawnValue && !m_updateBreakPointRequired)
         return;
    if (this->m_shouldRecalculatePeaks)
    {
        QtConcurrent::run(this, &WaveformWidget::recalculatePeaks); //this->recalculatePeaks();
        return;
    }

    m_pixMap = QPixmap(this->m_lastSize);
    m_pixMap.scaled(m_lastSize);
    m_pixMap.fill(this->m_waveformBackgroundColor);
    QPainter painter(&m_pixMap);

    int minX = this->m_pixMap.rect().x();
    int maxX = this->m_pixMap.rect().x() + this->m_pixMap.rect().width();

    /*grab peak values for each region to be represented by a pixel in the visible
    portion of the widget, scale them, and draw: */
    if(this->m_srcAudioFile->getNumChannels() == 2)
    {

            int startIndex = 2*minX;
            int endIndex = 2*maxX;

            int yMidpoint = this->height()/2;
            int counter = minX;

            for(int i = startIndex;  i < endIndex; i+=2)
            {
                if (counter < (qreal)value() / maximum() * width())
                    painter.setPen(QPen(this->m_progressColor, 1, Qt::SolidLine, Qt::RoundCap));
                else
                    painter.setPen(QPen(this->m_waveformColor, 1, Qt::SolidLine, Qt::RoundCap));

                int chan1YMidpoint = yMidpoint - this->height()/4;
                int chan2YMidpoint = yMidpoint + this->height()/4;


                painter.drawLine(counter, chan1YMidpoint, counter, chan1YMidpoint+((this->height()/4)*this->m_peakVector.at(i)*m_scaleFactor));
                painter.drawLine(counter, chan1YMidpoint, counter, chan1YMidpoint -((this->height()/4)*this->m_peakVector.at(i)*m_scaleFactor));

                painter.drawLine(counter, chan2YMidpoint, counter, chan2YMidpoint+((this->height()/4)*this->m_peakVector.at(i+1)*m_scaleFactor)   );
                painter.drawLine(counter, chan2YMidpoint, counter, chan2YMidpoint -((this->height()/4)*this->m_peakVector.at(i+1)*m_scaleFactor)   );
                counter++;
            }

    }

    if(m_srcAudioFile->getNumChannels() == 1)
    {
           int curIndex = minX;
           int yMidpoint = this->height()/2;


           for(unsigned int i = 0; i < m_peakVector.size(); i++)
           {
               if (curIndex < (qreal)value() / maximum() * width())
                   painter.setPen(QPen(this->m_progressColor, 1, Qt::SolidLine, Qt::RoundCap));
               else
                   painter.setPen(QPen(this->m_waveformColor, 1, Qt::SolidLine, Qt::RoundCap));
               painter.drawLine(curIndex, yMidpoint, curIndex, yMidpoint+((this->height()/4)*this->m_peakVector.at(i)*m_scaleFactor)   );
               painter.drawLine(curIndex, yMidpoint, curIndex, yMidpoint -((this->height()/4)*this->m_peakVector.at(i)*m_scaleFactor)   );

               curIndex++;
           }
    }

    if (this->m_breakPointPos > 0 && this->m_hasBreakPoint)
    {
        painter.setPen(QPen(Qt::darkGray, 2, Qt::SolidLine, Qt::RoundCap));
        painter.drawLine(m_breakPointPos, 0, m_breakPointPos, 1000);
    }

    this->m_pixMapLabel->setPixmap(m_pixMap);
    this->m_pixMapLabel->resize(this->m_lastSize);
    m_lastDrawnValue = (qreal)value() / maximum() * width();
    this->m_lastSize = this->size();

}

/*!
    \brief Mutator for waveform color.

    @param The desired color for the waveform visualization.
*/
void WaveformWidget::setColor(QColor color)
{
    this->m_waveformColor = color;
}


void WaveformWidget::resizeEvent(QResizeEvent *e)
{
    QAbstractSlider::resizeEvent(e);
    if (!m_isClickHold)
        m_shouldRecalculatePeaks = true;
    m_lastSize = e->size();
}

