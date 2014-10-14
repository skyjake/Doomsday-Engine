/** @file ui2_main.cpp  InFine animation system widgets.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2014 Daniel Swanson <danij@dengine.net>
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
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#include <cstring> // memcpy, memmove
#include <QtAlgorithms>
#include <de/concurrency.h>
#include <de/memoryzone.h>
#include <de/timer.h>
#include <de/vector1.h>
#include <de/Log>

#include "de_base.h"
#include "de_audio.h"
#include "de_console.h"
#include "de_graphics.h"
#include "de_render.h"
#include "de_resource.h"
#include "de_ui.h"

#include "ui/infine/finaleinterpreter.h"
#include "ui/infine/finalepagewidget.h"

#ifdef __CLIENT__
#  include "MaterialSnapshot"
#  include "gl/gl_texmanager.h"
#  include "gl/sys_opengl.h" // TODO: get rid of this
#  include "ui/clientwindow.h"
#endif

using namespace de;

static bool inited;

/// Global widget store.
static QList<FinaleWidget *> widgets;

static FinaleWidget *findWidget(Id const &id)
{
    if(!id.isNone())
    {
        for(FinaleWidget *wi : widgets)
        {
            if(wi->id() == id) return wi;
        }
    }
    return nullptr;
}

void UI_Init()
{
    // Already been here?
    if(inited) return;

    inited = true;
}

void UI_Shutdown()
{
    if(!inited) return;

    // Garbage collection.
    qDeleteAll(widgets); widgets.clear();

    inited = false;
}

FinaleWidget *FI_Widget(Id const &id)
{
    if(!inited)
    {
        LOG_AS("FI_Widget")
        LOGDEV_SCR_WARNING("Widget %i not initialized yet!") << id;
        return 0;
    }
    return findWidget(id);
}

FinaleWidget *FI_Link(FinaleWidget *widgetToLink)
{
    if(widgetToLink)
    {
        widgets.append(widgetToLink);
    }
    return widgetToLink;
}

FinaleWidget *FI_Unlink(FinaleWidget *widgetToUnlink)
{
    if(widgetToUnlink)
    {
        widgets.removeOne(widgetToUnlink);
    }
    return widgetToUnlink;
}

#ifdef __CLIENT__

static void setupProjectionForFinale(dgl_borderedprojectionstate_t *bp)
{
    GL_ConfigureBorderedProjection(bp, BPF_OVERDRAW_CLIP,
                                   SCREENWIDTH, SCREENHEIGHT,
                                   DENG_GAMEVIEW_WIDTH, DENG_GAMEVIEW_HEIGHT,
                                   scalemode_t(Con_GetByte("rend-finale-stretch")));
}

bool FI_IsStretchedToView()
{
    dgl_borderedprojectionstate_t bp;
    setupProjectionForFinale(&bp);
    return (bp.scaleMode == SCALEMODE_STRETCH);
}

void UI2_Drawer()
{
    LOG_AS("UI2_Drawer");
    if(!inited)
    {
        LOGDEV_ERROR("Not initialized yet!");
        return;
    }

    if(!App_InFineSystem().finaleInProgess()) return;

    dgl_borderedprojectionstate_t bp;
    //dd_bool bordered;

    setupProjectionForFinale(&bp);
    GL_BeginBorderedProjection(&bp);

    /*bordered = (FI_ScriptActive() && FI_ScriptCmdExecuted());
    if(bordered)
    {
        // Draw using the special bordered projection.
        GL_ConfigureBorderedProjection(&borderedProjection);
        GL_BeginBorderedProjection(&borderedProjection);
    }*/

    for(Finale *finale : App_InFineSystem().finales())
    {
        finale->interpreter().page(FinaleInterpreter::Anims).draw();
        finale->interpreter().page(FinaleInterpreter::Texts).draw();
    }

    GL_EndBorderedProjection(&bp);

    //if(bordered)
    //    GL_EndBorderedProjection(&borderedProjection);
}

#endif // __CLIENT__
