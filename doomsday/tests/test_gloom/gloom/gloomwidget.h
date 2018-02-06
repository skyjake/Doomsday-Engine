#ifndef GLOOMWIDGET_H
#define GLOOMWIDGET_H

#include <de/GuiWidget>
#include "icamera.h"

namespace gloom {

class World;
class User;

class GloomWidget
    : public de::GuiWidget
    , public ICamera
{
    Q_OBJECT

public:
    GloomWidget();

    World *world() const;
    User & user();

    void setCameraPosition(const de::Vector3f &pos);
    void setCameraYaw(float yaw);

    // Events.
    void update();
    void drawContent();
    bool handleEvent(de::Event const &event);

    // Implements ICamera.
    de::Vector3f cameraPosition() const;
    de::Vector3f cameraFront() const;
    de::Vector3f cameraUp() const;
    de::Matrix4f cameraModelView() const;
    de::Matrix4f cameraProjection() const;

    DENG2_DEFINE_AUDIENCE(Change, void currentWorldChanged(const World *old, World *current))

public slots:
    void setWorld(World *world);

protected:
    void glInit();
    void glDeinit();

private:
    DENG2_PRIVATE(d)
};

} // namespace gloom

#endif // GLOOMWIDGET_H
