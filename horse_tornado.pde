// Horse tornado - simple LED persistence of vision

import com.heroicrobot.dropbit.registry.*;
import com.heroicrobot.dropbit.devices.pixelpusher.Pixel;
import com.heroicrobot.dropbit.devices.pixelpusher.Strip;
import java.util.*;
import java.awt.Color;

// Parameters and state
final double rotate_time = 2; // seconds
double time = 0;

// For PixelPusher programming details, see
// https://docs.google.com/document/d/1D3tlMd0-H1p7Nmi4XtdEq_6MiXMoZ2Fhey-4_rigBz4

// Monitor when LEDs appear and disappear from the world
class TestObserver implements Observer {
  public boolean hasStrips = false;
  public void update(Observable registry, Object updatedDevice) {
    hasStrips = true;
  }
};
TestObserver observer;

int wheelColor(final double t) {
  return Color.HSBtoRGB((float)t,1.,1.);
}

// Draw a rotating wheel color image
PImage wheel_image(final double time) {
  final PImage I = createImage(width,height,RGB);
  for (int y=0;y<I.height;y++) {
    final double ty = time/rotate_time+(double)y/(2*height);
    for (int x=0;x<I.width;x++)
      I.pixels[y*I.width+x] = wheelColor(ty+(double)x/width);
  }
  I.updatePixels();
  return I;
}

void setup() {
  frameRate(24);
  size(960,256);
}

void draw() {
  image(wheel_image(time),0,0);
  time += 1./frameRate;
}
