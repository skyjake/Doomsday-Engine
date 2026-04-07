/** @file textapp.h  Application with text-based/console interface.
 *
 * @authors Copyright © 2013-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef LIBCORE_TEXTAPP_H
#define LIBCORE_TEXTAPP_H

#include "app.h"
#include "loop.h"

/**
 * Macro for conveniently accessing the de::TextApp singleton instance.
 */
#define DE_TEXT_APP   (static_cast<de::TextApp *>(&de::App::app()))

namespace de {

/**
 * Application with a text-based/console UI.
 *
 * The event loop is protected against uncaught exceptions. Catches the
 * exception and shuts down the app cleanly.
 *
 * @ingroup core
 */
class DE_PUBLIC TextApp : public App
{
public:
    TextApp(const StringList &args);

    void setMetadata(const String &orgName,
                     const String &orgDomain,
                     const String &appName,
                     const String &appVersion);

    /**
     * Start the application event loop.
     *
     * @param startup  Function to call immediately after starting the event loop.
     */
    int exec(const std::function<void()> &startup = {});

    void quit(int code);

    Loop &loop();

protected:
    NativePath appDataPath() const;

private:
    DE_PRIVATE(d)
};

} // namespace de

#endif // LIBCORE_TEXTAPP_H
