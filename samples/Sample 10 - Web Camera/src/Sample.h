#pragma once

#include <string>
#include <vector>

#include "GLTextureSurface.h"
#include "Window.h"
#include <Ultralight/platform/Logger.h>

using namespace ultralight;

class WebTile;
class VideoStream;

///
/// Sample is responsible for all high-level logic.
///
class Sample : public WindowListener,
               public ViewListener,
               public LoadListener,
               public Logger {
public:
  Sample();
  ~Sample();

  void addWebTileWithURL(const std::string &url, int width, int height);

  void Run();

  void draw();

  /////////////////////////////////////////////////////////////////////////////
  /// Inherited from WindowListener:                                        ///
  /////////////////////////////////////////////////////////////////////////////

  virtual void OnClose() override;

  virtual void OnResize(uint32_t width, uint32_t height) override;

  virtual void OnChangeFocus(bool focused) override;

  virtual void OnKeyEvent(const KeyEvent &evt) override;

  virtual void OnMouseEvent(const MouseEvent &evt) override;

  virtual void OnScrollEvent(const ScrollEvent &evt) override;

  /////////////////////////////////////////////////////////////////////////////
  /// Inherited from ViewListener:                                          ///
  /////////////////////////////////////////////////////////////////////////////

  virtual void OnChangeCursor(ultralight::View *caller, Cursor cursor) override;

  virtual RefPtr<View> OnCreateChildView(ultralight::View *caller,
                                         const String &opener_url,
                                         const String &target_url,
                                         bool is_popup,
                                         const IntRect &popup_rect) override;

  /////////////////////////////////////////////////////////////////////////////
  /// Inherited from LoadListener:                                          ///
  /////////////////////////////////////////////////////////////////////////////

  virtual void OnFailLoading(ultralight::View *caller, uint64_t frame_id,
                             bool is_main_frame, const String &url,
                             const String &description,
                             const String &error_domain,
                             int error_code) override;

  /////////////////////////////////////////////////////////////////////////////
  /// Inherited from Logger:                                                ///
  /////////////////////////////////////////////////////////////////////////////

  virtual void LogMessage(LogLevel log_level, const String &message) override;

protected:
  bool should_quit_ = false;
  int active_web_tile_ = -1;
  int width_, height_;
  
  std::unique_ptr<VideoStream> video_stream_;
  std::vector<uint8_t> video_buffer_;
  std::unique_ptr<WebTile> video_tile_;

  std::vector<std::unique_ptr<WebTile>> web_tiles_;
  RefPtr<Renderer> renderer_;
  std::unique_ptr<GLTextureSurfaceFactory> surface_factory_;
  std::unique_ptr<Window> window_;
};
