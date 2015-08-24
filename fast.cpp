// Fast version of horse-tornado

// Warning: This code uses boost because it had all the libraries
// and we are under severe time pressue.  This is problem, because
// boost's image library in particular is a horrendous pile of
// template hell with terrifyingly bad documentation.

#include <atomic>
#include <cassert>
#include <iostream>
#include <unordered_set>

#include <sys/socket.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>

#include <png.h>

#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/array.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/multi_array.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/random/mersenne_twister.hpp>
#include <boost/random/uniform_real.hpp>
#include <boost/range/irange.hpp>
#include <boost/scoped_array.hpp>

using std::min;
using std::max;
using std::cout;
using std::cerr;
using std::endl;
using std::atomic;
using std::string;
using std::vector;
using std::function;
using std::unique_ptr;
using std::unordered_set;

using namespace boost::algorithm;
using boost::array;
using boost::format;
using boost::extents;
using boost::irange;
using boost::mt19937;
using boost::multi_array;
using boost::scoped_array;
using boost::uniform_real;

namespace {

typedef mt19937 Random;
const double pi = M_PI;

template<class T> inline T& const_cast_(const T& x) {
  return const_cast<T&>(x);
}

template<class I> auto range(const I n)
  -> decltype(irange(I(),n)) {
  return irange(I(),n);
}

template<class I> I positive_mod(I n, const I d) {
  assert(d > 0);
  n %= d;
  return n >= 0 ? n : n + d;
}

__attribute__((noreturn)) void die(const string s) {
  cerr << s << endl;
  exit(1);
}

double get_time() {
  timeval tv;
  gettimeofday(&tv,0);
  return tv.tv_sec+1e-6*tv.tv_usec;
}

void good_sleep(const double t) {
  if (t <= 0)
    return;
  const uint64_t b = 1000000000;
  const uint64_t n(b*t);
  timespec ts;
  ts.tv_sec = n/b;
  ts.tv_nsec = n%b;
  nanosleep(&ts,0);
}

struct Spinlock {
  void lock()   { while (f.test_and_set(std::memory_order_acquire)) {} }
  void unlock() { f.clear(std::memory_order_release); }
private:
  std::atomic_flag f = ATOMIC_FLAG_INIT;
};

// Run a function in a separate thread
void async(const function<void()>& f) {
  const auto p = new function<void()>(f);
  pthread_t thread;
  const int r = pthread_create(&thread,0,[](void* arg) {
    const auto p = (function<void()>*)arg;
    (*p)();
    delete p;
    return (void*)0;
  },p);
  if (r)
    die(string("Thread creation failed: ")+strerror(r));
  pthread_detach(thread);
}

template<class... Args> string join(const string& sep, const Args&... args) {
  const vector<string> xs = {string(args)...};
  return boost::join(xs,sep);
}

template<class... Args> string command_output(const string& path, const Args&... args) {
  int fd[2];
  if (pipe(fd) < 0)
    die("Pipe failed");
  const pid_t pid = fork();
  if (pid) { // Parent
    close(fd[1]);
    string results;
    char buffer[1024+1];
    for (;;) {
      ssize_t n = read(fd[0],buffer,sizeof(buffer)-1);
      if (n == 0)
        break;
      else if (n < 0)
        die(string("command_output: read failed: ")+strerror(errno));
      buffer[n] = 0;
      results += buffer;
    }
    close(fd[0]);
    int status;
    waitpid(pid,&status,0);
    if (!WIFEXITED(status) || WEXITSTATUS(status))
      exit(1);
    return results;
  } else { // Child
    if (dup2(fd[1],1) < 0)
      die("dup2 failed");
    execlp(path.c_str(),string(args).c_str()...,NULL);
    die("Command failed ("+path+"): "+join(" ",string(args)...)+", "+strerror(errno));
  }
}

template<class... Args> void run(const string& path, const Args&... args) {
  if (0)
    cout<<"command = "<<join(" ",string(args)...)<<endl;
  const pid_t pid = fork();
  if (pid) { // Parent
    int status;
    waitpid(pid,&status,0);
    if (!WIFEXITED(status) || WEXITSTATUS(status))
      exit(1);
  } else { // Child
    if (execlp(path.c_str(),string(args).c_str()...,NULL) < 0)
      die("Command failed ("+path+"): "+join(" ",string(args)...)+", "+strerror(errno));
  }
}

typedef array<uint8_t,3> Pixel;
typedef multi_array<Pixel,2> Image;

Image read_png(const string& path) {
  FILE* file = fopen(path.c_str(),"rb");
  if (!file)
    die("Failed to open "+path+" for reading");
  png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING,0,0,0);
  if (!png)
    die("Error reading png file "+path);
  png_infop info = png_create_info_struct(png);
  if (!info || setjmp(png_jmpbuf(png)))
    die("Error reading png file "+path);
  png_init_io(png,file);
  png_read_png(png,info,PNG_TRANSFORM_STRIP_16|PNG_TRANSFORM_STRIP_ALPHA
                       |PNG_TRANSFORM_PACKING|PNG_TRANSFORM_EXPAND,0);
  const int w = png_get_image_width(png,info),
            h = png_get_image_height(png,info);
  Image image(extents[w][h]);
  Pixel** rows = (Pixel**)png_get_rows(png,info);
  for (const int x : range(w))
    for (const int y : range(h))
      image[x][y] = rows[h-y-1][x];
  png_destroy_read_struct(&png,&info,0);
  fclose(file);
  return image;
}

struct Options {
  // Property tree.  Warning: Terrible hack to use static here.
  static boost::property_tree::ptree props;
  #define PROP(name,default) \
    const decltype(default) name = props.get("all." #name,default);

  // Global options
  PROP(port,9897)       // Port for pixel value packets
  PROP(size,200)        // Default number of LEDs per strip
  PROP(frequency,16.)   // Image repeats every 1/frequency
  PROP(frame_rate,1000) // Frame rate at which to update each strip
  PROP(reload_time,60.) // How often to change images (in seconds)
  PROP(mode,string("image"))
  PROP(animate,true)
  PROP(correct,true)
  PROP(scale,1.)

  // Aspect ratio control
  PROP(aspect,false)
  PROP(height,2.5)      // Strip height (same units as radius)
  PROP(radius,2.5)      // Radius from center to strips (same units as height)

  // Mouse control
  PROP(use_mouse,false)                       // If true, angle is controlled by mouse
  PROP(mouse_dev,string("/dev/input/mouse0")) // Mouse device to read angle from
  PROP(mouse_frequency,24.)                   // Frequency of mouse polling

  #undef PROP

  // Strips
  struct Strip {
    string name;
    int port;
    int number;
    int size;
    string ip;
    sockaddr_in addr;
  };
  const vector<Strip> strips;
  const int total_strips;

  Options()
    : strips()
    , total_strips() {
    auto& strips = const_cast_(this->strips);
    for (const auto& p : props)
      if (boost::algorithm::contains(p.first,"strip")) {
        // Parse strip details
        Strip s;
        s.name = p.second.get("name",p.first);
        s.port = p.second.get("port",port);
        s.number = p.second.get("number",0);
        s.size = p.second.get("size",size);
        s.ip = p.second.get<string>("ip");

        // Look up address
        s.addr.sin_family = AF_INET;
        const hostent* host = gethostbyname(s.ip.c_str());
        if (!host)
          die(string("Invalid strip '")+s.name+"' address '"+s.ip+"': "+hstrerror(h_errno));
        bcopy(host->h_addr,&s.addr.sin_addr.s_addr,host->h_length);
        s.addr.sin_port = htons(s.port);

        // Strip done
        strips.push_back(s);
      }
    sort(strips.begin(),strips.end(),[](const Strip& s0, const Strip& s1) { return s0.name < s1.name; });
    for (const auto& s : strips)
      cout<<"  "<<s.name<<" : ip "<<s.ip<<", number "<<s.number<<", size "<<s.size<<endl;
    const_cast_(total_strips) = props.get("all.strips",int(strips.size()));
  }
};

boost::property_tree::ptree Options::props;

// Color correction taken from PixelPusher code
const uint8_t correction[257] = {
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
  2,3,3,3,3,3,3,3,3,3,3,3,3,4,4,4,4,4,4,4,4,4,5,5,5,5,5,5,5,5,6,6,6,6,6,6,7,7,7,7,7,7,8,8,8,8,8,9,9,9,9,9,10,10,10,
  10,11,11,11,11,12,12,12,13,13,13,14,14,14,14,15,15,16,16,16,17,17,17,18,18,19,19,20,20,20,21,21,22,22,23,23,24,25,
  25,26,26,27,27,28,29,29,30,31,31,32,33,34,34,35,36,37,38,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,54,55,56,57,
  59,60,61,63,64,65,67,68,70,72,73,75,76,78,80,82,83,85,87,89,91,93,95,97,99,102,104,106,109,111,114,116,119,121,124,
  127,129,132,135,138,141,144,148,151,154,158,161,165,168,172,176,180,184,188,192,196,201,205,209,214,219,224,229,
  234,239,244,249,255};

// Black jump to red to green to blue jump to black
Image rainbow(const int w, const int h) {
  Image i(extents[w][h]);
  for (const int x : range(w))
    for (const int y : range(h)) {
      const auto t = 2.*y/h-double(x)/w;
      auto& p = i[x][y];
      if (t<0 || t>1)
        p = Pixel({{0,0,0}});
      else if (t<1./2) {
        const auto s = 2*t;
        p = Pixel({{uint8_t(255*(1-s)),uint8_t(255*s),0}});
      } else {
        const auto s = 2*t-1;
        p = Pixel({{0,uint8_t(255*(1-s)),uint8_t(255*s)}});
      }
    }
  return i;
}

// Solid color image
Image solid(const int w, const int h, const uint8_t r, const uint8_t g, const uint8_t b) {
  Image i(extents[w][w]);
  for (const int x : range(w))
    for (const int y : range(h))
      i[x][y] = Pixel({{r,g,b}});
  return i;
}

// Red except for one white pixel in each column
Image single(const int w) {
  auto i = solid(w,w,255,0,0);
  for (const int x : range(w))
    i[x][x] = Pixel({{255,255,255}});
  return i;
}

// Precomputed information to display one image via persistence of vision
struct POV {
  // Packet header size
  static const int header_size = 5;

  // Number of pixels around the cylinder
  const double around;

  // Packet buffers
  const int width, height, packet_size;
  scoped_array<uint8_t> buffer;

  // UDP socket
  const int sock;

  // Packet count
  uint32_t packet = 0;

  POV(const Options& o, const int size, const string& path)
    : around(), width(), height(), packet_size(), sock() { // Correctly initialized below

    // Use identify to compute aspect ratio
    const double aspect = !o.aspect ? 1 : [&]() {
      const auto dims = command_output("identify","identify","-format","%w %h",path);
      int w,h;
      if (sscanf(dims.c_str(),"%d %d",&w,&h) < 0)
        die("Can't parse dimensions: "+dims);
      return double(w)/h;
    }();

    // Pick resize dimensions
    int w = int(ceil( // Round up so that w > 0 (just in case)
      o.aspect ? aspect*o.height*o.frame_rate/(2*pi*o.radius*o.frequency) // Correct aspect
               : o.frame_rate/o.frequency)); // wrap once around the cylinder
    const int h = size;

    // Resize or rewrite image
    Image fit = [&]() {
      if (o.mode == "image") {
        char tmp[] = "/tmp/horse-tornado-fit-XXXXXX.png";
        if (mkstemps(tmp,4) < 0)
          die(string("Could not create temporary file ")+tmp+": "+strerror(errno));
        run("convert","convert","-resize",str(format("%dx%d!")%w%h),"-depth","8","-background","black",path,tmp);
        const auto image = read_png(tmp);
        boost::filesystem::remove(tmp);
        return image;
      } else {
        if      (o.mode == "black") return solid(w,h,0,0,0);
        else if (o.mode == "white") return solid(w,h,255,255,255);
        else if (o.mode == "rainbow") return rainbow(w,h);
        else if (o.mode == "single")  return single(h);
        else die("Got mode '"+o.mode+"', expected image, rainbow, black, or white");
      }
    }();
    w = fit.size();
    assert(fit.shape()[1]==size_t(h));

    // Optionally color correct
    if (o.correct)
      for (const int x : range(w))
        for (const int y : range(h)) {
          auto& p = fit[x][y];
          for (const auto i : range(3))
            p[i] = correction[p[i]];
        }

    // Rescale
    if (o.scale != 1)
      for (const int x : range(w))
        for (const int y : range(h)) {
          auto& p = fit[x][y];
          for (const auto i : range(3))
            p[i] = max(0,min(255,int(rint(o.scale*p[i]))));
        }

    // Number of pixels around the cylinder
    const_cast_(around) = o.aspect ? int(ceil(o.frame_rate/o.frequency)) : w;

    // Reshape into nearly-ready-to-send packets
    const_cast_(width) = w;
    const_cast_(height) = h;
    const_cast_(packet_size) = header_size+3*h;
    buffer.reset(new uint8_t[width*packet_size]);
    for (const int x : range(w))
      for (const int y : range(h)) {
        const auto p = fit[x][y];
        for (const int i : range(3))
          buffer[x*packet_size+header_size+3*y+i] = p[i];
      }

    // Prepare to send UDP packets
    const_cast_(sock) = socket(AF_INET,SOCK_DGRAM,0);
    if (sock < 0)
      die(string("Couldn't open UDP socket: ")+strerror(errno));
  }

  ~POV() {
    close(sock);
  }

  void send(const Options::Strip& s, const int64_t n) {
    uint8_t* buf = buffer.get()+packet_size*positive_mod(n,int64_t(width));
    uint32_t p = htonl(packet++);
    memcpy(buf,&p,4);
    buf[4] = s.number;
    sendto(sock,buf,packet_size,0,(sockaddr*)&s.addr,sizeof(s.addr));
  }
};

// Choose one entry at random from a sequence
template<class T> struct Choice {
  Random& random;
  uint64_t count = 0;
  T value; // Initially invalid

  Choice(Random& random)
    : random(random) {}

  void add(const T& x) {
    static const uniform_real<> uniform(0,1);
    if (uniform(random) <= 1./++count)
      value = x;
  }
};

} // unnamed namespace

int main(const int argc, const char** argv) {
  // Read command line arguments
  if (argc < 3)
    die(string("usage: ")+argv[0]+" <config> <image-or-dir>...\nexample: "+argv[0]+" config data/red-circle.gif");
  boost::property_tree::ini_parser::read_ini(argv[1],Options::props);
  const vector<string> paths(argv+2,argv+argc);
  const Options o;
  if (!o.strips.size())
    die("Need at least one strip");

  // Initialize randomness
  boost::random::mt19937 random;

  // Locked POV object
  Spinlock spin;
  unique_ptr<POV> pov;

  // Load a random image
  const function<void()> reload = [&]() {
    // Image file extensions
    static const unordered_set<string> image_exts({".png",".jpg",".jpeg",".gif",".bmp"});

    // Pick a random image of the given paths
    Choice<string> choice(random);
    for (const auto& root : paths) {
      if (!boost::filesystem::is_directory(root))
        choice.add(root);
      else {
        boost::filesystem::recursive_directory_iterator dir(root), end;
        while (dir != end) {
          const auto& path = dir->path();
          const auto& file = path.filename();
          if (file.empty() || file.string()[0]=='.')
            dir.no_push();
          else {
            string ext = extension(file);
            to_lower(ext);
            if (image_exts.count(ext))
              choice.add(path.string());
          }
          ++dir;
        }
      }
    }

    // Load image
    const auto image = choice.value;
    cout<<"Loading image "<<image<<endl;

    // Prepare POV
    // TOD: Don't assume all strips are the same size
    spin.lock();
    pov.reset(new POV(o,o.strips[0].size,image));
    spin.unlock();
  };

  // Prepare for mouse support if desired
  int mouse_fd = -1;
  int64_t mouse_n = int64_t(1)<<62;
  double mouse_x = 0;
  if (o.use_mouse) {
    mouse_fd = open(o.mouse_dev.c_str(),O_RDONLY|O_NONBLOCK);
    if (mouse_fd < 0)
      die("Can't open mouse device '"+o.mouse_dev+"': "+strerror(errno));
    mouse_n = int64_t(ceil(o.frame_rate/o.mouse_frequency));
  }

  // Display images forever
  const double dt = 1./o.frame_rate;
  const int64_t reload_n = int64_t(ceil(o.reload_time*o.frame_rate));
  const double t0 = get_time();
  for (int64_t n=0;;n++) {
    // Reload image
    if (n%reload_n==0)
      async(reload);

    // Draw on LEDs
    spin.lock();
    if (pov) {
      const auto x = o.use_mouse ? pov->around*mouse_x : n;
      for (const int i : range(o.strips.size()))
        pov->send(o.strips[i],int64_t(ceil(x+pov->around*i/o.total_strips)));
    }
    spin.unlock();

    // Update mouse
    if (o.use_mouse && n%mouse_n==0) {
      // TODO
    }

    // Wait for next frame
    const auto t = get_time();
    good_sleep(dt*(n+1)-(t-t0));
    if (n && n%1000==0)
      cout<<"n "<<n/1000<<"k, t "<<t-t0<<endl;

    // Are we done?
    if (!o.animate)
      break;
  }

  return 0;
}
