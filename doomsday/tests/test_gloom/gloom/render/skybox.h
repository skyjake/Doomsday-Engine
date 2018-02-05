#ifndef SKYBOX_H
#define SKYBOX_H

#include <de/AtlasTexture>
#include <de/Matrix>

class SkyBox
{
public:
    SkyBox();

    void setAtlas(de::AtlasTexture &atlas);
    void setSize(float scale);

    void glInit();
    void glDeinit();
    void render(de::Matrix4f const &mvpMatrix);

private:
    DENG2_PRIVATE(d)
};

#endif // SKYBOX_H
