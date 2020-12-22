/** @file manualconnectiondialog.h  Dialog for connecting to a server.
 *
 * @authors Copyright (c) 2014-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#ifndef DE_CLIENT_MANUALCONNECTIONDIALOG_H
#define DE_CLIENT_MANUALCONNECTIONDIALOG_H

#include <de/inputdialog.h>
#include <de/ipersistent.h>

/**
 * Dialog for connecting to a multiplayer server manually using an IP address or domain
 * name. The TCP port number can also be optionally provided.
 *
 * The dialog stores the previously used address persistently.
 */
class ManualConnectionDialog : public de::InputDialog, public de::IPersistent
{
public:
    DE_AUDIENCE(Selection, void manualConnectionSelected(const de::ui::Item *))

public:
    ManualConnectionDialog(const de::String &name = "manualconnection");

    /**
     * Enables or disables joining the selected game when the user clicks on a
     * session. By default, this is enabled.
     *
     * @param joinWhenSelected  @c true to allow auto-joining.
     */
    void enableJoinWhenSelected(bool joinWhenSelected);

    void queryOrConnect();
    void contentChanged();
    void validate();

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (const de::PersistentState &fromState);

protected:
    void finish(int result);

private:
    DE_PRIVATE(d)
};

#endif // DE_CLIENT_MANUALCONNECTIONDIALOG_H
