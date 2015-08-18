// Monitor when LEDs appear and disappear from the world

// For PixelPusher programming details, see
// https://docs.google.com/document/d/1D3tlMd0-H1p7Nmi4XtdEq_6MiXMoZ2Fhey-4_rigBz4

class TestObserver implements Observer {
  public boolean hasStrips = false;
  public void update(Observable registry, Object updatedDevice) {
    hasStrips = true;
  }
};
