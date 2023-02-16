#include "Sample.h"
#include "GLTextureSurface.h"
#include "WebTile.h"
#include "VideoStream.h"

#include <AppCore/Platform.h>
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <math.h>
#include <cmath>
#include <fstream>
#include <string>
#include <thread>
#include <chrono>

extern "C" {

// Called by GLFW when an error is encountered.
static void GLFW_error_callback(int error, const char* description) {
  fprintf(stderr, "GLFW Error: %s\n", description);
}

}

Sample::Sample() {
  glfwSetErrorCallback(GLFW_error_callback);

  if (!glfwInit())
    exit(EXIT_FAILURE);

  const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  int monitor_width = mode->width;
  int monitor_height = mode->height;

  // Make initial window size 80% of monitor's dimensions
  width_ = static_cast<int>(monitor_width * 0.8);
  height_ = static_cast<int>(monitor_height * 0.8);

  // Clamp initial window width to 1920 pixels
  if (width_ > 1920) width_ = 1920;

  window_.reset(new Window(width_, height_, false));
  window_->MoveToCenter();
  window_->set_listener(this);
  window_->SetTitle("Ultralight - Web Camera");

  width_ = window_->width();
  height_ = window_->height();
  
  glOrtho(0, width_, 0, height_, -1, 1);
  glEnable(GL_TEXTURE_2D);

  Config config;
  config.scroll_timer_delay = 1.0 / (mode->refreshRate);
  config.animation_timer_delay = 1.0 / (mode->refreshRate);

  ///
  /// Pass our configuration to the Platform singleton so that the library
  /// can use it.
  ///
  /// The Platform singleton can be used to define various platform-specific
  /// properties and handlers such as file system access, font loaders, and
  /// the gpu driver.
  ///
  Platform::instance().set_config(config);

  ///
  /// Use AppCore's font loader singleton to load fonts from the OS.
  ///
  /// You could replace this with your own to provide your own fonts.
  ///
  Platform::instance().set_font_loader(GetPlatformFontLoader());

  ///
  /// Use AppCore's file system singleton to load file:/// URLs from the OS.
  ///
  /// You could replace this with your own to provide your own file loader
  /// (useful if you need to bundle encrypted / compressed HTML assets).
  ///
  Platform::instance().set_file_system(GetPlatformFileSystem("./assets"));

  ///
  /// Register our Sample instance as a logger so we can handle the
  /// library's LogMessage() event below in case we encounter an error.
  ///
  Platform::instance().set_logger(this);

  ///
  /// Use a custom factory to create Surfaces backed by an OpenGL texture.
  ///
  surface_factory_.reset(new GLTextureSurfaceFactory());
  Platform::instance().set_surface_factory(surface_factory_.get());

  ///
  /// Create our Renderer (you should only create this once per application).
  /// 
  /// The Renderer singleton maintains the lifetime of the library and
  /// is required before creating any Views. It should outlive any Views.
  ///
  /// You should set up the Platform singleton before creating this.
  ///
  renderer_ = Renderer::Create();
  addWebTileWithURL("file:///welcome.html", width_, height_);

  // Set our first WebTile as active
  active_web_tile_ = 0;
  web_tiles_[active_web_tile_]->view()->Focus();

  // Connect to Web Camera which takes a quarter of the screen
  int video_width = width_ / 2;
  int video_height = height_ / 2;
  video_stream_.reset(new VideoStream(video_width, video_height, 30));
  int vidW = video_stream_->w();
  int vidH = video_stream_->h();
  video_buffer_.reserve(vidW * vidH * 3);  // RBG format by default
  video_stream_->start();

  // Add the video tile
  video_tile_.reset(new WebTile(renderer_, video_width, video_height, window_->scale()));
  video_tile_->view()->set_view_listener(this);
  video_tile_->view()->set_load_listener(this);
}

Sample::~Sample() {
  web_tiles_.clear();
  renderer_ = nullptr;

  glfwTerminate();
}

void Sample::addWebTileWithURL(const std::string& url, int width,
                                    int height) {
  WebTile* tile = new WebTile(renderer_, width, height, window_->scale());
  tile->view()->LoadURL(url.c_str());
  tile->view()->set_view_listener(this);
  tile->view()->set_load_listener(this);

  web_tiles_.push_back(std::unique_ptr<WebTile>(tile));
}

void Sample::Run() {
  ///
  /// Our main run loop tries to conserve CPU usage by sleeping in 40ms(25fps) bursts between each paint.
  ///
  /// We use glfwWaitEventsTimeout() to perform the sleep, which also allows
  /// us the ability to wake up early if the OS sends us an event.
  ///
  std::chrono::milliseconds interval_ms(40);
  std::chrono::steady_clock::time_point next_paint = std::chrono::steady_clock::now();
  
  while (!glfwWindowShouldClose(window_->handle())) {
    ///
    /// Query the system clock to see how many milliseconds are left until
    /// the next scheduled paint.
    ///
    long long timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      next_paint - std::chrono::steady_clock::now()).count();
    unsigned long timeout = timeout_ms <= 0 ? 0 : (unsigned long)timeout_ms;

    ///
    /// Use glfwWaitEventsTimeout() if we still have time left before the next
    /// paint. (Will use OS primitives to sleep and wait for OS input events)
    ///
    /// Otherwise, call glfwPollEvents() immediately and continue.
    ///
    if (timeout > 0)
      glfwWaitEventsTimeout(timeout / 1000.0);
    else
      glfwPollEvents();

    ///
    /// Allow Ultralight to update internal timers, JavaScript callbacks, and
    /// other resource callbacks.
    ///
    renderer_->Update();

    if (should_quit_)
      return;

    ///
    /// Update our timeout by querying the system clock again.
    ///
    timeout_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      next_paint - std::chrono::steady_clock::now()).count();

    ///
    /// Perform paint if we have reached the scheduled time.
    ///
    if (timeout_ms <= 0) {
      ///
      /// Re-paint the Surfaces of all Views as needed.
      ///
      renderer_->Render();

      ///
      /// Draw the WebTile overlays with OpenGL.
      ///
      draw();

      ///
      /// Schedule the next paint.
      ///
      next_paint = std::chrono::steady_clock::now() + interval_ms;
    }
  }
}

void Sample::draw() {
  // Draw the active web tile first
  GLTextureSurface* surface = web_tiles_[active_web_tile_]->surface();
  if (surface) {
    int tileWidth = surface->width();
    int tileHeight = surface->height();
    auto id = surface->GetTextureAndSyncIfNeeded();
    glViewport(0,0,width_,height_);
    glOrtho(0, width_, 0, height_, -1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glScalef(1,1,1);
    glBindTexture(GL_TEXTURE_2D, id);
    glColor4f(1,1,1,1);
    glBegin(GL_QUADS);
    glTexCoord2f(0,1);
    glVertex3f(0, 0, 0.0f);
    glTexCoord2f(1,1);
    glVertex3f((GLfloat)tileWidth, 0, 0.0f);
    glTexCoord2f(1,0);
    glVertex3f((GLfloat)tileWidth, (GLfloat)tileHeight, 0.0f);
    glTexCoord2f(0,0);
    glVertex3f(0, (GLfloat)tileHeight, 0.0f);
    glEnd();
    glBindTexture(GL_TEXTURE_2D, 0);

    GLuint srcFb;
    glGenFramebuffers(1, &srcFb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, id, 0);
    glBlitFramebuffer(0, 0, tileWidth, tileHeight, 0, tileHeight, tileWidth, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &srcFb);
  }

  // Draw the video tile later
  GLTextureSurface* video_surface = video_tile_->surface();
  if(video_surface && video_stream_->getRGB(video_buffer_)) {        
    int srcW, srcH, dstW, dstH, flip;
    srcW = dstW = video_stream_->w();
    srcH = dstH = video_stream_->h();
    flip = 1;

    auto texture_id = video_surface->GetTextureAndSyncIfNeeded();
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB8, srcW, srcH, 0, GL_RGB, GL_UNSIGNED_BYTE, &video_buffer_[0]);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    GLuint srcFb;
    glGenFramebuffers(1, &srcFb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, srcFb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture_id, 0);
    glBlitFramebuffer(0, 0, srcW, srcH, 0, flip * dstH, dstW, (1 - flip) * dstH, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
    glDeleteFramebuffers(1, &srcFb);
  }
  
  window_->PresentFrame();
}

void Sample::OnClose() {
  should_quit_ = true;
}

void Sample::OnResize(uint32_t width, uint32_t height) {
  if (width == width_ && height == height_)
    return;

  width_ = width;
  height_ = height;  

  for (auto& i : web_tiles_) {
    i->view()->Resize(width, height);
  }

  glViewport(0,0,width_,height_);
}

void Sample::OnChangeFocus(bool focused) {
}

void Sample::OnKeyEvent(const KeyEvent& evt) {
  if (evt.type == evt.kType_RawKeyDown) {
    if (evt.virtual_key_code == KeyCodes::GK_ESCAPE) {
      should_quit_ = true;
      return;
    }
    else if (evt.modifiers & KeyEvent::kMod_AltKey) {
      if (evt.virtual_key_code == KeyCodes::GK_LEFT) {
        web_tiles_[active_web_tile_]->view()->GoBack();
        return;
      }
      else if (evt.virtual_key_code == KeyCodes::GK_RIGHT) {
        web_tiles_[active_web_tile_]->view()->GoForward();
        return;
      }
      else if (evt.virtual_key_code == KeyCodes::GK_T) {
        web_tiles_[active_web_tile_]->ToggleTransparency();
        return;
      }
      else if (evt.virtual_key_code == KeyCodes::GK_D) {
        addWebTileWithURL("http://www.bing.com", width_, height_);

        active_web_tile_ = (int)web_tiles_.size() - 1;
        return;
      }
      else if (evt.virtual_key_code == KeyCodes::GK_X) {
        if (web_tiles_.size() > 1) {
          for (auto i = web_tiles_.begin(); i != web_tiles_.end(); ++i) {
            if (*i == web_tiles_[active_web_tile_]) {
              web_tiles_.erase(i);
              break;
            }
          }
          if (active_web_tile_ > 0) {
            active_web_tile_--;
          }

          web_tiles_[active_web_tile_]->view()->Focus();
          return;
        }
      }
    }
  }

  web_tiles_[active_web_tile_]->view()->FireKeyEvent(evt);
}

void Sample::OnMouseEvent(const MouseEvent& evt) {
  web_tiles_[active_web_tile_]->view()->FireMouseEvent(evt);
}

void Sample::OnScrollEvent(const ScrollEvent& evt) {
  web_tiles_[active_web_tile_]->view()->FireScrollEvent(evt);
}

void Sample::OnChangeCursor(ultralight::View* caller,
  Cursor cursor) {
  window_->SetCursor(cursor);
}

RefPtr<View> Sample::OnCreateChildView(ultralight::View* caller,
  const String& opener_url,
  const String& target_url,
  bool is_popup,
  const IntRect& popup_rect) {
  ViewConfig view_config;
  view_config.is_accelerated = false;
  view_config.initial_device_scale = window_->scale();
  RefPtr<View> new_view = renderer_->CreateView(width_, height_, view_config, nullptr);
  WebTile* new_tile = new WebTile(new_view);
  new_view->set_view_listener(this);
  new_view->set_load_listener(this);

  web_tiles_.push_back(std::unique_ptr<WebTile>(new_tile));

  return new_view;
}

void Sample::OnFailLoading(ultralight::View* caller,
  uint64_t frame_id,
  bool is_main_frame,
  const String& url,
  const String& description,
  const String& error_domain,
  int error_code) {
}

void Sample::LogMessage(LogLevel log_level,
  const String& message) {
  String msg(message);
  std::cout << msg.utf8().data() << std::endl;
}
