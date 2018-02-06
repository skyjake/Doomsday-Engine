#include "gloomwidget.h"
#include "audio/audiosystem.h"
#include "world/world.h"
#include "world/user.h"

#include <de/Drawable>
#include <de/GLBuffer>
#include <de/BaseGuiApp>
#include <de/Matrix>
#include <de/VRConfig>
#include <de/KeyEvent>

using namespace de;

DENG_GUI_PIMPL(GloomWidget)
{
    Matrix4f         modelView;
    World *          world = nullptr;
    Time             previousUpdateAt;
    User             user;
    User::InputState inputs;

    Impl(Public *i) : Base(i)
    {}

    void glInit()
    {
        if (world) { world->glInit(); }
        updateModelView();
        AudioSystem::get().setListener(&self());
    }

    void updateModelView()
    {
        modelView = Matrix4f::rotate(user.yaw(), Vector3f(0, 1, 0)) *
                    Matrix4f::rotate(180, Vector3f(0, 0, 1)) *
                    Matrix4f::translate(-user.position());
    }

    Matrix4f viewMatrix() const
    {
//        OculusRift &ovr = GloomApp::vr().oculusRift();

//        Vector3f neck = Vector3f(0, .35f, -.4f) * 2;

//        Matrix4f neckOff =
//                Matrix4f::translate(neck) *
//                Matrix4f::rotate(radianToDegree(ovr.headOrientation().x)/6, Vector3f(1, 0, 0)) *
//                Matrix4f::translate(-neck);

        return /*neckOff * ovr.eyePose() * */ modelView;
    }

    void glDeinit()
    {
        if (world) world->glDeinit();
    }
};

GloomWidget::GloomWidget()
    : GuiWidget("gloomwidget")
    , d(new Impl(this))
{}

World *GloomWidget::world() const
{
    return d->world;
}

User &GloomWidget::user()
{
    return d->user;
}

void GloomWidget::setCameraPosition(Vector3f const &pos)
{
    d->user.setPosition(pos);
}

void GloomWidget::setCameraYaw(float yaw)
{
    d->user.setYaw(yaw);
}

void GloomWidget::setWorld(World *world)
{
    const World *oldWorld = d->world;

    if (d->world)
    {
        if (isInitialized())
        {
            d->world->glDeinit();
        }
        d->world->setLocalUser(nullptr);
        d->user.setWorld(nullptr);
    }

    d->world = world;
    DENG2_FOR_AUDIENCE(Change, i) { i->currentWorldChanged(oldWorld, world); }

    if (d->world)
    {
        d->world->setLocalUser(&d->user);
        if (isInitialized())
        {
            try
            {
                d->world->glInit();
            }
            catch (Error const &er)
            {
                LOG_ERROR("Failed to initialize world for drawing: %s") << er.asText();
            }
        }
    }
}

//void GloomWidget::viewResized()
//{
//    GuiWidget::viewResized();
//}

void GloomWidget::update()
{
    GuiWidget::update();

    // How much time has passed?
    const TimeSpan elapsed = d->previousUpdateAt.since();
    d->previousUpdateAt = Time();

    d->user.setInputState(d->inputs);
    d->user.update(elapsed);

    if (d->world)
    {
        d->world->update(elapsed);
    }
    d->updateModelView();
}

void GloomWidget::drawContent()
{
    if (d->world)
    {
        // Any buffered draws should be done before rendering the world.
        auto &painter = root().painter();
        painter.flush();
        GLState::push().setNormalizedScissor(painter.normalizedScissor());

        d->world->render(*this);

        GLState::pop();
    }
}

bool GloomWidget::handleEvent(Event const &event)
{
    if (event.isKey())
    {
        KeyEvent const &key = event.as<KeyEvent>();

        User::InputBit bit = User::Inert;
        switch (key.ddKey())
        {
        case 'q':
        case DDKEY_LEFTARROW:  bit = User::TurnLeft; break;
        case 'e':
        case DDKEY_RIGHTARROW: bit = User::TurnRight; break;
        case 'w':
        case DDKEY_UPARROW:    bit = User::Forward; break;
        case 's':
        case DDKEY_DOWNARROW:  bit = User::Backward; break;
        case 'a':              bit = User::StepLeft; break;
        case 'd':              bit = User::StepRight; break;
        case DDKEY_LSHIFT:     bit = User::Shift; break;
        default:               break;
        }

        if (bit)
        {
            if (key.state() != KeyEvent::Released)
                d->inputs |= bit;
            else
                d->inputs &= ~bit;
        }
    }

    return GuiWidget::handleEvent(event);
}

Vector3f GloomWidget::cameraPosition() const
{
    return d->user.position();
}

Vector3f GloomWidget::cameraFront() const
{
    Vector4f v = d->viewMatrix().inverse() * Vector4f(0, 0, -1, 0);
    return v.normalize();
}

Vector3f GloomWidget::cameraUp() const
{
    Vector3f v = d->viewMatrix().inverse() * Vector4f(0, -1, 0, 0);
    return v.normalize();
}

Matrix4f GloomWidget::cameraProjection() const
{
    const auto size = rule().size();
    return Matrix4f::perspective(80, size.x / size.y, 0.1f, 2500.f);
}

Matrix4f GloomWidget::cameraModelView() const
{
    return d->viewMatrix();
}

void GloomWidget::glInit()
{
    GuiWidget::glInit();
    d->glInit();
}

void GloomWidget::glDeinit()
{
    GuiWidget::glDeinit();
    d->glDeinit();
}
