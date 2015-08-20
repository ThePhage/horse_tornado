Horse Tornado
=============

### Usage

The current version is pure python+numpy.  If you need numpy, run one of

    sudo pip install numpy
    sudo apt-get install python-numpy

The `horse_tornado` script is used as follows:

    ./horse_tornado config data                # Cycle through a directory
    ./horse_tornado config data/red-circle.gif # Use a specific image

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

### Which strip is which?

The above configuration assumes you know which PixelPusher is which in
terms of their ordering around the circle.  If you do not know this,
set the following two lines in the `[all]` section of `config`:

    mode = rainbow
    animate = 0

This will display a color ramp (upwards from red to green to blue) on
part of each strip.  If the ordering is correct, the ramp will move
upwards as you rotate left to right around the circle.  If this isn't
the case, adjust the config file accordingly.

### Aspect ratio

Conceptually, POV wraps an image around an imaginary cylinder, spins it
to match the spin of the viewer, and treats the LED strips as windows
onto the cylinder.  The wrapping is aspect-ratio dependent, and the code
can optionally account for this.  To enable aspect ratio support, put
the following in the `[all]` section of `config`:

    aspect = 1
    height = <strip-height>
    radius = <radius-out-to-the-strips>

Only the ratio height/radius matters, so units are arbitrary.

### PixelPusher and Processing documentation:

Somewhat obsolete, since we're no longer using Processing.

1. [Processing tutorial](https://processing.org/tutorials/overview)
2. [PixelPusher POV example code](https://github.com/robot-head/PixelPusher-processing-sketches/tree/master/pixelpusher_lightpainting)
