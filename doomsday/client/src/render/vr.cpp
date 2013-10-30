#include "de_console.h"
#include "render/vr.h"

// Console variables
int VR::mode = 0;
// Interpupillary distance in meters
float VR::ipd = 0.0622f;
float VR::playerHeight = 1.70f;
float VR::dominantEye = 0.0f;
byte  VR::swapEyes = 0;


// Global variables
bool  VR::applyFrustumShift = true;
float  VR::eyeShift = 0;
float  VR::hudDistance = 30.0f;
float  VR::weaponDistance = 10.0f;


/// @param eye: -1 means left eye, +1 means right eye
/// @return viewpoint eye shift in map units
float VR::getEyeShift(float eye)
{
    // 0.95 because eyes are not at top of head
    float mapUnitsPerMeter = Con_GetInteger("player-eyeheight") / ((0.95) *  VR::playerHeight);
    float result = mapUnitsPerMeter * (eye -  VR::dominantEye) * 0.5 *  VR::ipd;
    if ( VR::swapEyes != 0)
        result *= -1;
    return result;
}

static void vrModeChanged()
{
    if(ClientWindow::hasMain())
    {
        // The logical UI size may need to be changed.
        ClientWindow::main().updateRootSize();
    }
}

void VR::consoleRegister()
{
    C_VAR_FLOAT ("rend-vr-ipd",              & VR::ipd,           0, 0.02f, 0.2f);
    C_VAR_FLOAT ("rend-vr-player-height",    & VR::playerHeight,  0, 1.0f, 3.0f);
    C_VAR_FLOAT ("rend-vr-dominant-eye",     & VR::dominantEye,   0, -1.0f, 1.0f);
    C_VAR_BYTE  ("rend-vr-swap-eyes",        & VR::swapEyes,      0, 0, 1);
    C_VAR_INT2  ("rend-vr-mode",             & VR::mode,          0, 0, (int)(VR::MODE_MAX_3D_MODE_PLUS_ONE - 1), vrModeChanged);
}


// Warping

/// @todo warping

// Head tracking

#ifdef DENG_HAVE_OCULUS_API
#include "OVR.h"

class OculusTracker {
public:
    OculusTracker()
        : pitch(0)
        , roll(0)
        , yaw(0)
    {
        OVR::System::Init();
        pFusionResult = new OVR::SensorFusion();
        pManager = *OVR::DeviceManager::Create();
        pHMD = *pManager->EnumerateDevices<OVR::HMDDevice>().CreateDevice();
        if(pHMD)
        {
            InfoLoaded = pHMD->GetDeviceInfo(&Info);
            pSensor = pHMD->GetSensor();
        }
        else
        {
            pSensor = *pManager->EnumerateDevices<OVR::SensorDevice>().CreateDevice();
        }

        if (pSensor)
        {
            pFusionResult->AttachToSensor(pSensor);
        }
    }

    ~OculusTracker()
    {
        pSensor.Clear();
        pHMD.Clear();
        pManager.Clear();
        delete pFusionResult;
        OVR::System::Destroy();
    }

    const OVR::HMDInfo& getInfo() const {
        return Info;
    }

    bool isGood() const {
        return pSensor.GetPtr() != NULL;
    }

    void update()
    {
        OVR::Quatf quaternion = pFusionResult->GetOrientation();
        quaternion.GetEulerAngles<OVR::Axis_Y, OVR::Axis_X, OVR::Axis_Z>(&yaw, &pitch, &roll);
    }

    // Head orientation state, refreshed by call to update();
    float pitch, roll, yaw;

private:
    OVR::Ptr<OVR::DeviceManager> pManager;
    OVR::Ptr<OVR::HMDDevice> pHMD;
    OVR::Ptr<OVR::SensorDevice> pSensor;
    OVR::SensorFusion* pFusionResult;
    OVR::HMDInfo Info;
    bool InfoLoaded;
};

static OculusTracker* oculusTracker = NULL;
#endif

// True if Oculus Rift is enabled and can report head orientation.
bool VR::hasHeadOrientation()
{
#ifdef DENG_HAVE_OCULUS_API
    if (oculusTracker == NULL)
        oculusTracker = new OculusTracker();
    return oculusTracker->isGood();
#else
    // No API; No head tracking.
    return false;
#endif
}

// Returns current pitch, roll, yaw angles, in radians
std::vector<float> VR::getHeadOrientation()
{
    std::vector<float> result;
#ifdef DENG_HAVE_OCULUS_API
    if (! VR::hasHeadOrientation())
        return result; // empty vector
    oculusTracker->update();
    result.push_back(oculusTracker->pitch);
    result.push_back(oculusTracker->roll);
    result.push_back(oculusTracker->yaw);
    return result;
#else
    // No API; Return empty vector. You should have called VR::hasHeadOrientation() first...
    return result;
#endif
}

// To release memory and resources when done, for tidiness.
void VR::deleteOculusTracker()
{
#ifdef DENG_HAVE_OCULUS_API
    if (oculusTracker != NULL)
    {
        delete oculusTracker;
        oculusTracker = NULL;
    }
#endif
}
