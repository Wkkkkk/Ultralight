#include "VideoStream.h"

VideoStream::VideoStream(int w, int h, int fps) {
  sr_webcam_create(&device, 0);
  sr_webcam_set_format(device, w, h, fps);
  sr_webcam_set_callback(device, &VideoStream::callback);
  sr_webcam_set_user(device, this);
  if (sr_webcam_open(device) != 0) {
    std::cerr << "Unable to open device." << std::endl;
    return;
  }
  buffer.resize(sr_webcam_get_format_size(device));
  sr_webcam_get_dimensions(device, &w_, &h_);
  sr_webcam_get_framerate(device, &fps_);
}

VideoStream::~VideoStream() { sr_webcam_delete(device); }

void VideoStream::start() { sr_webcam_start(device); }

void VideoStream::stop() { sr_webcam_stop(device); }

void VideoStream::callback(sr_webcam_device *device, void *data) {
  VideoStream *stream = static_cast<VideoStream *>(sr_webcam_get_user(device));
  stream->dataReceived(data, sr_webcam_get_format_size(device));
}

void VideoStream::dataReceived(void *data, size_t size) {
  std::lock_guard<std::mutex> guard(mutex);
  uint8_t *rgb = static_cast<uint8_t *>(data);
  std::copy(rgb, rgb + size, buffer.begin());
  gotNewFrame = true;
}

bool VideoStream::getRGB(std::vector<uint8_t> &dst) {
  std::lock_guard<std::mutex> guard(mutex);
  if (!gotNewFrame) { return false; }

  dst.swap(buffer);
  gotNewFrame = false;
  return true;
}
