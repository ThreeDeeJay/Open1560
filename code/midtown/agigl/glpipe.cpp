/*
    Open1560 - An Open Source Re-Implementation of Midtown Madness 1 Beta
    Copyright (C) 2020 Brick

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "glpipe.h"

#include "agi/cmodel.h"
#include "agi/error.h"
#include "agi/rsys.h"
#include "agirend/bilight.h"
#include "agirend/bilmodel.h"
#include "agirend/lighter.h"
#include "agirend/rdlp.h"
#include "agirend/zbrender.h"
#include "data7/utimer.h"
#include "eventq7/winevent.h"
#include "mmcityinfo/state.h"
#include "pcwindis/dxinit.h"
#include "pcwindis/setupdata.h"

#include "glbitmap.h"
#include "glcontext.h"
#include "glerror.h"
#include "glrsys.h"
#include "gltexdef.h"
#include "glview.h"

#include <Windows.h>
#include <wingdi.h>

#include <glad/glad.h>

#include <wglext.h>

extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;

static const char* GetDebugTypeString(GLenum value)
{
    switch (value)
    {
        case GL_DEBUG_TYPE_ERROR: return "Error";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "Deprecated Behaviour";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "Undefined Behaviour";
        case GL_DEBUG_TYPE_PORTABILITY: return "Portability";
        case GL_DEBUG_TYPE_PERFORMANCE: return "Performance";
        case GL_DEBUG_TYPE_OTHER: return "Other";
        case GL_DEBUG_TYPE_MARKER: return "Marker";
        case GL_DEBUG_TYPE_PUSH_GROUP: return "Push Group";
        case GL_DEBUG_TYPE_POP_GROUP: return "Pop Group";

        default: return "Invalid";
    }
}

static i32 GetDebugSeverityPriority(GLenum value)
{
    switch (value)
    {
        case GL_DEBUG_SEVERITY_HIGH: return 2;
        case GL_DEBUG_SEVERITY_MEDIUM: return 1;
        case GL_DEBUG_SEVERITY_LOW: return 0;
        case GL_DEBUG_SEVERITY_NOTIFICATION: return 0;

        default: return 2;
    }
}

static i32 DebugMessageLevel = 0;

static void GLAPIENTRY DebugMessageCallback([[maybe_unused]] GLenum source, GLenum type, [[maybe_unused]] GLuint id,
    GLenum severity, [[maybe_unused]] GLsizei length, const GLchar* message, [[maybe_unused]] const void* userParam)
{
    i32 priority = GetDebugSeverityPriority(severity);

    if (priority < DebugMessageLevel)
        return;

    Printerf(priority, "GL Message: %X %s: %s", severity, GetDebugTypeString(type), message);
}

static mem::cmd_param PARAM_legacygl {"legacygl"};
static mem::cmd_param PARAM_gldebug {"gldebug"};
static mem::cmd_param PARAM_msaa {"msaa"};
static mem::cmd_param PARAM_scaling {"scaling"};
static mem::cmd_param PARAM_native_res {"nativeres"};
static mem::cmd_param PARAM_window_menu {"windowmenu"};
static mem::cmd_param PARAM_border {"border"};

agiGLPipeline::agiGLPipeline() = default;
agiGLPipeline::~agiGLPipeline() = default;

i32 agiGLPipeline::BeginGfx()
{
    if (gfx_started_)
        return AGI_ERROR_ALREADY_INITIALIZED;

    valid_bit_depths_ = 0x4;
    flags_ = 0x1 | 0x4 | 0x10;

    struct MonitorInfo
    {
        HMONITOR monitor;
        MONITORINFOEXA mi {{sizeof(mi)}};

        static BOOL CALLBACK MonitorCallback(
            HMONITOR hMonitor, HDC, [[maybe_unused]] LPRECT lprcMonitor, [[maybe_unused]] LPARAM lParam)
        {
            MonitorInfo& mi = *(MonitorInfo*) (lParam);

            if (!GetMonitorInfoA(hMonitor, &mi.mi))
                return TRUE;

            if (std::strcmp(GetRendererInfo().Device, mi.mi.szDevice))
                return TRUE;

            mi.monitor = hMonitor;

            return FALSE;
        }
    };

    MonitorInfo info {};
    EnumDisplayMonitors(NULL, NULL, MonitorInfo::MonitorCallback, (LPARAM) &info);

    HWND hwnd = static_cast<HWND>(window_);

    if (info.monitor == NULL)
    {
        Displayf("Failed to find monitor, using nearest");
        info.monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        GetMonitorInfoA(info.monitor, &info.mi);
    }

    i32 horz_res = info.mi.rcMonitor.right - info.mi.rcMonitor.left;
    i32 vert_res = info.mi.rcMonitor.bottom - info.mi.rcMonitor.top;

    if (dxiIsFullScreen() && !(PARAM_window_menu && MMSTATE.GameState == 0))
    {
        horz_res_ = horz_res;
        vert_res_ = vert_res;
    }
    else
    {
        horz_res_ = width_;
        vert_res_ = height_;
    }

    dxiWidth = horz_res_;
    dxiHeight = vert_res_;

    Displayf("Window Resolution: %u x %u", horz_res_, vert_res_);

    LONG window_style = 0;

    if (dxiIsFullScreen() || (width_ >= horz_res) || (height_ >= vert_res) || !PARAM_border.get_or(true))
    {
        window_style = IsDebuggerPresent() ? WS_OVERLAPPED : WS_POPUP;
    }
    else
    {
        window_style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU;
    }

    SetWindowLongA(hwnd, GWL_STYLE, window_style);

    RECT window_rect {0, 0, horz_res_, vert_res_};
    AdjustWindowRect(&window_rect, window_style, FALSE);

    i32 window_width = window_rect.right - window_rect.left;
    i32 window_height = window_rect.bottom - window_rect.top;

    SetWindowPos(hwnd, HWND_TOP, info.mi.rcMonitor.left + (horz_res - window_width) / 2,
        info.mi.rcMonitor.top + (vert_res - window_height) / 2, window_width, window_height,
        SWP_SHOWWINDOW | SWP_FRAMECHANGED | SWP_NOCOPYBITS);

    SetForegroundWindow(hwnd);

    window_dc_ = GetDC(hwnd);

    PIXELFORMATDESCRIPTOR pfd {sizeof(pfd)};

    pfd.nVersion = 1;
    pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int format = ChoosePixelFormat(window_dc_, &pfd);

    SetPixelFormat(window_dc_, format, &pfd);

    HGLRC gl_context = wglCreateContext(window_dc_);

    if (gl_context == NULL)
        Quitf("Failed to create legacy OpenGL context: 0x%08X", GetLastError());

    gl_context_ = MakeUnique<agiGLContext>(window_dc_, gl_context);

    DebugMessageLevel = PARAM_gldebug.get_or(
#ifdef ARTS_DEBUG
        1
#else
        -1
#endif
    );

    if (gl_context_->HasExtension("WGL_ARB_create_context"))
    {
        Displayf("Creating modern OpenGL context");

        if (gl_context_->HasExtension("WGL_ARB_pixel_format"))
        {
            const int pixel_attribs[] {
                // clang-format off
                WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
                WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
                WGL_DOUBLE_BUFFER_ARB, GL_TRUE,
                WGL_PIXEL_TYPE_ARB, WGL_TYPE_RGBA_ARB,
                WGL_COLOR_BITS_ARB, 32,
                WGL_DEPTH_BITS_ARB, 24,
                0,
                // clang-format on
            };

            if (UINT num_formats = 0;
                wglChoosePixelFormatARB(window_dc_, pixel_attribs, NULL, 1, &format, &num_formats) && num_formats)
            {
                DescribePixelFormat(window_dc_, format, sizeof(pfd), &pfd);
                SetPixelFormat(window_dc_, format, &pfd);
            }
            else
            {
                Errorf("Failed to choose pixel format");
            }
        }

        int attribs[11];
        int num_attribs = 0;

        attribs[num_attribs++] = WGL_CONTEXT_MAJOR_VERSION_ARB;
        attribs[num_attribs++] = 1;

        attribs[num_attribs++] = WGL_CONTEXT_MINOR_VERSION_ARB;
        attribs[num_attribs++] = 0;

        bool legacy_gl = PARAM_legacygl.get_or(false);

        // wglCreateContext contexts are required to support legacy functionality
        // If the legacy context is compatibility 3.2+, it probably supports core and compatibility 3.2+
        // Otherwise, if WGL_ARB_create_context_profile is supported, it probably only supports core 3.2+
        if (gl_context_->HasExtension("WGL_ARB_create_context_profile") &&
            gl_context_->HasVersion(legacy_gl ? 320 : 310))
        {
            attribs[1] = 3;
            attribs[3] = 2;

            int profile_mask = legacy_gl ? WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB : WGL_CONTEXT_CORE_PROFILE_BIT_ARB;

            attribs[num_attribs++] = WGL_CONTEXT_PROFILE_MASK_ARB;
            attribs[num_attribs++] = profile_mask;
        }
        else if (gl_context_->HasVersion(310))
        {
            attribs[1] = 3;
            attribs[3] = 1;
        }

        {
            int context_flags = 0;

            if (DebugMessageLevel > -1)
                context_flags |= WGL_CONTEXT_DEBUG_BIT_ARB;

#if 0
            context_flags |= WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#endif

            attribs[num_attribs++] = WGL_CONTEXT_FLAGS_ARB;
            attribs[num_attribs++] = context_flags;
        }

        if ((DebugMessageLevel < 0) && gl_context_->HasExtension("WGL_ARB_create_context_no_error"))
        {
            attribs[num_attribs++] = WGL_CONTEXT_OPENGL_NO_ERROR_ARB;
            attribs[num_attribs++] = 1;
        }

        attribs[num_attribs++] = 0;

        HGLRC modern_gl_context = NULL;

        while ((modern_gl_context = wglCreateContextAttribsARB(window_dc_, 0, attribs)) == NULL)
        {
            if (attribs[1] == 3)
            {
                if (attribs[3] > 0)
                    --attribs[3];
                else
                    attribs[1] = 1;
            }
            else
            {
                break;
            }
        }

        if (modern_gl_context)
        {
            gl_context_ = MakeUnique<agiGLContext>(window_dc_, modern_gl_context);
        }
        else
        {
            Errorf("Failed to create modern OpenGL context: 0x%08X", GetLastError());
        }
    }

    if (gladLoadGLLoader([](const char* name) -> void* { return agiGL->GetProc(name); }) != 1)
        Quitf("Failed to load GLAD");

    gl_context_->Init();

    Displayf("OpenGL Vendor: %s", glGetString(GL_VENDOR));
    Displayf("OpenGL Renderer: %s", glGetString(GL_RENDERER));

    if ((DebugMessageLevel > -1) && gl_context_->HasExtension("GL_KHR_debug"))
    {
        Displayf("Using glDebugMessageCallback");

        glEnable(GL_DEBUG_OUTPUT);
#ifndef ARTS_FINAL
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
#endif
        glDebugMessageCallback(DebugMessageCallback, 0);
    }

    if (DebugMessageLevel > 1)
    {
        Displayf("*** Begin OpenGL Extensions ***");
        for (HashIterator i(gl_context_->GetExtensions()); i.Next();)
            Displayf("%s", i->Key.get());
        Displayf("*** End OpenGL Extensions ***");
    }

    if (gl_context_->HasExtension("WGL_EXT_swap_control"))
    {
        int interval = 0;

        if (device_flags_1_ & 0x1)
            interval = gl_context_->HasExtension("WGL_EXT_swap_control_tear") ? -1 : 1;

        Displayf("wglSwapIntervalEXT(%i)", interval);

        if (!wglSwapIntervalEXT(interval))
            Errorf("wglSwapIntervalEXT failed: %08X", GetLastError());
    }

    // FIXME: Check pixel format masks
    screen_format_ = {sizeof(screen_format_)};

    // TODO: Should this have alpha?
    screen_format_.Flags = AGISD_PIXELFORMAT;
    screen_format_.PixelFormat = PixelFormat_A8R8G8B8;

    alpha_color_model_ = AsRc(agiColorModel::FindMatch(&screen_format_));
    opaque_color_model_ = alpha_color_model_;
    hi_color_model_ = alpha_color_model_;
    text_color_model_ = alpha_color_model_;

    agiCurState.SetCullMode(agiCullMode::None);
    agiCurState.SetBlendSet(agiBlendSet::SrcAlpha_InvSrcAlpha);
    agiCurState.SetTexturePerspective(true);
    agiCurState.SetMaxTextures(1);

    rasterizer_ = MakeRc<agiGLRasterizer>(this);
    renderer_ = MakeRc<agiZBufRenderer>(rasterizer_.get());

    switch (i32 scaling_mode = PARAM_scaling.get_or(0))
    {
        case 0: // Stretched, Keep Aspect
        case 1: // Stretched
        {
            blit_width_ = horz_res_;
            blit_height_ = vert_res_;

            if (scaling_mode == 0)
            {
                f32 game_aspect = static_cast<f32>(width_) / static_cast<f32>(height_);
                f32 draw_aspect = static_cast<f32>(blit_width_) / static_cast<f32>(blit_height_);

                if (draw_aspect > game_aspect)
                    blit_width_ = static_cast<i32>(blit_width_ * (game_aspect / draw_aspect));
                else if (draw_aspect < game_aspect)
                    blit_height_ = static_cast<i32>(blit_height_ * (draw_aspect / game_aspect));
            }

            break;
        }

        case 2: // Centered
        {
            blit_width_ = width_;
            blit_height_ = height_;

            break;
        }
    }

    blit_x_ = (horz_res_ - blit_width_) / 2;
    blit_y_ = (vert_res_ - blit_height_) / 2;

    bool builtin_fb = !gl_context_->HasExtension("GL_ARB_framebuffer_object");

    i32 msaa_level = 0;

    if (!builtin_fb && gl_context_->HasExtension("GL_ARB_texture_multisample"))
    {
        GLint max_samples = 0;
        glGetIntegerv(GL_MAX_SAMPLES, &max_samples);
        msaa_level = std::clamp(PARAM_msaa.get_or<i32>(0), 0, max_samples);
    }

    bool native_res = builtin_fb || PARAM_native_res.get_or(true);

    if (msaa_level != 0 && !gl_context_->HasExtension("GL_EXT_framebuffer_multisample_blit_scaled"))
    {
        Errorf("Multisample scaling not supported");

        native_res = true;
    }

    if (native_res)
    {
        render_width_ = blit_width_;
        render_height_ = blit_height_;
    }
    else
    {
        render_width_ = width_;
        render_height_ = height_;
    }

    // Don't bother using a custom framebuffer if there would be no difference
    if (render_width_ == blit_width_ && render_height_ == blit_height_ && msaa_level == 0)
        builtin_fb = true;

    render_x_ = 0;
    render_y_ = 0;

    PrintGlErrors();

    Displayf("Using %s framebuffer (msaa=%i)", builtin_fb ? "builtin" : "custom", msaa_level);

    // Clear the builtin frame buffer (avoid ugly remains/ghost image)
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
    // SwapBuffers(window_dc_);

    if (!builtin_fb)
    {
        glGenFramebuffers(1, &fbo_);
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
        glGenRenderbuffers(2, rbo_);

        if (render_width_ > blit_width_ || render_height_ > blit_height_)
            blit_filter_ = GL_LINEAR;
        else
            blit_filter_ = GL_NEAREST;

        if (msaa_level)
        {
            glBindRenderbuffer(GL_RENDERBUFFER, rbo_[0]);
            glRenderbufferStorageMultisample(GL_RENDERBUFFER, msaa_level, GL_RGBA8, render_width_, render_height_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_[0]);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo_[1]);
            glRenderbufferStorageMultisample(
                GL_RENDERBUFFER, msaa_level, GL_DEPTH_COMPONENT, render_width_, render_height_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_[1]);

            if (render_width_ != blit_width_ || render_height_ != blit_height_)
                blit_filter_ = GL_SCALED_RESOLVE_NICEST_EXT;
        }
        else
        {
            glBindRenderbuffer(GL_RENDERBUFFER, rbo_[0]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, render_width_, render_height_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, rbo_[0]);

            glBindRenderbuffer(GL_RENDERBUFFER, rbo_[1]);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, render_width_, render_height_);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo_[1]);
        }

        if (GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER); status != GL_FRAMEBUFFER_COMPLETE)
            Quitf("Failed to create framebuffer: 0x%08X", status);

        PrintGlErrors();
    }
    else
    {
        std::swap(render_x_, blit_x_);
        std::swap(render_y_, blit_y_);
    }

    glViewport(render_x_, render_y_, render_width_, render_height_);

    gfx_started_ = true;

    return AGI_ERROR_SUCCESS;
}

void agiGLPipeline::EndGfx()
{
    if (fbo_ != 0)
    {
        glDeleteFramebuffers(1, &fbo_);
        glDeleteRenderbuffers(2, rbo_);

        fbo_ = 0;
        rbo_[0] = 0;
        rbo_[1] = 0;
    }

    gl_context_ = nullptr;

    ReleaseDC(static_cast<HWND>(window_), window_dc_);
    window_dc_ = NULL;

    text_color_model_ = nullptr;
    hi_color_model_ = nullptr;
    opaque_color_model_ = nullptr;
    alpha_color_model_ = nullptr;

    renderer_ = nullptr;
    rasterizer_ = nullptr;

    gfx_started_ = false;
}

static mem::cmd_param PARAM_frameclear {"frameclear"};

void agiGLPipeline::BeginFrame()
{
    ARTS_UTIMED(agiBeginFrame);

    agiPipeline::BeginFrame();

    gl_context_->MakeCurrent();

    if (PARAM_frameclear.get_or(true))
    {
        agiGL->EnableDisable(GL_SCISSOR_TEST, false);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    if (fbo_ != 0)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    }
}

void agiGLPipeline::BeginScene()
{
    ARTS_UTIMED(agiBeginScene);

    UpdateZTrick();

    agiPipeline::BeginScene();
    agiLighter::BeginScene();

    in_scene_ = true;

    renderer_->BeginGroup();
}

void agiGLPipeline::EndScene()
{
    ARTS_UTIMED(agiEndScene);

    rasterizer_->EndGroup();
    in_scene_ = false;

    agiPipeline::EndScene();
}

void agiGLPipeline::EndFrame()
{
    ARTS_UTIMED(agiEndFrame);

    if (fbo_ != 0)
    {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo_);
        glBlitFramebuffer(render_x_, render_y_, render_x_ + render_width_, render_y_ + render_height_, blit_x_, blit_y_,
            blit_x_ + blit_width_, blit_y_ + blit_height_, GL_COLOR_BUFFER_BIT, blit_filter_);
    }

    SwapBuffers(window_dc_);

    if (!dxiDoubleBuffer())
        glFinish();

    PrintGlErrors();

    agiPipeline::EndFrame();
}

RcOwner<class agiTexDef> agiGLPipeline::CreateTexDef()
{
    return AsOwner(MakeRc<agiGLTexDef>(this));
}

RcOwner<class agiTexLut> agiGLPipeline::CreateTexLut()
{
    return nullptr;
}

RcOwner<class DLP> agiGLPipeline::CreateDLP()
{
    return AsOwner(MakeRc<RDLP>(this));
}

RcOwner<agiLight> agiGLPipeline::CreateLight()
{
    return AsOwner(MakeRc<agiBILight>(this));
}

RcOwner<agiLightModel> agiGLPipeline::CreateLightModel()
{
    return AsOwner(MakeRc<agiBILightModel>(this));
}

RcOwner<class agiViewport> agiGLPipeline::CreateViewport()
{
    return AsOwner(MakeRc<agiGLViewport>(this));
}

RcOwner<class agiBitmap> agiGLPipeline::CreateBitmap()
{
    return AsOwner(MakeRc<agiGLBitmap>(this));
}

void agiGLPipeline::CopyBitmap(i32 dst_x, i32 dst_y, agiBitmap* src, i32 src_x, i32 src_y, i32 width, i32 height)
{
    if (!IsAppActive())
        return;

    // FIXME: https://github.com/0x1F9F1/Open1560/issues/22
    if (src_y + height > src->GetHeight())
        return;

    ++agiBitmapCount;
    agiBitmapPixels += width * height;

    // FIXME: This overlaps with the rasterizer timer
    // ARTS_UTIMED(agiCopyBitmap);

    // rasterizer_->BeginGroup();

    agiTexDef* texture = static_cast<agiGLBitmap*>(src)->GetHandle();

    bool debug_draw = agiCurState.GetDrawMode() == 0x3;

    auto old_tex = agiCurState.SetTexture(debug_draw ? nullptr : texture);
    auto old_draw_mode = agiCurState.SetDrawMode(0xF);
    auto old_depth = agiCurState.SetZEnable(false);
    auto old_zwrite = agiCurState.SetZWrite(false);
    auto old_alpha = agiCurState.SetAlphaEnable(debug_draw ? true : false);
    auto old_filter = agiCurState.SetTexFilter(agiTexFilter::Point);
    auto old_fog_mode = agiCurState.SetFogMode(agiFogMode::None);
    auto old_fog_color = agiCurState.SetFogColor(0x00000000);
    auto old_blend_set = agiCurState.SetBlendSet(debug_draw ? agiBlendSet::One_One : agiBlendSet::SrcAlpha_InvSrcAlpha);

    agiScreenVtx blank {0.0f, 0.0f, 0.0f, 1.0f, debug_draw ? 0xFF000044 : 0xFFFFFFFF, 0xFFFFFFFF, 0.0f, 0.0f};
    agiScreenVtx verts[4] {blank, blank, blank, blank};
    u16 indices[6] {0, 1, 2, 0, 2, 3};

    f32 const inv_tex_w = 1.0f / src->GetWidth();
    f32 const inv_tex_h = 1.0f / src->GetHeight();

    verts[3].x = verts[0].x = static_cast<f32>(dst_x);
    verts[1].y = verts[0].y = static_cast<f32>(dst_y);
    verts[3].tu = verts[0].tu = src_x * inv_tex_w;
    verts[1].tv = verts[0].tv = src_y * inv_tex_h;

    verts[1].x = verts[2].x = static_cast<f32>(dst_x + width);
    verts[3].y = verts[2].y = static_cast<f32>(dst_y + height);
    verts[1].tu = verts[2].tu = (src_x + width) * inv_tex_w;
    verts[3].tv = verts[2].tv = (src_y + height) * inv_tex_h;

    rasterizer_->Mesh(agiVtxType::Screen, (agiVtx*) verts, 4, indices, 6);

    // rasterizer_->EndGroup();

    agiCurState.SetTexture(old_tex);
    agiCurState.SetDrawMode(old_draw_mode);
    agiCurState.SetZEnable(old_depth);
    agiCurState.SetZWrite(old_zwrite);
    agiCurState.SetAlphaEnable(old_alpha);
    agiCurState.SetTexFilter(old_filter);
    agiCurState.SetFogMode(old_fog_mode);
    agiCurState.SetFogColor(old_fog_color);
    agiCurState.SetBlendSet(old_blend_set);
}

void agiGLPipeline::ClearAll(i32 color)
{
    glClearColor((color & 0xFF) / 255.0f, ((color >> 8) & 0xFF) / 255.0f, ((color >> 16) & 0xFF) / 255.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void agiGLPipeline::ClearRect(i32 x, i32 y, i32 width, i32 height, u32 color)
{
    // rasterizer_->BeginGroup();

    auto tex = agiCurState.SetTexture(nullptr);
    auto draw_mode = agiCurState.SetDrawMode(0xF);
    auto depth = agiCurState.SetZEnable(false);
    auto zwrite = agiCurState.SetZWrite(false);
    auto alpha = agiCurState.SetAlphaEnable(false);
    auto filter = agiCurState.SetTexFilter(agiTexFilter::Point);
    auto fog_mode = agiCurState.SetFogMode(agiFogMode::None);
    auto fog_color = agiCurState.SetFogColor(0x00000000);

    agiScreenVtx blank {0.0f, 0.0f, 0.0f, 1.0f, color | 0xFF000000, 0xFFFFFFFF, 0.0f, 0.0f};
    agiScreenVtx verts[4] {blank, blank, blank, blank};
    u16 indices[6] {0, 1, 2, 0, 2, 3};

    verts[3].x = verts[0].x = static_cast<f32>(x);
    verts[1].y = verts[0].y = static_cast<f32>(y);

    verts[1].x = verts[2].x = static_cast<f32>(x + width);
    verts[3].y = verts[2].y = static_cast<f32>(y + height);

    rasterizer_->Mesh(agiVtxType::Screen, (agiVtx*) verts, 4, indices, 6);

    // rasterizer_->EndGroup();

    agiCurState.SetTexture(tex);
    agiCurState.SetDrawMode(draw_mode);
    agiCurState.SetZEnable(depth);
    agiCurState.SetZWrite(zwrite);
    agiCurState.SetAlphaEnable(alpha);
    agiCurState.SetTexFilter(filter);
    agiCurState.SetFogMode(fog_mode);
    agiCurState.SetFogColor(fog_color);
}

static mem::cmd_param PARAM_width {"width"};
static mem::cmd_param PARAM_height {"height"};
static mem::cmd_param PARAM_depth {"depth"};
static mem::cmd_param PARAM_vsync {"vsync"};
static mem::cmd_param PARAM_pack {"pack"};
static mem::cmd_param PARAM_integrated {"integrated"};
static mem::cmd_param PARAM_annotate {"annotate"};

void agiGLPipeline::Init()
{
    // TODO: Properly use width/height/depth
    width_ = PARAM_width.get_or<i32>(640);
    height_ = PARAM_height.get_or<i32>(480);
    bit_depth_ = PARAM_depth.get_or<i32>(32);

    device_flags_1_ = 0x1032; // hal, zbuffer, vram

    if (PARAM_vsync.get_or(true))
        device_flags_1_ |= 0x1;

    device_flags_2_ = device_flags_1_;
    device_flags_3_ = device_flags_1_;

    PackShift = PARAM_pack.get_or<i32>(0);
    AnnotateTextures = PARAM_annotate.get_or(false);

    bool use_gpu = !PARAM_integrated.get_or(false);
    HINSTANCE hInstance = GetModuleHandle(NULL);

    if (void* AmdPowerXpressRequestHighPerformance = GetProcAddress(hInstance, "AmdPowerXpressRequestHighPerformance"))
        *static_cast<DWORD*>(AmdPowerXpressRequestHighPerformance) = use_gpu;

    // NOTE: NVIDIA seems to only care about NvOptimusEnablement being present, not the value, despite what their documentation says
    if (void* NvOptimusEnablement = GetProcAddress(hInstance, "NvOptimusEnablement"))
        *static_cast<DWORD*>(NvOptimusEnablement) = use_gpu;
}

Ptr<u8[]> glScreenShot(i32& width, i32& height)
{
    if (wglGetCurrentContext() == NULL)
        return nullptr;

    GLint viewport[4] {};
    glGetIntegerv(GL_VIEWPORT, viewport);

    i32 x = viewport[0];
    i32 y = viewport[1];

    width = viewport[2];
    height = viewport[3];

    Ptr<u8[]> buffer = MakeUniqueUninit<u8[]>(width * height * 3);

    if (buffer)
    {
        glPixelStorei(GL_PACK_ALIGNMENT, 1);

        // FIXME: Does not work with MSAA (FBO anti-alias method is not valid for read pixels.)
        glReadPixels(x, y, width, height, GL_BGR, GL_UNSIGNED_BYTE, buffer.get());
    }

    PrintGlErrors();

    return buffer;
}

Owner<agiPipeline> glCreatePipeline([[maybe_unused]] i32 argc, [[maybe_unused]] char** argv)
{
    Ptr<agiGLPipeline> result = MakeUnique<agiGLPipeline>();
    result->Init();
    return AsOwner(result);
}
