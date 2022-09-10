#include "WaveformWidget.h"

#include <QStandardPaths>
#include <QDir>
#include <QToolTip>
#include <QDebug>

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
    is_clickable(false)
{
    clearFocus();
    setFocusPolicy(Qt::NoFocus);

    this->srcAudioFile = new AudioUtil();
    this->ffmpegConvertToMono = true;
    this->convert_process = new QProcess;
    this->first_draw = true;
    this->pixMapLabel = new QLabel(this);
    this->pixMapLabel->show();
    this->shouldRecalculatePeaks = true;
    connect(convert_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &WaveformWidget::setSourceFromConverted);

    this->paintTimer = new QTimer(this);
    connect(this->paintTimer, &QTimer::timeout, this, &WaveformWidget::overviewDraw);
    paintTimer->setInterval(100);
    paintTimer->start();
}

/*The AudioUtil instance "srcAudioFile" is our only dynamically allocated object*/
WaveformWidget::~WaveformWidget()
{
    delete this->srcAudioFile;
}


void WaveformWidget::setSource(QFileInfo *fileName)
{
    this->audioFilePath = fileName->canonicalFilePath();
    if ((fileName->completeSuffix() != "wav" && !this->ffmpeg_path.isEmpty()) || (this->ffmpegConvertToMono && !this->ffmpeg_path.isEmpty()))
    {
        this->convertAudio(fileName);
    }
    else
    {
        this->currentFileHandlingMode = FULL_CACHE;
        this->resetFile(fileName);
        this->scaleFactor = -1.0;
        this->lastSize = this->size();
        this->padding = DEFAULT_PADDING;
    }
}

void WaveformWidget::setSourceFromConverted()
{
    if (!this->audioFilePath.isEmpty())
    {
        QFileInfo *audioFileInfo = new QFileInfo(audioFilePath);
        QString newFileName = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QDir::separator() + audioFileInfo->completeBaseName() + QString(".wav");
        QFileInfo *fileInfo = new QFileInfo(newFileName);
        this->currentFileHandlingMode = FULL_CACHE;
        this->resetFile(fileInfo);
        this->scaleFactor = -1.0;
        this->lastSize = this->size();
        this->padding = DEFAULT_PADDING;
    }
}

void WaveformWidget::mousePressEvent(QMouseEvent *event)
{
  if ((event->button() == Qt::LeftButton) && is_clickable)
    emit barClicked(mouseEventPosition(event));

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

  is_clickable = clickable;
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
    this->audioFilePath = fileName->canonicalFilePath();
    this->srcAudioFile->setFile(audioFilePath);

    switch(this->currentFileHandlingMode)
    {
        case FULL_CACHE:
            this->srcAudioFile->setFileHandlingMode(AudioUtil::FULL_CACHE);

        case DISK_MODE:
            this->srcAudioFile->setFileHandlingMode(AudioUtil::DISK_MODE);
    }

    this->peakVector.clear();
    this->dataVector.clear();
    this->shouldRecalculatePeaks = true;
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
    this->currentFileHandlingMode = mode;

    switch (this->currentFileHandlingMode)
    {
        case FULL_CACHE:
            this->srcAudioFile->setFileHandlingMode(AudioUtil::FULL_CACHE);

        case DISK_MODE:
            this->srcAudioFile->setFileHandlingMode(AudioUtil::DISK_MODE);
    }
}

/*!
\brief Accessor for the file-handling mode of a given instance of WaveformWidget.

@return The file-handling mode of a given instance of WaveformWidget.
*/
WaveformWidget::FileHandlingMode WaveformWidget::getFileHandlingMode()
{
    return this->currentFileHandlingMode;
}

void WaveformWidget::recalculatePeaks()
{
    /*calculate scale factor*/
    vector<double> normPeak = srcAudioFile->calculateNormalizedPeaks();
    double peak = MathUtil::getVMax(normPeak);
    this->scaleFactor = 1.0/peak;
    this->scaleFactor = scaleFactor - scaleFactor * this->padding;

    /*calculate frame-grab increments*/
    int totalFrames = srcAudioFile->getTotalFrames();
    int frameIncrement = totalFrames/this->width();
        if(srcAudioFile->getNumChannels() == 2)
        {
            this->peakVector.clear();

            vector<double> regionMax;

            /*
              Populate the peakVector with peak values for each region of the source audio
              file to be represented by a single pixel of the widget.
            */

            for(int i = 0; i < totalFrames; i += frameIncrement)
            {
                regionMax = srcAudioFile->peakForRegion(i, i+frameIncrement);
                double frameAbsL = fabs(regionMax[0]);
                double frameAbsR = fabs(regionMax[1]);

                this->peakVector.push_back(frameAbsL);
                this->peakVector.push_back(frameAbsR);
            }
        }

        if(this->srcAudioFile->getNumChannels() == 1)
        {

            this->peakVector.clear();
            vector<double> regionMax;

            /*
              Populate the peakVector with peak values for each region of the source audio
              file to be represented by a single pixel of the widget.
            */

            for(int i = 0; i < totalFrames; i += frameIncrement)
            {
                regionMax = srcAudioFile->peakForRegion(i, i+frameIncrement);
                double frameAbs = fabs(regionMax[0]);

                this->peakVector.push_back(frameAbs);
            }
        }
        this->shouldRecalculatePeaks = false;
}

/*
    The overview drawing function works with the peakVector, which contains the peak value
    for every region (and each channel) of the source audio file to be represented by a single
    pixel of the widget.  The function steps through this vector and draws two vertical bars
    for each such value -- one above the Y-axis midpoint for the channel, and one below.
*/
void WaveformWidget::overviewDraw()
{
    if (!this->srcAudioFile->getSndFIleNotEmpty())
        return;
     if ((qreal)value() / maximum() * width() == lastDrawnValue)
         return;
     if (this->shouldRecalculatePeaks)
         this->recalculatePeaks();

    pixMap = QPixmap(size());
    pixMap.fill(Qt::transparent);
    QPainter painter(&pixMap);

    int minX = this->pixMap.rect().x(); //lastPaintEvent->region().boundingRect().x();
    int maxX = this->pixMap.rect().x() + this->pixMap.rect().width();

    /*grab peak values for each region to be represented by a pixel in the visible
    portion of the widget, scale them, and draw: */
    if(this->srcAudioFile->getNumChannels() == 2)
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


                painter.drawLine(counter, chan1YMidpoint, counter, chan1YMidpoint+((this->height()/4)*this->peakVector.at(i)*scaleFactor));
                painter.drawLine(counter, chan1YMidpoint, counter, chan1YMidpoint -((this->height()/4)*this->peakVector.at(i)*scaleFactor));

                painter.drawLine(counter, chan2YMidpoint, counter, chan2YMidpoint+((this->height()/4)*this->peakVector.at(i+1)*scaleFactor)   );
                painter.drawLine(counter, chan2YMidpoint, counter, chan2YMidpoint -((this->height()/4)*this->peakVector.at(i+1)*scaleFactor)   );

                counter++;
            }

    }

    if(srcAudioFile->getNumChannels() == 1)
    {
           int curIndex = minX;
           int yMidpoint = this->height()/2;


           for(unsigned int i = 0; i < peakVector.size(); i++)
           {
               if (curIndex < (qreal)value() / maximum() * width())
                   painter.setPen(QPen(this->m_progressColor, 1, Qt::SolidLine, Qt::RoundCap));
               else
                   painter.setPen(QPen(this->m_waveformColor, 1, Qt::SolidLine, Qt::RoundCap));
               painter.drawLine(curIndex, yMidpoint, curIndex, yMidpoint+((this->height()/4)*this->peakVector.at(i)*scaleFactor)   );
               painter.drawLine(curIndex, yMidpoint, curIndex, yMidpoint -((this->height()/4)*this->peakVector.at(i)*scaleFactor)   );

               curIndex++;
           }
    }

    this->pixMapLabel->setPixmap(pixMap);
    this->pixMapLabel->resize(size());
    lastDrawnValue = (qreal)value() / maximum() * width();
    this->lastSize = this->size();
}

/*!
    \brief Mutator for waveform color.

    @param The desired color for the waveform visualization.
*/
void WaveformWidget::setColor(QColor color)
{
    this->m_waveformColor = color;
}

void WaveformWidget::setFfmpegConvertToMono(bool convert)
{
    this->ffmpegConvertToMono = convert;
}

/**
 * @brief sets the path to ffmpeg binary to use for processing non wav files
 * @param path to ffmpeg binary
 */
void WaveformWidget::setFfmpegPath(QString path)
{
    this->ffmpeg_path = path;
}

void WaveformWidget::convertAudio(QFileInfo *fileName)
{
    QString newFileName = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QDir::separator() + fileName->completeBaseName() + QString(".wav");
    QStringList params;
    if (this->ffmpegConvertToMono)
        params << "-y"
               << "-i"
               << fileName->canonicalFilePath()
               << "-af"
               << "pan=mono|c0=.5*c0+.5*c1"
               << "-acodec"
               << "pcm_u8"
               << newFileName;
    else
        params << "-y"
               << "-i"
               << fileName->canonicalFilePath()
               << "-acodec"
               << "pcm_u8"
               << newFileName;

    convert_process->start(this->ffmpeg_path, params);
}


void WaveformWidget::resizeEvent(QResizeEvent *)
{

}

