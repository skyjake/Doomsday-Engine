#ifndef ICAMERA_H
#define ICAMERA_H

#include <de/Matrix>

namespace gloom {

class ICamera
{
public:
    virtual ~ICamera() {}

    /**
     * Returns the position of the camera in world space.
     */
    virtual de::Vector3f cameraPosition() const = 0;

    virtual de::Vector3f cameraFront() const = 0;

    virtual de::Vector3f cameraUp() const = 0;

    virtual de::Matrix4f cameraProjection() const = 0;

    virtual de::Matrix4f cameraModelView() const = 0;

    virtual de::Matrix4f cameraModelViewProjection() const {
        return cameraProjection() * cameraModelView();
    }
};

} // namespace gloom

#endif // ICAMERA_H
