import java.awt.Color;

int wheelColor(final double t) {
  return Color.HSBtoRGB((float)t,1.,1.);
}

// Draw a rotating wheel color image
PImage wheelImage(final double angle) {
  final double t0 = angle/2*pi;
  final PImage I = createImage(width,height,RGB);
  for (int y=0;y<I.height;y++) {
    final double ty = t0+(double)y/(2*height);
    for (int x=0;x<I.width;x++)
      I.pixels[y*I.width+x] = wheelColor(ty+(double)x/width);
  }
  I.updatePixels();
  return I;
}
