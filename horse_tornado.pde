// Horse tornado - simple LED persistence of vision

// For PixelPusher programming details, see
// https://docs.google.com/document/d/1D3tlMd0-H1p7Nmi4XtdEq_6MiXMoZ2Fhey-4_rigBz4

import javax.swing.*;
import com.heroicrobot.dropbit.registry.*;
import com.heroicrobot.dropbit.devices.pixelpusher.Pixel;
import com.heroicrobot.dropbit.devices.pixelpusher.Strip;
import processing.video.*;
import processing.core.*;
import java.util.*;

// Essentials
final float pi = (float)PI;

DeviceRegistry registry;
TestObserver observer;

// Parameters and state
final float rotate_time = 1./8; // seconds
final float strip_angle = 2*pi/200; // horizontal box filter width of each strip
final float image_time = 60; // seconds
final boolean timing = false;
float time = 0;

// Background image.  Assumed horizontally periodic, and extended for sentinel purposes.
final Queue<String> back_paths = new ArrayDeque(); // All available background images
PImage back; // Might be a movie

float back_end_time = 0;
Map<Integer,PImage> extended = new HashMap<Integer,PImage>(); // Extended for sentinel purposes, resized horizontally

// Load a background image or movie
void load(final String path) {
  println("Loading: "+path);
  if (path.endsWith(".jpg") || path.endsWith(".png")) {
    println("Loading image: "+path);
    back = loadImage(path);
    while (back.width > 1024 || back.height > 1024)
      back.resize(back.width/2,back.height/2);
    back_end_time = time + image_time;
  } else {
    println("Loading video: "+path);
    final Movie m = new Movie(this,path);
    println("  duration = "+m.duration());
    m.loop();
    back = m;
    back_end_time = time + m.duration();
  }
  size(200,100);
  image(back,0,0,width,height);
  extended.clear();
}

void load_next() {
  final String path = back_paths.poll();
  load(path);
  back_paths.add(path);
}

void setup() {
  registry = new DeviceRegistry();
  observer = new TestObserver();
  registry.addObserver(observer);
  registry.setAntiLog(true);
  registry.setFrameLimit(1000);
  frameRate(1000);

  // Find all available sources
  List<String> paths = new ArrayList<String>();
  for (final File file : new File(dataPath("")).listFiles())
    paths.add(file.getPath());
  Collections.sort(paths);
  for (final String path : paths)
    println("Scanning source: "+path);
  back_paths.addAll(paths);

  // Load the first source
  load_next();
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

void drawStrip(final int s, final Strip strip, final float angle) {
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
  if (false && s == 0)
    println("fraction = "+xlo/back.width);

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
    strip.setPixel(color(inv_dx*r,inv_dx*g,inv_dx*b),ny-y-1);
  }
}

void draw() {
  final double start = timing ? System.nanoTime() : 0;

  // Advance time
  time += 1./frameRate;
  if (false && time > back_end_time)
    load_next();

  // Draw on laptop screen
  //image(back,0,0,width,height);

  // Draw on LEDs
  double pushed = 0;
  if (observer.hasStrips) {
    registry.startPushing();
    final List<Strip> strips = registry.getStrips();
    final int ns = strips.size();
    for (int s=0;s<ns;s++) {
      final Strip st = strips.get(s);
      final double p = timing ? st.getPushedAt() : 0;
      if (p != 0) pushed = p;
      final float angle = 2*pi*time/rotate_time + 2*pi/ns*s;
      drawStrip(s,st,angle);
    }
  }

  if (timing && pushed != 0) {
    final double end = System.nanoTime();
    println("end-start = "+(end-start)+", end - pushed = "+(end-pushed));
    //println("  start "+start+", end "+end+", pushed "+pushed);
  }
}

void movieEvent(Movie m) {
  // Advance to the next movie frame
  m.read();
  extended.clear();
}
