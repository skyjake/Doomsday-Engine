/** @file oculusrift.cpp
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright (c) 2013 Christopher Bruns <cmbruns@rotatingpenguin.com>
 *
 * @par License
 * LGPL: http://www.gnu.org/licenses/lgpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser
 * General Public License for more details. You should have received a copy of
 * the GNU Lesser General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small> 
 */

#include "de/OculusRift"
#include "de/BaseWindow"
#include "de/VRWindowTransform"

#include <de/GLFramebuffer>
#include <de/GLState>
#include <de/Lockable>
#include <de/Guard>
#include <de/App>
#include <de/Log>

#ifdef DENG_HAVE_OCULUS_API
#  include <OVR.h>
#  include <OVR_CAPI_GL.h>
using namespace OVR;
#endif

namespace de {

#ifdef DENG_HAVE_OCULUS_API
Vector3f quaternionToPRYAngles(Quatf const &q)
{
    Vector3f pry;
    q.GetEulerAngles<Axis_Y, Axis_X, Axis_Z>(&pry.z, &pry.x, &pry.y);
    return pry;
}
#endif

DENG2_PIMPL(OculusRift)
, DENG2_OBSERVES(Canvas, KeyEvent)
#ifdef DENG_HAVE_OCULUS_API
, DENG2_OBSERVES(Variable, Change)
#endif
, public Lockable
{
#ifdef DENG_HAVE_OCULUS_API
    ovrHmd hmd;
    ovrEyeType currentEye;
    ovrPosef headPose[2];
    ovrEyeRenderDesc render[2];
    ovrTexture textures[2];
    ovrFovPort fov[2];
    float fovXDegrees;
    ovrFrameTiming timing;
#endif
    Matrix4f eyeMatrix[2];
    Vector3f pitchRollYaw;
    Vector3f headPosition;
    Vector3f eyeOffset[2];
    float aspect = 1.f;

    BaseWindow *window = nullptr;
    QRect oldGeometry;

    bool inited = false;
    bool frameOngoing = false;
    bool needPoseUpdate = false;
    bool densityChanged = false;
    //Vector2f screenSize;
    //float lensSeparationDistance;
    //Vector4f hmdWarpParam;
    //Vector4f chromAbParam;
    float eyeToScreenDistance;
    //float latency;
    //float ipd;
    float yawOffset;

    Instance(Public *i)
        : Base(i)
        //, screenSize(0.14976f, 0.09360f)
        //, lensSeparationDistance(0.0635f)
        //, hmdWarpParam(1.0f, 0.220f, 0.240f, 0.000f)
        //, chromAbParam(0.996f, -0.004f, 1.014f, 0.0f)
        , eyeToScreenDistance(0.041f)
        //, latency(.030f)
        //,ipd(.064f)
        , yawOffset(0)
    {
#ifdef DENG_HAVE_OCULUS_API
        hmd = 0;
        /*
        ovr_Initialize();
        hmd = ovrHmd_Create(0);

        if(!hmd && App::commandLine().has("-ovrdebug"))
        {
            hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
        }

        if(hmd)
        {
            LOG_INPUT_NOTE("HMD: %s (%s) %i.%i %ix%i pixels")
                    << String(hmd->ProductName) << String(hmd->Manufacturer)
                    << hmd->FirmwareMajor << hmd->FirmwareMinor
                    << hmd->Resolution.w << hmd->Resolution.h;
        }
        */
#endif
    }

    ~Instance()
    {
        DENG2_GUARD(this);
        deinit();

#ifdef DENG_HAVE_OCULUS_API
        /*
        if(hmd)
        {
            ovrHmd_Destroy(hmd);
        }*/
        ovr_Shutdown();
#endif
    }

#ifdef DENG_HAVE_OCULUS_API
    /// Returns the offscreen framebuffer where the Oculus Rift raw frame is drawn.
    /// This is passed to LibOVR as a texture.
    GLFramebuffer &framebuffer()
    {
        DENG2_ASSERT(window);
        return window->transform().as<VRWindowTransform>().unwarpedFramebuffer();
    }

    void variableValueChanged(Variable &, Value const &)
    {
        densityChanged = true;
    }

    void resizeFramebuffer()
    {
        Sizei size[2];
        ovrFovPort fovMax;
        float density = App::config().getf("vr.oculusRift.pixelDensity", 1.f);
        for(int eye = 0; eye < 2; ++eye)
        {
            // Use the default FOV.
            fov[eye] = hmd->DefaultEyeFov[eye];

            size[eye] = ovrHmd_GetFovTextureSize(hmd, ovrEyeType(eye), fov[eye], density);
        }

        fovMax.LeftTan  = max(fov[0].LeftTan,  fov[1].LeftTan);
        fovMax.RightTan = max(fov[0].RightTan, fov[1].RightTan);
        fovMax.UpTan    = max(fov[0].UpTan,    fov[1].UpTan);
        fovMax.DownTan  = max(fov[0].DownTan,  fov[1].DownTan);

        float comboXTan = max(fovMax.LeftTan, fovMax.RightTan);
        float comboYTan = max(fovMax.UpTan,   fovMax.DownTan);

        aspect = comboXTan / comboYTan;

        LOGDEV_GL_MSG("Aspect ratio: %f") << aspect;

        // Calculate the horizontal total FOV in degrees that the renderer will use
        // for clipping.
        fovXDegrees = radianToDegree(2 * atanf(comboXTan));

        LOGDEV_GL_MSG("Clip FOV: %.2f degrees") << fovXDegrees;

        framebuffer().resize(GLFramebuffer::Size(size[0].w + size[1].w,
                                                 max(size[0].h, size[1].h)));
        uint const w = framebuffer().size().x;
        uint const h = framebuffer().size().y;

        framebuffer().colorTexture().setFilter(gl::Linear, gl::Linear, gl::MipNone);
        framebuffer().colorTexture().glApplyParameters();

        LOG_GL_VERBOSE("Framebuffer size: ") << framebuffer().size().asText();

        for(int eye = 0; eye < 2; ++eye)
        {
            ovrGLTexture tex;
            zap(tex);

            tex.OGL.Header.API            = ovrRenderAPI_OpenGL;
            tex.OGL.Header.TextureSize    = Sizei(w, h);
            tex.OGL.Header.RenderViewport = Recti(eye == 0? 0 : ((w + 1) / 2), 0, w/2, h);
            tex.OGL.TexId                 = framebuffer().colorTexture().glName();

            textures[eye] = tex.Texture;
        }
    }
#endif

    void init()
    {
        if(inited) return;
        inited = true;

#ifdef DENG_HAVE_OCULUS_API
        //ovr_Initialize();
        hmd = ovrHmd_Create(0);

        if(!hmd && App::commandLine().has("-ovrdebug"))
        {
            hmd = ovrHmd_CreateDebug(ovrHmd_DK2);
        }

        if(hmd)
        {
            LOG_INPUT_NOTE("HMD: %s (%s) %i.%i %ix%i pixels")
                    << String(hmd->ProductName) << String(hmd->Manufacturer)
                    << hmd->FirmwareMajor << hmd->FirmwareMinor
                    << hmd->Resolution.w << hmd->Resolution.h;
        }

        // If there is no Oculus Rift connected, do nothing.
        if(!hmd) return;

        App::config("vr.oculusRift.pixelDensity").audienceForChange() += this;

        DENG2_GUARD(this);

        // Configure for orientation and position tracking.
        ovrHmd_ConfigureTracking(hmd, ovrTrackingCap_Orientation |
                                      ovrTrackingCap_MagYawCorrection |
                                      ovrTrackingCap_Position, 0);

        LOG_GL_MSG("Initializing Oculus Rift for rendering");

        // We will be rendering into the main window.
        window = &CanvasWindow::main().as<BaseWindow>();
        DENG2_ASSERT(window->isVisible());

        DENG2_ASSERT(QGLContext::currentContext() != 0);

        // Observe key events for dismissing the Health and Safety warning.
        window->canvas().audienceForKeyEvent() += this;

        // Set up the rendering target according to the OVR parameters.
        auto &buf = framebuffer();
        buf.glInit();

        // Set up the framebuffer and eye viewports.
        resizeFramebuffer();

        uint distortionCaps = hmd->DistortionCaps &
                ( ovrDistortionCap_Chromatic
                | ovrDistortionCap_TimeWarp
                | ovrDistortionCap_Vignette
                | ovrDistortionCap_Overdrive );

        // Configure OpenGL.
        ovrGLConfig cfg;
        cfg.OGL.Header.API         = ovrRenderAPI_OpenGL;
        cfg.OGL.Header.RTSize      = hmd->Resolution;
        cfg.OGL.Header.Multisample = buf.sampleCount();
#ifdef WIN32
        cfg.OGL.Window             = (HWND) window->nativeHandle();
        cfg.OGL.DC                 = wglGetCurrentDC();
#endif
        if(!ovrHmd_ConfigureRendering(hmd, &cfg.Config, distortionCaps, fov, render))
        {
            LOG_GL_ERROR("Failed to configure Oculus Rift for rendering");
            return;
        }

        for(int i = 0; i < 2; ++i)
        {
            eyeOffset[i] = Vector3f(render[i].HmdToEyeViewOffset.x,
                                    render[i].HmdToEyeViewOffset.y,
                                    render[i].HmdToEyeViewOffset.z);
        }

        /*
        for(int i = 0; i < 2; ++i)
        {
            qDebug() << "Eye:" << render[i].Eye
                     << "Fov:" << render[i].Fov.LeftTan << render[i].Fov.RightTan
                     << render[i].Fov.UpTan << render[i].Fov.DownTan
                     << "DistortedViewport:" << render[i].DistortedViewport.Pos.x
                     << render[i].DistortedViewport.Pos.y
                     << render[i].DistortedViewport.Size.w
                     << render[i].DistortedViewport.Size.h
                     << "PixelsPerTanAngleAtCenter:" << render[i].PixelsPerTanAngleAtCenter.x
                     << render[i].PixelsPerTanAngleAtCenter.y
                     << "ViewAdjust:" << render[i].ViewAdjust.x
                     << render[i].ViewAdjust.y
                     << render[i].ViewAdjust.z;
        }*/

        /*
        float clearColor[4] = { 0.0f, 0.5f, 1.0f, 0.0f };
        ovrHmd_SetFloatArray(hmd, "DistortionClearColor", clearColor, 4);
        */

        // Move the window to the correct display.
        //if(ovrHmd_GetEnabledCaps(hmd) & ovrHmdCap_ExtendDesktop)
        {
            //LOG_GL_MSG("Using extended desktop mode");

        }
        /*
        else
        {
            LOG_GL_MSG("Using direct-to-HMD rendering mode");
        }*/

        ovrHmd_AttachToWindow(hmd, window->nativeHandle(), NULL, NULL);

        moveWindow(HMDScreen);
#endif
    }

#ifdef DENG_HAVE_OCULUS_API
    QRect screenGeometry(Screen which) const
    {
        foreach(QScreen *scr, qApp->screens())
        {
#ifdef WIN32
            bool isRift = String(hmd->DisplayDeviceName).startsWith(scr->name());
            if((which == HMDScreen && isRift) || (which == DefaultScreen && !isRift))
            {
                LOG_GL_MSG("HMD display: \"%s\" Screen: \"%s\" Geometry: %i,%i %ix%i")
                        << String(hmd->DisplayDeviceName)
                        << scr->name()
                        << scr->geometry().left() << scr->geometry().top()
                        << scr->geometry().width() << scr->geometry().height();
                return scr->geometry();
            }
#else
            DENG2_UNUSED(scr);
            DENG2_UNUSED(which);
#endif
        }
        // Fall back the first screen.
        return qApp->screens().at(0)->geometry();
    }
#endif

    void deinit()
    {
        if(!inited) return;
        inited = false;
        frameOngoing = false;

#ifdef DENG_HAVE_OCULUS_API
        DENG2_GUARD(this);

        LOG_GL_MSG("Stopping Oculus Rift rendering");

        App::config("vr.oculusRift.pixelDensity").audienceForChange() -= this;

        if(hmd)
        {
            ovrHmd_ConfigureRendering(hmd, NULL, 0, NULL, NULL);
            framebuffer().glDeinit();

            moveWindow(PreviousScreen);
            window->canvas().audienceForKeyEvent() -= this;
            window = 0;

            ovrHmd_Destroy(hmd);
        }
#endif
    }

#ifdef DENG_HAVE_OCULUS_API
    bool isWindowOnHMD() const
    {
        if(!window) return false;
        QRect const hmdRect = screenGeometry(HMDScreen);
        return hmdRect.contains(window->geometry());
    }

    void moveWindow(Screen screen)
    {
        if(!window) return;

        if(screen == HMDScreen)
        {
            if(isWindowOnHMD()) return; // Nothing further to do.
#ifdef WIN32            
            oldGeometry = window->geometry();
            window->setGeometry(screenGeometry(HMDScreen));
            window->showFullScreen();
#endif
        }
        else if(screen == PreviousScreen)
        {
            if(!isWindowOnHMD()) return;
#ifdef WIN32
            window->showMaximized();
            window->setGeometry(oldGeometry);
#endif
        }
        else
        {
            if(!isWindowOnHMD()) return;
#ifdef WIN32
            window->showMaximized();
            window->setGeometry(screenGeometry(DefaultScreen));
#endif
        }
    }
#endif

    /**
     * Observe key events (any key) to dismiss the OVR Health and Safety warning.
     */
    void keyEvent(KeyEvent const &ev)
    {
        if(!window || ev.type() == Event::KeyRelease) return;

#ifdef DENG_HAVE_OCULUS_API
        if(isHealthAndSafetyWarningDisplayed())
        {
            if(dismissHealthAndSafetyWarning())
            {
                window->canvas().audienceForKeyEvent() -= this;
            }
        }
#endif
    }

#ifdef DENG_HAVE_OCULUS_API
    bool isReady() const
    {
        return hmd != 0;
    }

    bool isHealthAndSafetyWarningDisplayed() const
    {
        if(!isReady()) return false;

        ovrHSWDisplayState hswDisplayState;
        ovrHmd_GetHSWDisplayState(hmd, &hswDisplayState);
        return hswDisplayState.Displayed;
    }

    bool dismissHealthAndSafetyWarning()
    {
        if(isHealthAndSafetyWarningDisplayed())
        {
            return ovrHmd_DismissHSWDisplay(hmd);
        }
        return true;
    }

    void dismissHealthAndSafetyWarningOnTap()
    {
        if(!isHealthAndSafetyWarningDisplayed()) return;

        ovrTrackingState ts = ovrHmd_GetTrackingState(hmd, ovr_GetTimeInSeconds());
        if(ts.StatusFlags & ovrStatus_OrientationTracked)
        {
            OVR::Vector3f rawAccel(ts.RawSensorData.Accelerometer.x,
                                   ts.RawSensorData.Accelerometer.y,
                                   ts.RawSensorData.Accelerometer.z);

            // Arbitrary value and representing moderate tap on the side of the DK2 Rift.
            if(rawAccel.LengthSq() > 250.f)
            {
                ovrHmd_DismissHSWDisplay(hmd);
            }
        }
    }

    void updateEyePoses()
    {
        if(!frameOngoing) return;
        needPoseUpdate = false;

        ovrVector3f hmdEyeOffsets[2] {
            render[0].HmdToEyeViewOffset,
            render[1].HmdToEyeViewOffset
        };

        // Pose for the current eye.
        ovrHmd_GetEyePoses(hmd, 0, hmdEyeOffsets, headPose, nullptr);

        pitchRollYaw = quaternionToPRYAngles(headPose[0].Orientation);

        headPosition = Vector3f((headPose[0].Position.x + headPose[1].Position.x)/2,
                                (headPose[0].Position.y + headPose[1].Position.y)/2,
                                (headPose[0].Position.z + headPose[1].Position.z)/2);

        for(int i = 0; i < 2; ++i)
        {
            // TODO: Rotate using provided quaternion.
            // Note that Doomsday doesn't currently use this matrix!
            eyeMatrix[i] = Matrix4f::translate(Vector3f(headPose[i].Position.x,
                                                        headPose[i].Position.y,
                                                        headPose[i].Position.z))
                        *
                        Matrix4f::rotate(-radianToDegree(pitchRollYaw[1]), Vector3f(0, 0, 1)) *
                        Matrix4f::rotate(-radianToDegree(pitchRollYaw[0]), Vector3f(1, 0, 0)) *
                        Matrix4f::rotate(-radianToDegree(pitchRollYaw[2]), Vector3f(0, 1, 0));
                        //*
                        //Matrix4f::translate(-eyeOffset[i]);
        }
    }

    void beginFrame()
    {
        DENG2_ASSERT(isReady());
        DENG2_ASSERT(!frameOngoing);

        if(densityChanged)
        {
            densityChanged = false;
            resizeFramebuffer();
        }

        frameOngoing = true;
        needPoseUpdate = true;
        timing = ovrHmd_BeginFrame(hmd, 0);
    }

    void endFrame()
    {
        DENG2_ASSERT(frameOngoing);

        ovrHmd_EndFrame(hmd, headPose, textures);

        dismissHealthAndSafetyWarningOnTap();
        frameOngoing = false;
    }
#endif
};

OculusRift::OculusRift() : d(new Instance(this))
{}

void OculusRift::glPreInit()
{
#ifdef DENG_HAVE_OCULUS_API
    LOG_AS("OculusRift");
    LOG_VERBOSE("Initializing LibOVR");
    ovr_Initialize();
#endif
}

bool OculusRift::isHMDConnected() const
{
#ifdef DENG_HAVE_OCULUS_API
    if(d->isReady()) return true;
    return ovrHmd_Detect() > 0;
#endif
    return false;
}

void OculusRift::init()
{
    LOG_AS("OculusRift");
    d->init();
}

void OculusRift::deinit()
{
    LOG_AS("OculusRift");
    d->deinit();
}

void OculusRift::beginFrame()
{
#ifdef DENG_HAVE_OCULUS_API    
    if(!isReady() || !d->inited || d->frameOngoing) return;

    // Begin the frame and acquire timing information.
    d->beginFrame();
#endif
}

void OculusRift::endFrame()
{
#ifdef DENG_HAVE_OCULUS_API
    if(!isReady() || !d->frameOngoing) return;

    // End the frame and let the Oculus SDK handle displaying it with the
    // appropriate transformation.
    d->endFrame();
#endif
}

void OculusRift::setCurrentEye(int index)
{
#ifdef DENG_HAVE_OCULUS_API
    if(d->hmd)
    {
        d->currentEye = d->hmd->EyeRenderOrder[index];
    }
#else
    DENG2_UNUSED(index);
#endif
}

OculusRift::Eye OculusRift::currentEye() const
{
#ifdef DENG_HAVE_OCULUS_API
    return (d->currentEye == ovrEye_Left? LeftEye : RightEye);
#else
    return LeftEye;
#endif
}

Vector2ui OculusRift::resolution() const
{
#ifdef DENG_HAVE_OCULUS_API
    if(!d->hmd) return Vector2ui();
    return Vector2ui(d->hmd->Resolution.w, d->hmd->Resolution.h);
#else
    return Vector2ui();
#endif
}

void OculusRift::setYawOffset(float yawRadians)
{
    d->yawOffset = yawRadians;
}

void OculusRift::resetTracking()
{
#ifdef DENG_HAVE_OCULUS_API
    if(d->isReady())
    {
        ovrHmd_RecenterPose(d->hmd);
    }
#endif
}

void OculusRift::resetYaw()
{
    d->yawOffset = -d->pitchRollYaw.z;

    /*
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);
    if(isReady())
    {
        d->yawOffset = -d->oculusDevice->yaw;
    }
#endif
    */
}

// True if Oculus Rift is enabled and can report head orientation.
bool OculusRift::isReady() const
{
#ifdef DENG_HAVE_OCULUS_API
    return d->isReady();
#else
    return false;
#endif
}

Vector3f OculusRift::headOrientation() const
{
#ifdef DENG_HAVE_OCULUS_API
    if(d->needPoseUpdate) d->updateEyePoses();
#endif
    Vector3f pry = d->pitchRollYaw;
    pry.z = wrap(pry.z + d->yawOffset, -PIf, PIf);
    return pry;
}

Matrix4f OculusRift::eyePose() const
{
    DENG2_ASSERT(isReady());
#ifdef DENG_HAVE_OCULUS_API
    if(d->needPoseUpdate) d->updateEyePoses();
    return d->eyeMatrix[d->currentEye];
#else
    return Matrix4f();
#endif
}

Vector3f OculusRift::headPosition() const
{
#ifdef DENG_HAVE_OCULUS_API
    if(d->needPoseUpdate) d->updateEyePoses();
#endif
    return d->headPosition;
}

Vector3f OculusRift::eyeOffset() const
{
#ifdef DENG_HAVE_OCULUS_API
    return d->eyeOffset[d->currentEye];
#else
    return Vector3f();
#endif
}

Matrix4f OculusRift::projection(float nearDist, float farDist) const
{
    DENG2_ASSERT(isReady());
#ifdef DENG_HAVE_OCULUS_API
    return Matrix4f(ovrMatrix4f_Projection(d->fov[d->currentEye], nearDist, farDist,
                    true /* right-handed */).M[0]).transpose();
#else
    DENG2_UNUSED2(nearDist, farDist);
    return Matrix4f();
#endif
}

float OculusRift::yawOffset() const
{
    return d->yawOffset;
}

float OculusRift::aspect() const
{
    return d->aspect;
}

float OculusRift::fovX() const
{
#ifdef DENG_HAVE_OCULUS_API
    return d->fovXDegrees;
#endif
    return 0;
}

void OculusRift::moveWindowToScreen(Screen screen)
{
#ifdef DENG_HAVE_OCULUS_API
    d->moveWindow(screen);
#else
    DENG2_UNUSED(screen);
#endif
}

} // namespace de
