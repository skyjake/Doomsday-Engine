/** @file nativeui.cpp Native GUI functionality.
 * @ingroup base
 *
 * @authors Copyright © 2012-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2013 Daniel Swanson <danij@dengine.net>
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

#include "dd_share.h"
#include "sys_system.h"
#include "ui/nativeui.h"
#include "ui/clientwindow.h"

#include <de/app.h>
#include <de/bytearrayfile.h>
#include <de/filesystem.h>
#include <SDL_messagebox.h>
#include <stdarg.h>

void Sys_MessageBox(messageboxtype_t type,
                    const char *     title,
                    const char *     msg,
                    const char *     detailedMsg)
{
    Sys_MessageBox2(type, title, msg, 0, detailedMsg);
}

void Sys_MessageBox2(messageboxtype_t type,
                     const char *     title,
                     const char *     msg,
                     const char *     informativeMsg,
                     const char *     detailedMsg)
{
    Sys_MessageBox3(type, title, msg, informativeMsg, detailedMsg, 0);
}

int Sys_MessageBox3(messageboxtype_t type,
                    const char *     title,
                    const char *     msg,
                    const char *     informativeMsg,
                    const char *     detailedMsg,
                    const char **    buttons)
{
    if (novideo)
    {
        // There's no GUI...
        de::warning("%s", msg);
        return 0;
    }

    if (ClientWindow::mainExists())
    {
        ClientWindow::main().hide();
    }

    SDL_MessageBoxData box{};
    box.title = title;

    switch (type)
    {
    case MBT_INFORMATION: box.flags = SDL_MESSAGEBOX_INFORMATION; break;
    case MBT_QUESTION:    box.flags = SDL_MESSAGEBOX_INFORMATION; break;
    case MBT_WARNING:     box.flags = SDL_MESSAGEBOX_WARNING;     break;
    case MBT_ERROR:       box.flags = SDL_MESSAGEBOX_ERROR;       break;
    default:
        break;
    }

    de::String text = msg;
    if (detailedMsg)
    {
        /// @todo Making the dialog a bit wider would be nice, but it seems one has to
        /// derive a new message box class for that -- the default one has a fixed size.
        text += "\n\n";
        text += detailedMsg;
    }
    if (informativeMsg)
    {
        text += "\n\n";
        text += informativeMsg;
    }
    box.message = text;

    std::vector<SDL_MessageBoxButtonData> buttonData;
    if (buttons)
    {
        for (int i = 0; buttons[i]; ++i)
        {
            buttonData.push_back(SDL_MessageBoxButtonData{
                Uint32(i == 0 ? SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT
                     : i == 1 ? SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT : 0),
                i,
                buttons[i]});
        }
    }
    else
    {
        buttonData.push_back(
            SDL_MessageBoxButtonData{SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK"});
    }
    box.buttons    = buttonData.data();
    box.numbuttons = int(buttonData.size());
    int rc;
    SDL_ShowMessageBox(&box, &rc);
    return rc;
}

void Sys_MessageBoxf(messageboxtype_t type, const char* title, const char* format, ...)
{
    char buffer[1024];
    va_list args;

    va_start(args, format);
    dd_vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    Sys_MessageBox(type, title, buffer, 0);
}

int Sys_MessageBoxWithButtons(messageboxtype_t type,
                              const char *     title,
                              const char *     msg,
                              const char *     informativeMsg,
                              const char **    buttons)
{
    return Sys_MessageBox3(type, title, msg, informativeMsg, 0, buttons);
}

void Sys_MessageBoxWithDetailsFromFile(messageboxtype_t type,
                                       const char *     title,
                                       const char *     msg,
                                       const char *     informativeMsg,
                                       const char *     detailsFileName)
{
    try
    {
        de::String details;
        de::App::rootFolder().locate<const de::File>(detailsFileName) >> details;

        // This will be used as a null-terminated string.
        details.append('\0');

        Sys_MessageBox2(type, title, msg, informativeMsg, details);
    }
    catch (const de::Error &er)
    {
        de::warning("Could not read \"%s\": %s", detailsFileName, er.asText().c_str());

        // Show it without the details, then.
        Sys_MessageBox2(type, title, msg, informativeMsg, 0);
    }
}
