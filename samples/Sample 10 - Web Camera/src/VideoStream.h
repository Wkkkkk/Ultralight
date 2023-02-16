#pragma once

#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

#include <sr_webcam.h>

class VideoStream {
public:
  VideoStream(int w, int h, int fps);
  ~VideoStream();

  void start();
  void stop();

  // Save the video into buffer (RGB format)
  // Returns true if buffer is new
  bool getRGB(std::vector<uint8_t> &dst);

  int w() const { return w_; }
  int h() const { return h_; }
  int fps() const { return fps_; }

  // Triggered when new data is available
  static void callback(sr_webcam_device *device, void *data);

private:
  void dataReceived(void *data, size_t size);

  std::mutex mutex;
  std::vector<uint8_t> buffer;
  sr_webcam_device* device;

  bool gotNewFrame = false;
  int w_ = 0, h_ = 0, fps_ = 0;
};