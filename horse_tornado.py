import os
import socket
import time
from PIL import Image

# packet header size
RGB_START = 5

# port for pixel value packets
UDP_PORT = 9897

# the length of each LED strip
STRIP_SIZE = 12

# the PixelPusher class represents the connection to a single PixelPusher unit
class PixelPusher( object ):
    
    # store PixelPusher info and create socket
    def __init__( self, stripNumber, ipAddress ):
        self.stripNumber = stripNumber
        self.ipAddress = ipAddress
        self.packetNumber = 0
        self.sock = socket.socket( socket.AF_INET, socket.SOCK_DGRAM ) # prepare a UDP socket
        self.bytesSent = 0
        
    # send a packet; overwrites header portion of buffer
    def push( self, buffer ):
        buffer[ 0 ] = (self.packetNumber >> 24) & 0xFF
        buffer[ 1 ] = (self.packetNumber >> 16) & 0xFF
        buffer[ 2 ] = (self.packetNumber >> 8) & 0xFF
        buffer[ 3 ] = self.packetNumber & 0xFF
        buffer[ 4 ] = self.stripNumber
        self.bytesSent += self.sock.sendto( buffer, (self.ipAddress, UDP_PORT) )
        self.packetNumber += 1

# the ImageBuf class stores a set of pre-computed buffers for a single image
class ImageBuf( object ):

    # load the given image and construct buffers from it
    def __init__( self, fileName ):
    
        # load image
        image = Image.open( fileName )
        maxImageSize = (STRIP_SIZE, STRIP_SIZE * 10)
        image.thumbnail( maxImageSize, Image.ANTIALIAS )
        (width, height) = image.size
        print 'loaded %s and resized to %d x %d' % (fileName, width, height)
    
        # allocate buffer for each column of pixels
        self.bufferIndex = 0
        self.buffers = []
        for i in xrange( width ):
            self.buffers.append( bytearray( 'X' * (RGB_START + STRIP_SIZE * 3) ) )
        
        # get a pixel buffer for fast access
        imagePixels = image.load()
        
        # copy into buffers
        for x in xrange( width ):
            buffer = self.buffers[ x ]
            for y in xrange( height ):
                index = RGB_START + y * 3
                buffer[ index:(index+3) ] = imagePixels[ x, y ]
        
    # a utility function for testing
    def setRGB( self, r, g, b ):
        for buffer in self.buffers:
            for i in xrange( STRIP_SIZE ):
                buffer[ RGB_START + i * 3 ] = r
                buffer[ RGB_START + i * 3 + 1 ] = g
                buffer[ RGB_START + i * 3 + 2 ] = b
    
    # retrieve the buffer corresponding to the next column of pixels
    def nextBuffer( self ):
        buffer = self.buffers[ self.bufferIndex ]
        self.bufferIndex += 1
        if self.bufferIndex >= len( self.buffers ):
            self.bufferIndex = 0
        return buffer

# load pixel pushers using IP addresses in the given file
def loadPixelPushers( ipFileName ):
    pixelPushers = []
    addresses = [addr.strip() for addr in open( ipFileName ).readlines()]
    for address in addresses:
        if address:
            pixelPushers.append( PixelPusher( 0, address ) )
    return pixelPushers

# load images from the given path
def loadImages( path ):
    images = []
    fileNames = os.listdir( path )
    for fileName in fileNames:
        if fileName.endswith( '.png' ) or fileName.endswith( '.jpg' ):
            images.append( ImageBuf( path + '/' + fileName ) )
    return images

def main():

    # create pixel pusher objects and images
    # no more allocation should occur after this
    pixelPushers = loadPixelPushers( 'ip.txt' )
    images = loadImages( '.' )
    print 'loaded %d pixel pushers' % len( pixelPushers )
    print 'loaded %d images' % len( images )
    
    # display images forever
    while True:
        for (index, pixelPusher) in enumerate( pixelPushers ):
            image = images[ index % len( images ) ]
            pixelPusher.push( image.nextBuffer() )
        time.sleep( 0.5 )

if __name__ == '__main__':
    main()
