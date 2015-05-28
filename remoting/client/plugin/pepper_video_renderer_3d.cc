// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/plugin/pepper_video_renderer_3d.h"

#include <math.h>

#include "base/callback_helpers.h"
#include "base/stl_util.h"
#include "ppapi/c/pp_codecs.h"
#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/lib/gl/include/GLES2/gl2.h"
#include "ppapi/lib/gl/include/GLES2/gl2ext.h"
#include "remoting/proto/video.pb.h"
#include "remoting/protocol/session_config.h"

namespace remoting {

class PepperVideoRenderer3D::PendingPacket {
 public:
  PendingPacket(scoped_ptr<VideoPacket> packet, const base::Closure& done)
      : packet_(packet.Pass()),
        done_runner_(done) {
  }

  ~PendingPacket() {}

  const VideoPacket* packet() const { return packet_.get(); }

 private:
  scoped_ptr<VideoPacket> packet_;
  base::ScopedClosureRunner done_runner_;
};


class PepperVideoRenderer3D::Picture {
 public:
  Picture(pp::VideoDecoder* decoder, PP_VideoPicture picture)
      : decoder_(decoder), picture_(picture) {}
  ~Picture() { decoder_->RecyclePicture(picture_); }

  const PP_VideoPicture& picture() { return picture_; }

 private:
  pp::VideoDecoder* decoder_;
  PP_VideoPicture picture_;
};


PepperVideoRenderer3D::FrameDecodeTimestamp::FrameDecodeTimestamp(
    uint32_t frame_id,
    base::TimeTicks decode_started_time)
    : frame_id(frame_id), decode_started_time(decode_started_time) {
}

PepperVideoRenderer3D::PepperVideoRenderer3D()
    : event_handler_(nullptr),
      latest_input_event_timestamp_(0),
      initialization_finished_(false),
      decode_pending_(false),
      get_picture_pending_(false),
      paint_pending_(false),
      latest_frame_id_(0),
      force_repaint_(false),
      current_shader_program_texture_target_(0),
      shader_program_(0),
      shader_texcoord_scale_location_(0),
      callback_factory_(this) {
}

PepperVideoRenderer3D::~PepperVideoRenderer3D() {
  if (shader_program_)
    gles2_if_->DeleteProgram(graphics_.pp_resource(), shader_program_);

  STLDeleteElements(&pending_packets_);
}

bool PepperVideoRenderer3D::Initialize(pp::Instance* instance,
                                       const ClientContext& context,
                                       EventHandler* event_handler) {
  DCHECK(event_handler);
  DCHECK(!event_handler_);

  event_handler_ = event_handler;

  const int32_t context_attributes[] = {
      PP_GRAPHICS3DATTRIB_ALPHA_SIZE,     8,
      PP_GRAPHICS3DATTRIB_BLUE_SIZE,      8,
      PP_GRAPHICS3DATTRIB_GREEN_SIZE,     8,
      PP_GRAPHICS3DATTRIB_RED_SIZE,       8,
      PP_GRAPHICS3DATTRIB_DEPTH_SIZE,     0,
      PP_GRAPHICS3DATTRIB_STENCIL_SIZE,   0,
      PP_GRAPHICS3DATTRIB_SAMPLES,        0,
      PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS, 0,
      PP_GRAPHICS3DATTRIB_WIDTH,          640,
      PP_GRAPHICS3DATTRIB_HEIGHT,         480,
      PP_GRAPHICS3DATTRIB_NONE,
  };
  graphics_ = pp::Graphics3D(instance, context_attributes);

  if (graphics_.is_null()) {
    LOG(WARNING) << "Graphics3D interface is not available.";
    return false;
  }
  if (!instance->BindGraphics(graphics_)) {
    LOG(WARNING) << "Failed to bind Graphics3D.";
    return false;
  }

  // Fetch the GLES2 interface to use to render frames.
  gles2_if_ = static_cast<const PPB_OpenGLES2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  CHECK(gles2_if_);

  video_decoder_ = pp::VideoDecoder(instance);
  if (video_decoder_.is_null()) {
    LOG(WARNING) << "VideoDecoder interface is not available.";
    return false;
  }

  PP_Resource graphics_3d = graphics_.pp_resource();

  gles2_if_->ClearColor(graphics_3d, 1, 0, 0, 1);
  gles2_if_->Clear(graphics_3d, GL_COLOR_BUFFER_BIT);

  // Assign vertex positions and texture coordinates to buffers for use in
  // shader program.
  static const float kVertices[] = {
      -1, -1, -1, 1, 1, -1, 1, 1,  // Position coordinates.
      0,  1,  0,  0, 1, 1,  1, 0,  // Texture coordinates.
  };

  GLuint buffer;
  gles2_if_->GenBuffers(graphics_3d, 1, &buffer);
  gles2_if_->BindBuffer(graphics_3d, GL_ARRAY_BUFFER, buffer);
  gles2_if_->BufferData(graphics_3d, GL_ARRAY_BUFFER, sizeof(kVertices),
                        kVertices, GL_STATIC_DRAW);

  CheckGLError();

  return true;
}

void PepperVideoRenderer3D::OnViewChanged(const pp::View& view) {
  pp::Size size = view.GetRect().size();
  float scale = view.GetDeviceScale();
  view_size_.set(ceilf(size.width() * scale), ceilf(size.height() * scale));
  graphics_.ResizeBuffers(view_size_.width(), view_size_.height());

  force_repaint_ = true;
  PaintIfNeeded();
}

void PepperVideoRenderer3D::EnableDebugDirtyRegion(bool enable) {
  debug_dirty_region_ = enable;
}

void PepperVideoRenderer3D::OnSessionConfig(
    const protocol::SessionConfig& config) {
  PP_VideoProfile video_profile = PP_VIDEOPROFILE_VP8_ANY;
  switch (config.video_config().codec) {
    case protocol::ChannelConfig::CODEC_VP8:
      video_profile = PP_VIDEOPROFILE_VP8_ANY;
      break;
    case protocol::ChannelConfig::CODEC_VP9:
      video_profile = PP_VIDEOPROFILE_VP9_ANY;
      break;
    default:
      NOTREACHED();
  }
  int32_t result = video_decoder_.Initialize(
      graphics_, video_profile, PP_HARDWAREACCELERATION_WITHFALLBACK,
      callback_factory_.NewCallback(&PepperVideoRenderer3D::OnInitialized));
  CHECK_EQ(result, PP_OK_COMPLETIONPENDING)
      << "video_decoder_.Initialize() returned " << result;
}

ChromotingStats* PepperVideoRenderer3D::GetStats() {
  return &stats_;
}

protocol::VideoStub* PepperVideoRenderer3D::GetVideoStub() {
  return this;
}

void PepperVideoRenderer3D::ProcessVideoPacket(scoped_ptr<VideoPacket> packet,
                                               const base::Closure& done) {
  base::ScopedClosureRunner done_runner(done);

  // Don't need to do anything if the packet is empty. Host sends empty video
  // packets when the screen is not changing.
  if (!packet->data().size())
    return;

  // Update statistics.
  stats_.video_frame_rate()->Record(1);
  stats_.video_bandwidth()->Record(packet->data().size());
  if (packet->has_capture_time_ms())
    stats_.video_capture_ms()->Record(packet->capture_time_ms());
  if (packet->has_encode_time_ms())
    stats_.video_encode_ms()->Record(packet->encode_time_ms());
  if (packet->has_latest_event_timestamp() &&
      packet->latest_event_timestamp() > latest_input_event_timestamp_) {
    latest_input_event_timestamp_ = packet->latest_event_timestamp();
    base::TimeDelta round_trip_latency =
        base::Time::Now() -
        base::Time::FromInternalValue(packet->latest_event_timestamp());
    stats_.round_trip_ms()->Record(round_trip_latency.InMilliseconds());
  }

  bool resolution_changed = false;

  if (packet->format().has_screen_width() &&
      packet->format().has_screen_height()) {
    webrtc::DesktopSize frame_size(packet->format().screen_width(),
                                   packet->format().screen_height());
    if (!frame_size_.equals(frame_size)) {
      frame_size_ = frame_size;
      resolution_changed = true;
    }
  }

  if (packet->format().has_x_dpi() && packet->format().has_y_dpi()) {
    webrtc::DesktopVector frame_dpi(packet->format().x_dpi(),
                                    packet->format().y_dpi());
    if (!frame_dpi_.equals(frame_dpi)) {
      frame_dpi_ = frame_dpi;
      resolution_changed = true;
    }
  }

  if (resolution_changed)
    event_handler_->OnVideoSize(frame_size_, frame_dpi_);

  // Update the desktop shape region.
  webrtc::DesktopRegion desktop_shape;
  if (packet->has_use_desktop_shape()) {
    for (int i = 0; i < packet->desktop_shape_rects_size(); ++i) {
      Rect remoting_rect = packet->desktop_shape_rects(i);
      desktop_shape.AddRect(webrtc::DesktopRect::MakeXYWH(
          remoting_rect.x(), remoting_rect.y(),
          remoting_rect.width(), remoting_rect.height()));
    }
  } else {
    // Fallback for the case when the host didn't include the desktop shape.
    desktop_shape =
        webrtc::DesktopRegion(webrtc::DesktopRect::MakeSize(frame_size_));
  }

  if (!desktop_shape_.Equals(desktop_shape)) {
    desktop_shape_.Swap(&desktop_shape);
    event_handler_->OnVideoShape(desktop_shape_);
  }

  // Report the dirty region, for debugging, if requested.
  if (debug_dirty_region_) {
    webrtc::DesktopRegion dirty_region;
    for (int i = 0; i < packet->dirty_rects_size(); ++i) {
      Rect remoting_rect = packet->dirty_rects(i);
      dirty_region.AddRect(webrtc::DesktopRect::MakeXYWH(
          remoting_rect.x(), remoting_rect.y(),
          remoting_rect.width(), remoting_rect.height()));
    }
    event_handler_->OnVideoFrameDirtyRegion(dirty_region);
  }

  pending_packets_.push_back(
      new PendingPacket(packet.Pass(), done_runner.Release()));
  DecodeNextPacket();
}

void PepperVideoRenderer3D::OnInitialized(int32_t result) {
  // Assume that VP8 and VP9 codecs are always supported by the browser.
  CHECK_EQ(result, PP_OK) << "VideoDecoder::Initialize() failed: " << result;
  initialization_finished_ = true;

  // Start decoding in case a frame was received during decoder initialization.
  DecodeNextPacket();
}

void PepperVideoRenderer3D::DecodeNextPacket() {
  if (!initialization_finished_ || decode_pending_ || pending_packets_.empty())
    return;

  ++latest_frame_id_;
  frame_decode_timestamps_.push_back(
      FrameDecodeTimestamp(latest_frame_id_, base::TimeTicks::Now()));

  const VideoPacket* packet = pending_packets_.front()->packet();

  int32_t result = video_decoder_.Decode(
      latest_frame_id_, packet->data().size(), packet->data().data(),
      callback_factory_.NewCallback(&PepperVideoRenderer3D::OnDecodeDone));
  CHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  decode_pending_ = true;
}

void PepperVideoRenderer3D::OnDecodeDone(int32_t result) {
  DCHECK(decode_pending_);
  decode_pending_ = false;

  if (result != PP_OK) {
    LOG(ERROR) << "VideoDecoder::Decode() returned " << result;
    event_handler_->OnVideoDecodeError();
    return;
  }

  delete pending_packets_.front();
  pending_packets_.pop_front();

  DecodeNextPacket();
  GetNextPicture();
}

void PepperVideoRenderer3D::GetNextPicture() {
  if (get_picture_pending_)
    return;

  int32_t result =
      video_decoder_.GetPicture(callback_factory_.NewCallbackWithOutput(
          &PepperVideoRenderer3D::OnPictureReady));
  CHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  get_picture_pending_ = true;
}

void PepperVideoRenderer3D::OnPictureReady(int32_t result,
                                           PP_VideoPicture picture) {
  DCHECK(get_picture_pending_);
  get_picture_pending_ = false;

  if (result != PP_OK) {
    LOG(ERROR) << "VideoDecoder::GetPicture() returned " << result;
    event_handler_->OnVideoDecodeError();
    return;
  }

  CHECK(!frame_decode_timestamps_.empty());
  const FrameDecodeTimestamp& frame_timer = frame_decode_timestamps_.front();

  if (picture.decode_id != frame_timer.frame_id) {
    LOG(ERROR)
        << "Received a video packet that didn't contain a complete frame.";
    event_handler_->OnVideoDecodeError();
    return;
  }

  base::TimeDelta decode_time =
      base::TimeTicks::Now() - frame_timer.decode_started_time;
  stats_.video_decode_ms()->Record(decode_time.InMilliseconds());
  frame_decode_timestamps_.pop_front();

  next_picture_.reset(new Picture(&video_decoder_, picture));

  PaintIfNeeded();
  GetNextPicture();
}

void PepperVideoRenderer3D::PaintIfNeeded() {
  bool need_repaint = next_picture_ || (force_repaint_ && current_picture_);
  if (paint_pending_ || !need_repaint)
    return;

  if (next_picture_)
    current_picture_ = next_picture_.Pass();

  force_repaint_ = false;
  latest_paint_started_time_ = base::TimeTicks::Now();

  const PP_VideoPicture& picture = current_picture_->picture();
  PP_Resource graphics_3d = graphics_.pp_resource();

  EnsureProgramForTexture(picture.texture_target);

  gles2_if_->UseProgram(graphics_3d, shader_program_);

  // Calculate v_scale passed to the vertex shader.
  double scale_x = picture.visible_rect.size.width;
  double scale_y = picture.visible_rect.size.height;
  if (picture.texture_target != GL_TEXTURE_RECTANGLE_ARB) {
    scale_x /= picture.texture_size.width;
    scale_y /= picture.texture_size.height;
  }
  gles2_if_->Uniform2f(graphics_3d, shader_texcoord_scale_location_,
                       scale_x, scale_y);

  // Set viewport position & dimensions.
  gles2_if_->Viewport(graphics_3d, 0, 0, view_size_.width(),
                      view_size_.height());

  // Select the texture unit GL_TEXTURE0.
  gles2_if_->ActiveTexture(graphics_3d, GL_TEXTURE0);

  // Select the texture.
  gles2_if_->BindTexture(graphics_3d, picture.texture_target,
                         picture.texture_id);

  // Select linear filter in case the texture needs to be scaled.
  gles2_if_->TexParameteri(graphics_3d, picture.texture_target,
                           GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  // Render texture by drawing a triangle strip with 4 vertices.
  gles2_if_->DrawArrays(graphics_3d, GL_TRIANGLE_STRIP, 0, 4);

  CheckGLError();

  // Request PPAPI display the queued texture.
  int32_t result = graphics_.SwapBuffers(
      callback_factory_.NewCallback(&PepperVideoRenderer3D::OnPaintDone));
  CHECK_EQ(result, PP_OK_COMPLETIONPENDING);
  paint_pending_ = true;
}

void PepperVideoRenderer3D::OnPaintDone(int32_t result) {
  CHECK_EQ(result, PP_OK) << "Graphics3D::SwapBuffers() failed";

  paint_pending_ = false;
  base::TimeDelta paint_time =
      base::TimeTicks::Now() - latest_paint_started_time_;
  stats_.video_paint_ms()->Record(paint_time.InMilliseconds());

  PaintIfNeeded();
}

void PepperVideoRenderer3D::EnsureProgramForTexture(uint32_t texture_target) {
  static const char kVertexShader[] =
      "varying vec2 v_texCoord;            \n"
      "attribute vec4 a_position;          \n"
      "attribute vec2 a_texCoord;          \n"
      "uniform vec2 v_scale;               \n"
      "void main()                         \n"
      "{                                   \n"
      "    v_texCoord = v_scale * a_texCoord; \n"
      "    gl_Position = a_position;       \n"
      "}";

  static const char kFragmentShader2D[] =
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform sampler2D s_texture;        \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2D(s_texture, v_texCoord); \n"
      "}";

  static const char kFragmentShaderRectangle[] =
      "#extension GL_ARB_texture_rectangle : require\n"
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform sampler2DRect s_texture;    \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2DRect(s_texture, v_texCoord).rgba; \n"
      "}";

  static const char kFragmentShaderExternal[] =
      "#extension GL_OES_EGL_image_external : require\n"
      "precision mediump float;            \n"
      "varying vec2 v_texCoord;            \n"
      "uniform samplerExternalOES s_texture; \n"
      "void main()                         \n"
      "{"
      "    gl_FragColor = texture2D(s_texture, v_texCoord); \n"
      "}";

  // Initialize shader program only if texture type has changed.
  if (current_shader_program_texture_target_ != texture_target) {
    current_shader_program_texture_target_ = texture_target;

    if (texture_target == GL_TEXTURE_2D) {
      CreateProgram(kVertexShader, kFragmentShader2D);
    } else if (texture_target == GL_TEXTURE_RECTANGLE_ARB) {
      CreateProgram(kVertexShader, kFragmentShaderRectangle);
    } else if (texture_target == GL_TEXTURE_EXTERNAL_OES) {
      CreateProgram(kVertexShader, kFragmentShaderExternal);
    } else {
      LOG(FATAL) << "Unknown texture target: " << texture_target;
    }
  }
}

void PepperVideoRenderer3D::CreateProgram(const char* vertex_shader,
                                          const char* fragment_shader) {
  PP_Resource graphics_3d = graphics_.pp_resource();
  if (shader_program_)
    gles2_if_->DeleteProgram(graphics_3d, shader_program_);

  // Create shader program.
  shader_program_ = gles2_if_->CreateProgram(graphics_3d);
  CreateShaderProgram(GL_VERTEX_SHADER, vertex_shader);
  CreateShaderProgram(GL_FRAGMENT_SHADER, fragment_shader);
  gles2_if_->LinkProgram(graphics_3d, shader_program_);
  gles2_if_->UseProgram(graphics_3d, shader_program_);
  gles2_if_->Uniform1i(
      graphics_3d,
      gles2_if_->GetUniformLocation(graphics_3d, shader_program_, "s_texture"),
      0);
  CheckGLError();

  shader_texcoord_scale_location_ = gles2_if_->GetUniformLocation(
      graphics_3d, shader_program_, "v_scale");

  GLint pos_location = gles2_if_->GetAttribLocation(
      graphics_3d, shader_program_, "a_position");
  GLint tc_location = gles2_if_->GetAttribLocation(
      graphics_3d, shader_program_, "a_texCoord");
  CheckGLError();

  // Construct the vertex array for DrawArrays(), using the buffer created in
  // Initialize().
  gles2_if_->EnableVertexAttribArray(graphics_3d, pos_location);
  gles2_if_->VertexAttribPointer(graphics_3d, pos_location, 2, GL_FLOAT,
                                 GL_FALSE, 0, 0);
  gles2_if_->EnableVertexAttribArray(graphics_3d, tc_location);
  gles2_if_->VertexAttribPointer(
      graphics_3d, tc_location, 2, GL_FLOAT, GL_FALSE, 0,
      static_cast<float*>(0) + 8);  // Skip position coordinates.

  gles2_if_->UseProgram(graphics_3d, 0);

  CheckGLError();
}

void PepperVideoRenderer3D::CreateShaderProgram(int type, const char* source) {
  int size = strlen(source);
  GLuint shader = gles2_if_->CreateShader(graphics_.pp_resource(), type);
  gles2_if_->ShaderSource(graphics_.pp_resource(), shader, 1, &source, &size);
  gles2_if_->CompileShader(graphics_.pp_resource(), shader);
  gles2_if_->AttachShader(graphics_.pp_resource(), shader_program_, shader);
  gles2_if_->DeleteShader(graphics_.pp_resource(), shader);
}

void PepperVideoRenderer3D::CheckGLError() {
  GLenum error = gles2_if_->GetError(graphics_.pp_resource());
  CHECK_EQ(error, static_cast<GLenum>(GL_NO_ERROR)) << "GL error: " << error;
}

}  // namespace remoting
