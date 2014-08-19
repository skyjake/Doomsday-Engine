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
#  include <../Src/OVR_CAPI_GL.h> // shouldn't this be included by the SDK?
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
, public Lockable
{
#ifdef DENG_HAVE_OCULUS_API
    ovrHmd hmd;
    ovrEyeType currentEye;
    ovrPosef headPose[2];
    //bool needPose[2] { true, true };
    ovrEyeRenderDesc render[2];
    ovrTexture textures[2];
    ovrFovPort fov[2];
    float fovXDegrees;
    ovrFrameTiming timing;
#endif
    Matrix4f eyeMatrix;
    Vector3f pitchRollYaw;
    Vector3f headPosition;
    float aspect = 1.f;

    BaseWindow *window = nullptr;

    bool inited = false;
    bool frameOngoing = false;
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
#endif
    }

    ~Instance()
    {
        DENG2_GUARD(this);
        deinit();

#ifdef DENG_HAVE_OCULUS_API
        if(hmd)
        {
            ovrHmd_Destroy(hmd);
        }
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

    void resizeFramebuffer()
    {
        Sizei size[2];
        ovrFovPort fovMax;
        for(int eye = 0; eye < 2; ++eye)
        {
            // Use the default FOV.
            fov[eye] = hmd->DefaultEyeFov[eye];

            size[eye] = ovrHmd_GetFovTextureSize(hmd, ovrEyeType(eye), fov[eye],
                                                 1.0f /*density*/);
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

        /// @todo Apply chosen pixel density here.

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

        // If there is no Oculus Rift connected, do nothing.
        if(!hmd) return;

#ifdef DENG_HAVE_OCULUS_API
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

        uint distortionCaps = ovrDistortionCap_Chromatic // TODO: make configurable?
                            | ovrDistortionCap_TimeWarp
                            | ovrDistortionCap_Overdrive;

        // Configure OpenGL.
        ovrGLConfig cfg;
        cfg.OGL.Header.API         = ovrRenderAPI_OpenGL;
        cfg.OGL.Header.RTSize      = Sizei(buf.size().x, buf.size().y);
        cfg.OGL.Header.Multisample = buf.sampleCount();
#ifdef WIN32
        cfg.OGL.Window             = window->nativeHandle();
        //cfg.OGL.DC               = dc;
#endif
        if(!ovrHmd_ConfigureRendering(hmd, &cfg.Config, distortionCaps, fov, render))
        {
            LOG_GL_ERROR("Failed to configure Oculus Rift for rendering");
            return;
        }

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
        }

        ovrHmd_AttachToWindow(hmd, window->nativeHandle(), NULL, NULL);

        /*
        float clearColor[4] = { 0.0f, 0.5f, 1.0f, 0.0f };
        ovrHmd_SetFloatArray(hmd, "DistortionClearColor", clearColor, 4);
        */
#endif
    }

    void deinit()
    {
        if(!inited) return;
        inited = false;
        frameOngoing = false;

#ifdef DENG_HAVE_OCULUS_API
        DENG2_GUARD(this);

        LOG_GL_MSG("Stopping Oculus Rift rendering");

        ovrHmd_ConfigureRendering(hmd, NULL, 0, NULL, NULL);

        framebuffer().glDeinit();

        window->canvas().audienceForKeyEvent() -= this;
        window = 0;
#endif
    }

    /**
     * Observe key events (any key) to dismiss the OVR Health and Safety warning.
     */
    void keyEvent(KeyEvent const &ev)
    {
        if(!window || ev.type() == Event::KeyRelease) return;

#ifdef DENG_HAVE_OCULUS_API
        if(isHealthAndSafetyWarningDisplayed())
        {
            dismissHealthAndSafetyWarning();
            window->canvas().audienceForKeyEvent() -= this;
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

    void dismissHealthAndSafetyWarning()
    {
        if(isHealthAndSafetyWarningDisplayed())
        {
            ovrHmd_DismissHSWDisplay(hmd);
        }
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

    void updateEyePose()
    {
        if(!frameOngoing) return;

        ovrPosef &pose = headPose[currentEye];

        // Pose for the current eye.
        pose = ovrHmd_GetEyePose(hmd, currentEye);

        pitchRollYaw = quaternionToPRYAngles(pose.Orientation);

        headPosition = Vector3f(pose.Position.x,
                                pose.Position.y,
                                pose.Position.z);

        // TODO: Rotation directly from quaternion?

        eyeMatrix = Matrix4f::translate(headPosition)
                    *
                    Matrix4f::rotate(-radianToDegree(pitchRollYaw[1]), Vector3f(0, 0, 1)) *
                    Matrix4f::rotate(-radianToDegree(pitchRollYaw[0]), Vector3f(1, 0, 0)) *
                    Matrix4f::rotate(-radianToDegree(pitchRollYaw[2]), Vector3f(0, 1, 0))
                    *
                    Matrix4f::translate(Vector3f(render[currentEye].ViewAdjust.x,
                                                 render[currentEye].ViewAdjust.y,
                                                 render[currentEye].ViewAdjust.z));
    }

    void beginFrame()
    {
        DENG2_ASSERT(isReady());
        DENG2_ASSERT(!frameOngoing);

        frameOngoing = true;
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
        d->updateEyePose();
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

/*
float OculusRift::interpupillaryDistance() const
{
    return d->ipd;
}*/

Vector2ui OculusRift::resolution() const
{
#ifdef DENG_HAVE_OCULUS_API
    if(!d->hmd) return Vector2ui();
    return Vector2ui(d->hmd->Resolution.w, d->hmd->Resolution.h);
#else
    return Vector2ui();
#endif
}

/*float OculusRift::aspect() const
{
    return 0.5f * d->screenSize.x / d->screenSize.y;
}

Vector2f OculusRift::screenSize() const
{
    return d->screenSize;
}

Vector4f OculusRift::chromAbParam() const
{
    return d->chromAbParam;
}

Vector4f OculusRift::hmdWarpParam() const
{
    return d->hmdWarpParam;
}

float OculusRift::lensSeparationDistance() const
{
    return d->lensSeparationDistance;
}*/

void OculusRift::setYawOffset(float yawRadians)
{
    d->yawOffset = yawRadians;
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
    /*DENG2_GUARD(d);
    return d->inited && d->oculusDevice->isGood();*/
#else
    return false;
#endif
}

/*void OculusRift::allowUpdate()
{
    //d->headOrientationUpdateIsAllowed = true;
}*/

/*
void OculusRift::update()
{
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);

    if(d->headOrientationUpdateIsAllowed && isReady())
    {
        d->oculusDevice->update();
        d->headOrientationUpdateIsAllowed = false;
    }
#endif
}*/

Vector3f OculusRift::headOrientation() const
{
    if(d->frameOngoing) d->updateEyePose();

    Vector3f pry = d->pitchRollYaw;
    pry.z = wrap(pry.z + d->yawOffset, -PIf, PIf);
    return pry;

/*    Vector3f result;
#ifdef DENG_HAVE_OCULUS_API
    DENG2_GUARD(d);
    if(isReady())
    {
        result[0] = d->oculusDevice->pitch;
        result[1] = d->oculusDevice->roll;
        result[2] = wrap(d->oculusDevice->yaw + d->yawOffset, -PIf, PIf);
    }
#endif
    return result;*/
}

Matrix4f OculusRift::eyePose() const
{
    DENG2_ASSERT(isReady());
#ifdef DENG_HAVE_OCULUS_API
    d->updateEyePose();
#endif
    return d->eyeMatrix;
}

Vector3f OculusRift::headPosition() const
{
    return d->headPosition;
}

Matrix4f OculusRift::projection(float nearDist, float farDist) const
{
    DENG2_ASSERT(isReady());
#ifdef DENG_HAVE_OCULUS_API
    return ovrMatrix4f_Projection(d->fov[d->currentEye], nearDist, farDist,
                                  false /* right-handed */).M[0];
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

/*
Matrix4f OculusRift::headModelViewMatrix() const
{
    return d->eyeMatrix;
    Vector3f const pry = headOrientation();
    return Matrix4f::rotate(-radianToDegree(pry[1]), Vector3f(0, 0, 1)) *
           Matrix4f::rotate(-radianToDegree(pry[0]), Vector3f(1, 0, 0)) *
           Matrix4f::rotate(-radianToDegree(pry[2]), Vector3f(0, 1, 0));
}
*/

/*
float OculusRift::distortionScale() const
{
    float lensShift = d->screenSize.x * 0.25f - lensSeparationDistance() * 0.5f;
    float lensViewportShift = 4.0f * lensShift / d->screenSize.x;
    float fitRadius = fabs(-1 - lensViewportShift);
    float rsq = fitRadius*fitRadius;
    Vector4f k = hmdWarpParam();
    float scale = (k[0] + k[1] * rsq + k[2] * rsq * rsq + k[3] * rsq * rsq * rsq);
    return scale;
}*/

float OculusRift::fovX() const
{
#ifdef DENG_HAVE_OCULUS_API
    return d->fovXDegrees;
#endif
    return 0;
    /*float const w = 0.25 * d->screenSize.x * distortionScale();
    return de::radianToDegree(2.0 * atan(w / d->eyeToScreenDistance));*/
}

#if 0
float OculusRift::fovY() const
{
#ifdef DENG_HAVE_OCULUS_API
#endif
    return 0;
    /*float const w = 0.5 * d->screenSize.y * distortionScale();
    return de::radianToDegree(2.0 * atan(w / d->eyeToScreenDistance));*/
}
#endif

} // namespace de
