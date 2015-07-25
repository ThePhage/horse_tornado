// Horse tornado - simple LED persistence of vision

// For PixelPusher programming details, see
// https://docs.google.com/document/d/1D3tlMd0-H1p7Nmi4XtdEq_6MiXMoZ2Fhey-4_rigBz4

import javax.swing.*;
import com.heroicrobot.dropbit.registry.*;
import com.heroicrobot.dropbit.devices.pixelpusher.Pixel;
import com.heroicrobot.dropbit.devices.pixelpusher.Strip;
import processing.core.*;
import java.util.*;

// Essentials
final float pi = (float)Math.PI;

DeviceRegistry registry;
TestObserver observer;

// Background image.  Assumed horizontally periodic, and extended for sentinel purposes.
PImage back;
Map<Integer,PImage> extended = new HashMap<Integer,PImage>(); // Extended for sentinel purposes, resized horizontally

// Parameters and state
final float rotate_time = 2; // seconds
final float strip_angle = 2*pi/50; // horizontal box filter width of each strip
float time = 0;

void setup() {
  registry = new DeviceRegistry();
  observer = new TestObserver();
  registry.addObserver(observer);
  registry.setAntiLog(true);
  registry.setFrameLimit(1000);
  frameRate(1000);

  // Load the background image
  back = loadImage("withcharm.jpg");
  while (back.width > 1024 || back.height > 1024)
    back.resize(back.width/2,back.height/2);
  size(back.width,back.height);
}

// Extend an image to the right via wraparound to simplify drawStrip
PImage extend(final PImage in, final int height) {
  final PImage out = createImage(2*in.width,height,RGB);
  out.copy(in,0,0,in.width,in.height,
              0,0,in.width,height);
  out.copy(0,0,in.width,height,
           in.width,0,in.width,height);
  return out;
}

void drawStrip(final Strip strip, final float angle) {
  // Grab a height resized image
  final int ny = strip.getLength();
  final PImage im;
  if (extended.containsKey(ny))
    im = extended.get(ny);
  else {
    im = extend(back,ny);
    extended.put(ny,im);
  }

  // Determine horizontal section
  final float xlo = angle/(2*pi)%1*back.width,
              dx = strip_angle/(2*pi)*back.width,
              xhi = xlo + dx,
              inv_dx = 1/dx;
              
  // Draw each LED
  for (int y=0;y<ny;y++) {
    float r = 0, g = 0, b = 0;
    for (int x=(int)xlo;x<=(int)xhi;x++) {
      final int c = im.get(x,y);
      final float w = min(xhi,x+1)-max(x,xlo);
      r += w*red(c);
      g += w*green(c);
      b += w*blue(c);
    }
    strip.setPixel(color(inv_dx*r,inv_dx*g,inv_dx*b),y);
  }
}

void draw() {
  // Advance time
  time += 1./frameRate;

  // Draw on laptop screen
  image(back,0,0);
  
  // Draw on LEDs
  if (observer.hasStrips) {
    registry.startPushing();
    final List<Strip> strips = registry.getStrips();
    final int ns = strips.size();
    for (int s=0;s<ns;s++) {
      final float angle = 2*pi*time/rotate_time + 2*pi/ns*s;
      drawStrip(strips.get(s),angle);
    }
  }
}
