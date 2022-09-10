libwaveformwidget
-----------------

This library provides a widget for the Qt framework capable of drawing the waveform of an audio file.  

DEPENDENCIES:
	this library depends on  
	(1) The Qt framework (tested with v. 4.6, but likely to work with earlier versions)
        (2) libsndfile (master branch with mp3 support (mpg123) preferred) (available from https://github.com/libsndfile/libsndfile)

Full API documentation can be found at http://www.columbia.edu/~naz2106/WaveformWidget

The header and implementation files for this widget (class WaveformWidget) can be found in the src directory, as well as header and implementation files for a pair of classes leveraged by it.


BUILDING FROM SOURCE:

To build the widget as a dynamic library, simply invoke "qmake" and then "make" from within the "src" directory.

I have provided a shell script, "install.sh", that, when invoked with appropriate privileges, will copy the object files to your "/usr/lib" directory, and the header files to your "/usr/include" directory.  I have also provided an "uninstall.sh" script that will remove the files from these directories (useful if you modify the source files).


Usage example:


#include <WaveformWidget.h>


WaveformWidget *widget_waveform = new WaveformWidget(this);

// widget_waveform->setSource("/path/to/file.mp3"); // needs libsndfile with mp3 support.
widget_waveform->setSource("/path/to/file.wav");
widget_waveform->setClickable(true); // WaveformWidget it's a slider. You can disable touch events and such
widget_waveform->setValue(0.5); // You can set a value to match your playback time
