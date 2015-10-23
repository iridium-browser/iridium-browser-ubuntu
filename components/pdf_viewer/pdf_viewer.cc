// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/hash_tables.h"
#include "base/memory/scoped_ptr.h"
#include "components/view_manager/public/cpp/lib/view_manager_client_impl.h"
#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/public/cpp/view.h"
#include "components/view_manager/public/cpp/view_manager.h"
#include "components/view_manager/public/cpp/view_manager_client_factory.h"
#include "components/view_manager/public/cpp/view_manager_delegate.h"
#include "components/view_manager/public/cpp/view_observer.h"
#include "components/view_manager/public/interfaces/gpu.mojom.h"
#include "components/view_manager/public/interfaces/surface_id.mojom.h"
#include "components/view_manager/public/interfaces/surfaces.mojom.h"
#include "gpu/GLES2/gl2chromium.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "mojo/application/public/cpp/application_connection.h"
#include "mojo/application/public/cpp/application_delegate.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/application/public/cpp/application_runner.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/application/public/cpp/content_handler_factory.h"
#include "mojo/application/public/cpp/interface_factory_impl.h"
#include "mojo/application/public/cpp/service_provider_impl.h"
#include "mojo/application/public/interfaces/content_handler.mojom.h"
#include "mojo/application/public/interfaces/shell.mojom.h"
#include "mojo/common/data_pipe_utils.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_utils.h"
#include "mojo/public/c/gles2/gles2.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "third_party/pdfium/public/fpdf_ext.h"
#include "third_party/pdfium/public/fpdfview.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/mojo/events/input_events.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"
#include "ui/mojo/geometry/geometry.mojom.h"
#include "ui/mojo/geometry/geometry_util.h"
#include "v8/include/v8.h"

const uint32_t g_background_color = 0xFF888888;
const uint32_t g_transparent_color = 0x00000000;

namespace pdf_viewer {
namespace {

void LostContext(void*) {
  DCHECK(false);
}

// BitmapUploader is useful if you want to draw a bitmap or color in a View.
class BitmapUploader : public mojo::ResourceReturner {
 public:
  explicit BitmapUploader(mojo::View* view)
      : view_(view),
        color_(g_transparent_color),
        width_(0),
        height_(0),
        format_(BGRA),
        next_resource_id_(1u),
        id_namespace_(0u),
        local_id_(0u),
        returner_binding_(this) {
  }
  ~BitmapUploader() override {
    MojoGLES2DestroyContext(gles2_context_);
  }

  void Init(mojo::Shell* shell) {
    mojo::ServiceProviderPtr surfaces_service_provider;
    mojo::URLRequestPtr request(mojo::URLRequest::New());
    request->url = mojo::String::From("mojo:view_manager");
    shell->ConnectToApplication(request.Pass(),
                                mojo::GetProxy(&surfaces_service_provider),
                                nullptr, nullptr);
    ConnectToService(surfaces_service_provider.get(), &surface_);
    surface_->GetIdNamespace(
        base::Bind(&BitmapUploader::SetIdNamespace, base::Unretained(this)));
    mojo::ResourceReturnerPtr returner_ptr;
    returner_binding_.Bind(GetProxy(&returner_ptr));
    surface_->SetResourceReturner(returner_ptr.Pass());

    mojo::ServiceProviderPtr gpu_service_provider;
    mojo::URLRequestPtr request2(mojo::URLRequest::New());
    request2->url = mojo::String::From("mojo:view_manager");
    shell->ConnectToApplication(request2.Pass(),
                                mojo::GetProxy(&gpu_service_provider), nullptr,
                                nullptr);
    ConnectToService(gpu_service_provider.get(), &gpu_service_);

    mojo::CommandBufferPtr gles2_client;
    gpu_service_->CreateOffscreenGLES2Context(GetProxy(&gles2_client));
    gles2_context_ = MojoGLES2CreateContext(
        gles2_client.PassInterface().PassHandle().release().value(),
        &LostContext, NULL, mojo::Environment::GetDefaultAsyncWaiter());
    MojoGLES2MakeCurrent(gles2_context_);
  }

  // Sets the color which is RGBA.
  void SetColor(uint32_t color) {
    if (color_ == color)
      return;
    color_ = color;
    if (surface_)
      Upload();
  }

  enum Format {
    RGBA,  // Pixel layout on Android.
    BGRA,  // Pixel layout everywhere else.
  };

  // Sets a bitmap.
  void SetBitmap(int width,
                 int height,
                 scoped_ptr<std::vector<unsigned char>> data,
                 Format format) {
    width_ = width;
    height_ = height;
    bitmap_ = data.Pass();
    format_ = format;
    if (surface_)
      Upload();
  }

 private:
  void Upload() {
    mojo::Size size;
    size.width = view_->bounds().width;
    size.height = view_->bounds().height;
    if (!size.width || !size.height) {
      view_->SetSurfaceId(mojo::SurfaceId::New());
      return;
    }

    if (id_namespace_ == 0u)  // Can't generate a qualified ID yet.
      return;

    if (size != surface_size_) {
      if (local_id_ != 0u) {
        surface_->DestroySurface(local_id_);
      }
      local_id_++;
      surface_->CreateSurface(local_id_);
      surface_size_ = size;
      auto qualified_id = mojo::SurfaceId::New();
      qualified_id->id_namespace = id_namespace_;
      qualified_id->local = local_id_;
      view_->SetSurfaceId(qualified_id.Pass());
    }

    gfx::Rect bounds(size.width, size.height);
    mojo::PassPtr pass = mojo::CreateDefaultPass(1, bounds);
    mojo::CompositorFramePtr frame = mojo::CompositorFrame::New();
    frame->resources.resize(0u);

    pass->quads.resize(0u);
    pass->shared_quad_states.push_back(
        mojo::CreateDefaultSQS(size.To<gfx::Size>()));

    MojoGLES2MakeCurrent(gles2_context_);
    if (bitmap_.get()) {
      mojo::Size bitmap_size;
      bitmap_size.width = width_;
      bitmap_size.height = height_;
      GLuint texture_id = BindTextureForSize(bitmap_size);
      glTexSubImage2D(GL_TEXTURE_2D,
                      0,
                      0,
                      0,
                      bitmap_size.width,
                      bitmap_size.height,
                      TextureFormat(),
                      GL_UNSIGNED_BYTE,
                      &((*bitmap_)[0]));

      GLbyte mailbox[GL_MAILBOX_SIZE_CHROMIUM];
      glGenMailboxCHROMIUM(mailbox);
      glProduceTextureCHROMIUM(GL_TEXTURE_2D, mailbox);
      GLuint sync_point = glInsertSyncPointCHROMIUM();

      mojo::TransferableResourcePtr resource =
          mojo::TransferableResource::New();
      resource->id = next_resource_id_++;
      resource_to_texture_id_map_[resource->id] = texture_id;
      resource->format = mojo::RESOURCE_FORMAT_RGBA_8888;
      resource->filter = GL_LINEAR;
      resource->size = bitmap_size.Clone();
      mojo::MailboxHolderPtr mailbox_holder = mojo::MailboxHolder::New();
      mailbox_holder->mailbox = mojo::Mailbox::New();
      for (int i = 0; i < GL_MAILBOX_SIZE_CHROMIUM; ++i)
        mailbox_holder->mailbox->name.push_back(mailbox[i]);
      mailbox_holder->texture_target = GL_TEXTURE_2D;
      mailbox_holder->sync_point = sync_point;
      resource->mailbox_holder = mailbox_holder.Pass();
      resource->is_repeated = false;
      resource->is_software = false;

      mojo::QuadPtr quad = mojo::Quad::New();
      quad->material = mojo::MATERIAL_TEXTURE_CONTENT;

      mojo::RectPtr rect = mojo::Rect::New();
      if (width_ <= size.width && height_ <= size.height) {
        rect->width = width_;
        rect->height = height_;
      } else {
        // The source bitmap is larger than the viewport. Resize it while
        // maintaining the aspect ratio.
        float width_ratio = static_cast<float>(width_) / size.width;
        float height_ratio = static_cast<float>(height_) / size.height;
        if (width_ratio > height_ratio) {
          rect->width = size.width;
          rect->height = height_ / width_ratio;
        } else {
          rect->height = size.height;
          rect->width = width_ / height_ratio;
        }
      }
      quad->rect = rect.Clone();
      quad->opaque_rect = rect.Clone();
      quad->visible_rect = rect.Clone();
      quad->needs_blending = true;
      quad->shared_quad_state_index = 0u;

      mojo::TextureQuadStatePtr texture_state = mojo::TextureQuadState::New();
      texture_state->resource_id = resource->id;
      texture_state->premultiplied_alpha = true;
      texture_state->uv_top_left = mojo::PointF::New();
      texture_state->uv_bottom_right = mojo::PointF::New();
      texture_state->uv_bottom_right->x = 1.f;
      texture_state->uv_bottom_right->y = 1.f;
      texture_state->background_color = mojo::Color::New();
      texture_state->background_color->rgba = g_transparent_color;
      for (int i = 0; i < 4; ++i)
        texture_state->vertex_opacity.push_back(1.f);
      texture_state->y_flipped = false;

      frame->resources.push_back(resource.Pass());
      quad->texture_quad_state = texture_state.Pass();
      pass->quads.push_back(quad.Pass());
    }

    if (color_ != g_transparent_color) {
      mojo::QuadPtr quad = mojo::Quad::New();
      quad->material = mojo::MATERIAL_SOLID_COLOR;
      quad->rect = mojo::Rect::From(bounds);
      quad->opaque_rect = mojo::Rect::New();
      quad->visible_rect = mojo::Rect::From(bounds);
      quad->needs_blending = true;
      quad->shared_quad_state_index = 0u;

      mojo::SolidColorQuadStatePtr color_state =
          mojo::SolidColorQuadState::New();
      color_state->color = mojo::Color::New();
      color_state->color->rgba = color_;
      color_state->force_anti_aliasing_off = false;

      quad->solid_color_quad_state = color_state.Pass();
      pass->quads.push_back(quad.Pass());
    }

    frame->passes.push_back(pass.Pass());

    surface_->SubmitFrame(local_id_, frame.Pass(), mojo::Closure());
  }

  uint32_t BindTextureForSize(const mojo::Size size) {
    // TODO(jamesr): Recycle textures.
    GLuint texture = 0u;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 TextureFormat(),
                 size.width,
                 size.height,
                 0,
                 TextureFormat(),
                 GL_UNSIGNED_BYTE,
                 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    return texture;
  }

  uint32_t TextureFormat() {
    return format_ == BGRA ? GL_BGRA_EXT : GL_RGBA;
  }

  void SetIdNamespace(uint32_t id_namespace) {
    id_namespace_ = id_namespace;
    if (color_ != g_transparent_color || bitmap_.get())
      Upload();
  }

  // ResourceReturner implementation.
  void ReturnResources(
      mojo::Array<mojo::ReturnedResourcePtr> resources) override {
    MojoGLES2MakeCurrent(gles2_context_);
    // TODO(jamesr): Recycle.
    for (size_t i = 0; i < resources.size(); ++i) {
      mojo::ReturnedResourcePtr resource = resources[i].Pass();
      DCHECK_EQ(1, resource->count);
      glWaitSyncPointCHROMIUM(resource->sync_point);
      uint32_t texture_id = resource_to_texture_id_map_[resource->id];
      DCHECK_NE(0u, texture_id);
      resource_to_texture_id_map_.erase(resource->id);
      glDeleteTextures(1, &texture_id);
    }
  }

  mojo::View* view_;
  mojo::GpuPtr gpu_service_;
  MojoGLES2Context gles2_context_;

  mojo::Size size_;
  uint32_t color_;
  int width_;
  int height_;
  Format format_;
  scoped_ptr<std::vector<unsigned char>> bitmap_;
  mojo::SurfacePtr surface_;
  mojo::Size surface_size_;
  uint32_t next_resource_id_;
  uint32_t id_namespace_;
  uint32_t local_id_;
  base::hash_map<uint32_t, uint32_t> resource_to_texture_id_map_;
  mojo::Binding<mojo::ResourceReturner> returner_binding_;

  DISALLOW_COPY_AND_ASSIGN(BitmapUploader);
};

class EmbedderData {
 public:
  EmbedderData(mojo::Shell* shell, mojo::View* root) : bitmap_uploader_(root) {
    bitmap_uploader_.Init(shell);
    bitmap_uploader_.SetColor(g_background_color);
  }

  BitmapUploader& bitmap_uploader() { return bitmap_uploader_; }

 private:
  BitmapUploader bitmap_uploader_;

  DISALLOW_COPY_AND_ASSIGN(EmbedderData);
};

class PDFView : public mojo::ApplicationDelegate,
                public mojo::ViewManagerDelegate,
                public mojo::ViewObserver {
 public:
  PDFView(mojo::InterfaceRequest<mojo::Application> request,
          mojo::URLResponsePtr response)
      : app_(this, request.Pass(), base::Bind(&PDFView::OnTerminate,
                                              base::Unretained(this))),
        current_page_(0), page_count_(0), doc_(nullptr),
        view_manager_client_factory_(app_.shell(), this) {
    FetchPDF(response.Pass());
  }

  ~PDFView() override {
    if (doc_)
      FPDF_CloseDocument(doc_);
    for (auto& roots : embedder_for_roots_) {
      roots.first->RemoveObserver(this);
      delete roots.second;
    }
  }

 private:
  // Overridden from ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(&view_manager_client_factory_);
    return true;
  }

  // Overridden from ViewManagerDelegate:
  void OnEmbed(mojo::View* root) override {
    DCHECK(embedder_for_roots_.find(root) == embedder_for_roots_.end());
    root->AddObserver(this);
    EmbedderData* embedder_data = new EmbedderData(app_.shell(), root);
    embedder_for_roots_[root] = embedder_data;
    DrawBitmap(embedder_data);
  }

  void OnViewManagerDestroyed(mojo::ViewManager* view_manager) override {}

  // Overridden from ViewObserver:
  void OnViewBoundsChanged(mojo::View* view,
                           const mojo::Rect& old_bounds,
                           const mojo::Rect& new_bounds) override {
    DCHECK(embedder_for_roots_.find(view) != embedder_for_roots_.end());
    DrawBitmap(embedder_for_roots_[view]);
  }

  void OnViewInputEvent(mojo::View* view,
                        const mojo::EventPtr& event) override {
    DCHECK(embedder_for_roots_.find(view) != embedder_for_roots_.end());
    if (event->key_data &&
        (event->action != mojo::EVENT_TYPE_KEY_PRESSED ||
         event->key_data->is_char)) {
      return;
    }

    if ((event->key_data &&
         event->key_data->windows_key_code == mojo::KEYBOARD_CODE_DOWN) ||
        (event->pointer_data && event->pointer_data->vertical_wheel < 0)) {
      if (current_page_ < (page_count_ - 1)) {
        current_page_++;
        DrawBitmap(embedder_for_roots_[view]);
      }
    } else if ((event->key_data &&
                event->key_data->windows_key_code == mojo::KEYBOARD_CODE_UP) ||
               (event->pointer_data &&
                event->pointer_data->vertical_wheel > 0)) {
      if (current_page_ > 0) {
        current_page_--;
        DrawBitmap(embedder_for_roots_[view]);
      }
    }
  }

  void OnViewDestroyed(mojo::View* view) override {
    DCHECK(embedder_for_roots_.find(view) != embedder_for_roots_.end());
    const auto& it = embedder_for_roots_.find(view);
    DCHECK(it != embedder_for_roots_.end());
    delete it->second;
    embedder_for_roots_.erase(it);
    if (embedder_for_roots_.size() == 0)
      app_.Quit();
  }

  void DrawBitmap(EmbedderData* embedder_data) {
    if (!doc_)
      return;

    FPDF_PAGE page = FPDF_LoadPage(doc_, current_page_);
    int width = static_cast<int>(FPDF_GetPageWidth(page));
    int height = static_cast<int>(FPDF_GetPageHeight(page));

    scoped_ptr<std::vector<unsigned char>> bitmap;
    bitmap.reset(new std::vector<unsigned char>);
    bitmap->resize(width * height * 4);

    FPDF_BITMAP f_bitmap = FPDFBitmap_CreateEx(width, height, FPDFBitmap_BGRA,
                                               &(*bitmap)[0], width * 4);
    FPDFBitmap_FillRect(f_bitmap, 0, 0, width, height, 0xFFFFFFFF);
    FPDF_RenderPageBitmap(f_bitmap, page, 0, 0, width, height, 0, 0);
    FPDFBitmap_Destroy(f_bitmap);

    FPDF_ClosePage(page);

    embedder_data->bitmap_uploader().SetBitmap(width, height, bitmap.Pass(),
                                               BitmapUploader::BGRA);
  }

  void FetchPDF(mojo::URLResponsePtr response) {
    data_.clear();
    mojo::common::BlockingCopyToString(response->body.Pass(), &data_);
    if (data_.length() >= static_cast<size_t>(std::numeric_limits<int>::max()))
      return;
    doc_ = FPDF_LoadMemDocument(data_.data(), static_cast<int>(data_.length()),
                                nullptr);
    page_count_ = FPDF_GetPageCount(doc_);
  }

  // Callback from the quit closure. We key off this rather than
  // ApplicationDelegate::Quit() as we don't want to shut down the messageloop
  // when we quit (the messageloop is shared among multiple PDFViews).
  void OnTerminate() {
    delete this;
  }

  mojo::ApplicationImpl app_;
  std::string data_;
  int current_page_;
  int page_count_;
  FPDF_DOCUMENT doc_;
  std::map<mojo::View*, EmbedderData*> embedder_for_roots_;
  mojo::ViewManagerClientFactory view_manager_client_factory_;

  DISALLOW_COPY_AND_ASSIGN(PDFView);
};

class ContentHandlerImpl : public mojo::ContentHandler {
 public:
  ContentHandlerImpl(mojo::InterfaceRequest<ContentHandler> request)
      : binding_(this, request.Pass()) {}
  ~ContentHandlerImpl() override {}

 private:
  // Overridden from ContentHandler:
  void StartApplication(mojo::InterfaceRequest<mojo::Application> request,
                        mojo::URLResponsePtr response) override {
    new PDFView(request.Pass(), response.Pass());
  }

  mojo::StrongBinding<mojo::ContentHandler> binding_;

  DISALLOW_COPY_AND_ASSIGN(ContentHandlerImpl);
};

class PDFViewer : public mojo::ApplicationDelegate,
                  public mojo::InterfaceFactory<mojo::ContentHandler> {
 public:
  PDFViewer() {
    v8::V8::InitializeICU();
    FPDF_InitLibrary();
  }

  ~PDFViewer() override {
    FPDF_DestroyLibrary();
  }

 private:
  // Overridden from ApplicationDelegate:
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // Overridden from InterfaceFactory<ContentHandler>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::ContentHandler> request) override {
    new ContentHandlerImpl(request.Pass());
  }

  DISALLOW_COPY_AND_ASSIGN(PDFViewer);
};

}  // namespace
}  // namespace pdf_viewer

MojoResult MojoMain(MojoHandle application_request) {
  mojo::ApplicationRunner runner(new pdf_viewer::PDFViewer());
  return runner.Run(application_request);
}
