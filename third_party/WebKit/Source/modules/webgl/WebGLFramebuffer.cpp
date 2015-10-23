/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "modules/webgl/WebGLFramebuffer.h"

#include "modules/webgl/WebGLRenderbuffer.h"
#include "modules/webgl/WebGLRenderingContextBase.h"
#include "modules/webgl/WebGLTexture.h"
#include "platform/NotImplemented.h"

namespace blink {

namespace {

class WebGLRenderbufferAttachment final : public WebGLFramebuffer::WebGLAttachment {
public:
    static WebGLFramebuffer::WebGLAttachment* create(WebGLRenderbuffer*);

    DECLARE_VIRTUAL_TRACE();

private:
    explicit WebGLRenderbufferAttachment(WebGLRenderbuffer*);
    WebGLRenderbufferAttachment() { }

    GLsizei width() const override;
    GLsizei height() const override;
    GLenum format() const override;
    GLenum type() const override;
    WebGLSharedObject* object() const override;
    bool isSharedObject(WebGLSharedObject*) const override;
    bool valid() const override;
    void onDetached(WebGraphicsContext3D*) override;
    void attach(WebGraphicsContext3D*, GLenum target, GLenum attachment) override;
    void unattach(WebGraphicsContext3D*, GLenum target, GLenum attachment) override;

    Member<WebGLRenderbuffer> m_renderbuffer;
};

WebGLFramebuffer::WebGLAttachment* WebGLRenderbufferAttachment::create(WebGLRenderbuffer* renderbuffer)
{
    return new WebGLRenderbufferAttachment(renderbuffer);
}

DEFINE_TRACE(WebGLRenderbufferAttachment)
{
    visitor->trace(m_renderbuffer);
    WebGLFramebuffer::WebGLAttachment::trace(visitor);
}

WebGLRenderbufferAttachment::WebGLRenderbufferAttachment(WebGLRenderbuffer* renderbuffer)
    : m_renderbuffer(renderbuffer)
{
}

GLsizei WebGLRenderbufferAttachment::width() const
{
    return m_renderbuffer->width();
}

GLsizei WebGLRenderbufferAttachment::height() const
{
    return m_renderbuffer->height();
}

GLenum WebGLRenderbufferAttachment::format() const
{
    GLenum format = m_renderbuffer->internalFormat();
    if (format == GL_DEPTH_STENCIL_OES
        && m_renderbuffer->emulatedStencilBuffer()
        && m_renderbuffer->emulatedStencilBuffer()->internalFormat() != GL_STENCIL_INDEX8) {
        return 0;
    }
    return format;
}

WebGLSharedObject* WebGLRenderbufferAttachment::object() const
{
    return m_renderbuffer->object() ? m_renderbuffer.get() : 0;
}

bool WebGLRenderbufferAttachment::isSharedObject(WebGLSharedObject* object) const
{
    return object == m_renderbuffer;
}

bool WebGLRenderbufferAttachment::valid() const
{
    return m_renderbuffer->object();
}

void WebGLRenderbufferAttachment::onDetached(WebGraphicsContext3D* context)
{
    m_renderbuffer->onDetached(context);
}

void WebGLRenderbufferAttachment::attach(WebGraphicsContext3D* context, GLenum target, GLenum attachment)
{
    Platform3DObject object = objectOrZero(m_renderbuffer.get());
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT && m_renderbuffer->emulatedStencilBuffer()) {
        context->framebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, object);
        context->framebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, objectOrZero(m_renderbuffer->emulatedStencilBuffer()));
    } else {
        context->framebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, object);
    }
}

void WebGLRenderbufferAttachment::unattach(WebGraphicsContext3D* context, GLenum target, GLenum attachment)
{
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        context->framebufferRenderbuffer(target, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, 0);
        context->framebufferRenderbuffer(target, GL_STENCIL_ATTACHMENT, GL_RENDERBUFFER, 0);
    } else {
        context->framebufferRenderbuffer(target, attachment, GL_RENDERBUFFER, 0);
    }
}

GLenum WebGLRenderbufferAttachment::type() const
{
    notImplemented();
    return 0;
}

class WebGLTextureAttachment final : public WebGLFramebuffer::WebGLAttachment {
public:
    static WebGLFramebuffer::WebGLAttachment* create(WebGLTexture*, GLenum target, GLint level);

    DECLARE_VIRTUAL_TRACE();

private:
    WebGLTextureAttachment(WebGLTexture*, GLenum target, GLint level);
    WebGLTextureAttachment() { }

    GLsizei width() const override;
    GLsizei height() const override;
    GLenum format() const override;
    GLenum type() const override;
    WebGLSharedObject* object() const override;
    bool isSharedObject(WebGLSharedObject*) const override;
    bool valid() const override;
    void onDetached(WebGraphicsContext3D*) override;
    void attach(WebGraphicsContext3D*, GLenum target, GLenum attachment) override;
    void unattach(WebGraphicsContext3D*, GLenum target, GLenum attachment) override;

    Member<WebGLTexture> m_texture;
    GLenum m_target;
    GLint m_level;
};

WebGLFramebuffer::WebGLAttachment* WebGLTextureAttachment::create(WebGLTexture* texture, GLenum target, GLint level)
{
    return new WebGLTextureAttachment(texture, target, level);
}

DEFINE_TRACE(WebGLTextureAttachment)
{
    visitor->trace(m_texture);
    WebGLFramebuffer::WebGLAttachment::trace(visitor);
}

WebGLTextureAttachment::WebGLTextureAttachment(WebGLTexture* texture, GLenum target, GLint level)
    : m_texture(texture)
    , m_target(target)
    , m_level(level)
{
}

GLsizei WebGLTextureAttachment::width() const
{
    return m_texture->getWidth(m_target, m_level);
}

GLsizei WebGLTextureAttachment::height() const
{
    return m_texture->getHeight(m_target, m_level);
}

GLenum WebGLTextureAttachment::format() const
{
    return m_texture->getInternalFormat(m_target, m_level);
}

WebGLSharedObject* WebGLTextureAttachment::object() const
{
    return m_texture->object() ? m_texture.get() : 0;
}

bool WebGLTextureAttachment::isSharedObject(WebGLSharedObject* object) const
{
    return object == m_texture;
}

bool WebGLTextureAttachment::valid() const
{
    return m_texture->object();
}

void WebGLTextureAttachment::onDetached(WebGraphicsContext3D* context)
{
    m_texture->onDetached(context);
}

void WebGLTextureAttachment::attach(WebGraphicsContext3D* context, GLenum target, GLenum attachment)
{
    Platform3DObject object = objectOrZero(m_texture.get());
    context->framebufferTexture2D(target, attachment, m_target, object, m_level);
}

void WebGLTextureAttachment::unattach(WebGraphicsContext3D* context, GLenum target, GLenum attachment)
{
    if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
        context->framebufferTexture2D(target, GL_DEPTH_ATTACHMENT, m_target, 0, m_level);
        context->framebufferTexture2D(target, GL_STENCIL_ATTACHMENT, m_target, 0, m_level);
    } else {
        context->framebufferTexture2D(target, attachment, m_target, 0, m_level);
    }
}

GLenum WebGLTextureAttachment::type() const
{
    return m_texture->getType(m_target, m_level);
}

bool isColorRenderable(GLenum internalformat)
{
    switch (internalformat) {
    case GL_RGB:
    case GL_RGBA:
    case GL_SRGB_ALPHA_EXT:
    case GL_R8:
    case GL_R8UI:
    case GL_R8I:
    case GL_R16UI:
    case GL_R16I:
    case GL_R32UI:
    case GL_R32I:
    case GL_RG8:
    case GL_RG8UI:
    case GL_RG8I:
    case GL_RG16UI:
    case GL_RG16I:
    case GL_RG32UI:
    case GL_RG32I:
    case GL_RGB8:
    case GL_RGB565:
    case GL_RGBA8:
    case GL_SRGB8_ALPHA8:
    case GL_RGB5_A1:
    case GL_RGBA4:
    case GL_RGB10_A2:
    case GL_RGBA8UI:
    case GL_RGBA8I:
    case GL_RGB10_A2UI:
    case GL_RGBA16UI:
    case GL_RGBA16I:
    case GL_RGBA32UI:
    case GL_RGBA32I:
        return true;
    default:
        return false;
    }
}

bool isDepthRenderable(GLenum internalformat, bool includesDepthStencil)
{
    switch (internalformat) {
    case GL_DEPTH_COMPONENT:
    case GL_DEPTH_COMPONENT16:
    case GL_DEPTH_COMPONENT24:
    case GL_DEPTH_COMPONENT32F:
        return true;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
        return includesDepthStencil;
    default:
        return false;
    }
}

bool isStencilRenderable(GLenum internalformat, bool includesDepthStencil)
{
    switch (internalformat) {
    case GL_STENCIL_INDEX8:
        return true;
    case GL_DEPTH_STENCIL:
    case GL_DEPTH24_STENCIL8:
    case GL_DEPTH32F_STENCIL8:
        return includesDepthStencil;
    default:
        return false;
    }
}

} // anonymous namespace

WebGLFramebuffer::WebGLAttachment::WebGLAttachment()
{
}

WebGLFramebuffer::WebGLAttachment::~WebGLAttachment()
{
}

WebGLFramebuffer* WebGLFramebuffer::create(WebGLRenderingContextBase* ctx)
{
    return new WebGLFramebuffer(ctx);
}

WebGLFramebuffer::WebGLFramebuffer(WebGLRenderingContextBase* ctx)
    : WebGLContextObject(ctx)
    , m_object(ctx->webContext()->createFramebuffer())
    , m_destructionInProgress(false)
    , m_hasEverBeenBound(false)
    , m_readBuffer(GL_COLOR_ATTACHMENT0)
{
}

WebGLFramebuffer::~WebGLFramebuffer()
{
    // Attachments in |m_attachments| will be deleted from other
    // places, and we must not touch that map in deleteObjectImpl once
    // the destructor has been entered.
    m_destructionInProgress = true;

    // See the comment in WebGLObject::detachAndDeleteObject().
    detachAndDeleteObject();
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GLenum target, GLenum attachment, GLenum texTarget, WebGLTexture* texture, GLint level)
{
    ASSERT(isBound(target));
    removeAttachmentFromBoundFramebuffer(target, attachment);
    if (!m_object)
        return;
    if (texture && texture->object()) {
        m_attachments.add(attachment, WebGLTextureAttachment::create(texture, texTarget, level));
        drawBuffersIfNecessary(false);
        texture->onAttached();
    }
}

void WebGLFramebuffer::setAttachmentForBoundFramebuffer(GLenum target, GLenum attachment, WebGLRenderbuffer* renderbuffer)
{
    ASSERT(isBound(target));
    removeAttachmentFromBoundFramebuffer(target, attachment);
    if (!m_object)
        return;
    if (renderbuffer && renderbuffer->object()) {
        m_attachments.add(attachment, WebGLRenderbufferAttachment::create(renderbuffer));
        drawBuffersIfNecessary(false);
        renderbuffer->onAttached();
    }
}

void WebGLFramebuffer::attach(GLenum target, GLenum attachment, GLenum attachmentPoint)
{
    ASSERT(isBound(target));
    WebGLAttachment* attachmentObject = getAttachment(attachment);
    if (attachmentObject)
        attachmentObject->attach(context()->webContext(), target, attachmentPoint);
}

WebGLSharedObject* WebGLFramebuffer::getAttachmentObject(GLenum attachment) const
{
    if (!m_object)
        return nullptr;
    WebGLAttachment* attachmentObject = getAttachment(attachment);
    return attachmentObject ? attachmentObject->object() : nullptr;
}

bool WebGLFramebuffer::isAttachmentComplete(WebGLAttachment* attachedObject, GLenum attachment, const char** reason) const
{
    ASSERT(attachedObject && attachedObject->valid());
    ASSERT(reason);

    GLenum internalformat = attachedObject->format();

    switch (attachment) {
    case GL_DEPTH_ATTACHMENT:
        if (!isDepthRenderable(internalformat, context()->isWebGL2OrHigher())) {
            *reason = "the internalformat of the attached image is not depth-renderable";
            return false;
        }
        break;
    case GL_STENCIL_ATTACHMENT:
        if (!isStencilRenderable(internalformat, context()->isWebGL2OrHigher())) {
            *reason = "the internalformat of the attached image is not stencil-renderable";
            return false;
        }
        break;
    case GL_DEPTH_STENCIL_ATTACHMENT:
        ASSERT(!context()->isWebGL2OrHigher());
        if (internalformat != GL_DEPTH_STENCIL_OES) {
            *reason = "the internalformat of the attached image is not DEPTH_STENCIL";
            return false;
        }
        break;
    default:
        ASSERT(attachment == GL_COLOR_ATTACHMENT0 || (attachment > GL_COLOR_ATTACHMENT0 && attachment < static_cast<GLenum>(GL_COLOR_ATTACHMENT0 + context()->maxColorAttachments())));
        if (!isColorRenderable(internalformat)) {
            *reason = "the internalformat of the attached image is not color-renderable";
            return false;
        }
        break;
    }

    if (!attachedObject->width() || !attachedObject->height()) {
        *reason = "attachment has a 0 dimension";
        return false;
    }
    return true;
}

WebGLFramebuffer::WebGLAttachment* WebGLFramebuffer::getAttachment(GLenum attachment) const
{
    const AttachmentMap::const_iterator it = m_attachments.find(attachment);
    return (it != m_attachments.end()) ? it->value.get() : 0;
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(GLenum target, GLenum attachment)
{
    ASSERT(isBound(target));
    if (!m_object)
        return;

    WebGLAttachment* attachmentObject = getAttachment(attachment);
    if (attachmentObject) {
        attachmentObject->onDetached(context()->webContext());
        m_attachments.remove(attachment);
        drawBuffersIfNecessary(false);
        switch (attachment) {
        case GL_DEPTH_STENCIL_ATTACHMENT:
            attach(target, GL_DEPTH_ATTACHMENT, GL_DEPTH_ATTACHMENT);
            attach(target, GL_STENCIL_ATTACHMENT, GL_STENCIL_ATTACHMENT);
            break;
        case GL_DEPTH_ATTACHMENT:
            attach(target, GL_DEPTH_STENCIL_ATTACHMENT, GL_DEPTH_ATTACHMENT);
            break;
        case GL_STENCIL_ATTACHMENT:
            attach(target, GL_DEPTH_STENCIL_ATTACHMENT, GL_STENCIL_ATTACHMENT);
            break;
        }
    }
}

void WebGLFramebuffer::removeAttachmentFromBoundFramebuffer(GLenum target, WebGLSharedObject* attachment)
{
    ASSERT(isBound(target));
    if (!m_object)
        return;
    if (!attachment)
        return;

    bool checkMore = true;
    while (checkMore) {
        checkMore = false;
        for (const auto& it : m_attachments) {
            WebGLAttachment* attachmentObject = it.value.get();
            if (attachmentObject->isSharedObject(attachment)) {
                GLenum attachmentType = it.key;
                attachmentObject->unattach(context()->webContext(), target, attachmentType);
                removeAttachmentFromBoundFramebuffer(target, attachmentType);
                checkMore = true;
                break;
            }
        }
    }
}

GLenum WebGLFramebuffer::colorBufferFormat() const
{
    if (!m_object)
        return 0;
    WebGLAttachment* attachment = getAttachment(GL_COLOR_ATTACHMENT0);
    if (!attachment)
        return 0;
    return attachment->format();
}

GLenum WebGLFramebuffer::checkStatus(const char** reason) const
{
    unsigned count = 0;
    GLsizei width = 0, height = 0;
    WebGLAttachment* depthAttachment = nullptr;
    WebGLAttachment* stencilAttachment = nullptr;
    WebGLAttachment* depthStencilAttachment = nullptr;
    bool isWebGL2OrHigher = context()->isWebGL2OrHigher();
    for (const auto& it : m_attachments) {
        WebGLAttachment* attachment = it.value.get();
        if (!isAttachmentComplete(attachment, it.key, reason))
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        if (!attachment->valid()) {
            *reason = "attachment is not valid";
            return GL_FRAMEBUFFER_UNSUPPORTED;
        }
        if (!attachment->format()) {
            *reason = "attachment is an unsupported format";
            return GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
        }
        switch (it.key) {
        case GL_DEPTH_ATTACHMENT:
            depthAttachment = attachment;
            break;
        case GL_STENCIL_ATTACHMENT:
            stencilAttachment = attachment;
            break;
        case GL_DEPTH_STENCIL_ATTACHMENT:
            depthStencilAttachment = attachment;
            break;
        }
        if (!isWebGL2OrHigher) {
            if (!count) {
                width = attachment->width();
                height = attachment->height();
            } else {
                if (width != attachment->width() || height != attachment->height()) {
                    *reason = "attachments do not have the same dimensions";
                    return GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS;
                }
            }
        }
        ++count;
    }
    if (!count) {
        *reason = "no attachments";
        return GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
    }
    // WebGL 1 specific: no conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments.
    if (!isWebGL2OrHigher
        && ((depthStencilAttachment && (depthAttachment || stencilAttachment))
            || (depthAttachment && stencilAttachment))) {
        *reason = "conflicting DEPTH/STENCIL/DEPTH_STENCIL attachments";
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }
    if (isWebGL2OrHigher
        && (depthAttachment && stencilAttachment && depthAttachment->object() != stencilAttachment->object())) {
        *reason = "both DEPTH/STENCIL attachments are present and not the same image";
        return GL_FRAMEBUFFER_UNSUPPORTED;
    }
    return GL_FRAMEBUFFER_COMPLETE;
}

bool WebGLFramebuffer::onAccess(WebGraphicsContext3D* context3d, const char** reason)
{
    if (checkStatus(reason) != GL_FRAMEBUFFER_COMPLETE)
        return false;
    return true;
}

bool WebGLFramebuffer::hasStencilBuffer() const
{
    WebGLAttachment* attachment = getAttachment(GL_STENCIL_ATTACHMENT);
    if (!attachment)
        attachment = getAttachment(GL_DEPTH_STENCIL_ATTACHMENT);
    return attachment && attachment->valid();
}

void WebGLFramebuffer::deleteObjectImpl(WebGraphicsContext3D* context3d)
{
    // Both the AttachmentMap and its WebGLAttachment objects are GCed
    // objects and cannot be accessed after the destructor has been
    // entered, as they may have been finalized already during the
    // same GC sweep. These attachments' OpenGL objects will be fully
    // destroyed once their JavaScript wrappers are collected.
    if (!m_destructionInProgress) {
        for (const auto& attachment : m_attachments)
            attachment.value->onDetached(context3d);
    }

    context3d->deleteFramebuffer(m_object);
    m_object = 0;
}

bool WebGLFramebuffer::isBound(GLenum target) const
{
    return (context()->getFramebufferBinding(target) == this);
}

void WebGLFramebuffer::drawBuffers(const Vector<GLenum>& bufs)
{
    m_drawBuffers = bufs;
    m_filteredDrawBuffers.resize(m_drawBuffers.size());
    for (size_t i = 0; i < m_filteredDrawBuffers.size(); ++i)
        m_filteredDrawBuffers[i] = GL_NONE;
    drawBuffersIfNecessary(true);
}

void WebGLFramebuffer::drawBuffersIfNecessary(bool force)
{
    if (!context()->extensionEnabled(WebGLDrawBuffersName))
        return;
    bool reset = force;
    // This filtering works around graphics driver bugs on Mac OS X.
    for (size_t i = 0; i < m_drawBuffers.size(); ++i) {
        if (m_drawBuffers[i] != GL_NONE && getAttachment(m_drawBuffers[i])) {
            if (m_filteredDrawBuffers[i] != m_drawBuffers[i]) {
                m_filteredDrawBuffers[i] = m_drawBuffers[i];
                reset = true;
            }
        } else {
            if (m_filteredDrawBuffers[i] != GL_NONE) {
                m_filteredDrawBuffers[i] = GL_NONE;
                reset = true;
            }
        }
    }
    if (reset) {
        context()->webContext()->drawBuffersEXT(
            m_filteredDrawBuffers.size(), m_filteredDrawBuffers.data());
    }
}

GLenum WebGLFramebuffer::getDrawBuffer(GLenum drawBuffer)
{
    int index = static_cast<int>(drawBuffer - GL_DRAW_BUFFER0_EXT);
    ASSERT(index >= 0);
    if (index < static_cast<int>(m_drawBuffers.size()))
        return m_drawBuffers[index];
    if (drawBuffer == GL_DRAW_BUFFER0_EXT)
        return GL_COLOR_ATTACHMENT0;
    return GL_NONE;
}

bool WebGLFramebuffer::getReadBufferFormatAndType(GLenum* format, GLenum* type) const
{
    if (m_readBuffer == GL_NONE)
        return false;
    WebGLAttachment* image = getAttachment(m_readBuffer);
    if (!image)
        return false;
    if (format)
        *format = image->format();
    if (type)
        *type = image->type();
    return true;
}

DEFINE_TRACE(WebGLFramebuffer)
{
    visitor->trace(m_attachments);
    WebGLContextObject::trace(visitor);
}

}
