Horse Tornado
=============

### Usage

The current version is pure python+numpy.  If you need numpy, run one of

    sudo pip install numpy
    sudo apt-get install python-numpy

The `horse_tornado` script is used as follows:

    ./horse_tornado config data/red-circle.gif

where `config` is an ini style configuration file (an example is checked in)
and the image is displayed via persistence of vision.

### Configuring the PixelPushers

Included in this repository are the files `pixel[1-4].rc`, which are the 
configuration files corresponding to pixelpushers 1 - 4. They should only
differ by the `start[1-2]` settings which set the startup colors for the
strips.

To load a config file on pixelpusher, use the configtool that is checked
out under `PixelPusher-utilities/configtool/configtool`. If the
PixelPusher-utilites folder is empty run:

```bash
git submodule update --init --recursive
```

Otherwise from the configtool directory run (on mac):

```bash
./configtool /dev/tty.usbmodem12341 ../../pixel1.rc
```

To configure the 1st pixelpusher. The number after usbmodem will change
from computer to computer.

### PixelPusher and Processing documentation:

Somewhat obsolete, since we're no longer using Processing.

1. [Processing tutorial](https://processing.org/tutorials/overview)
2. [PixelPusher POV example code](https://github.com/robot-head/PixelPusher-processing-sketches/tree/master/pixelpusher_lightpainting)
