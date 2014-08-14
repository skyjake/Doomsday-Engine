/** @file testapp.h  Test application.
 *
 * @authors Copyright (c) 2014 Jaakko Keränen <jaakko.keranen@iki.fi>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, see:
 * http://www.gnu.org/licenses</small>
 */

#ifndef APPFW_TEST_APP_H
#define APPFW_TEST_APP_H

#include <de/BaseGuiApp>
#include <de/ImageBank>
#include "appwindowsystem.h"

#define OVR_FOV         122.f
#define OVR_NEAR_CLIP   .1f
#define OVR_FAR_CLIP    1300.f

class TestApp : public de::BaseGuiApp
{
public:
    TestApp(int &argc, char **argv);

    void initialize();

    static TestApp &app();
    static AppWindowSystem &windowSystem();
    static MainWindow &main();
    static de::ImageBank &images();

private:
    DENG2_PRIVATE(d)
};

#endif // APPFW_TEST_APP_H
