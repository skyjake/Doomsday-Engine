/** @file manualconnectiondialog.h  Dialog for connecting to a server.
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

#ifndef DENG_CLIENT_MANUALCONNECTIONDIALOG_H
#define DENG_CLIENT_MANUALCONNECTIONDIALOG_H

#include <de/InputDialog>
#include <de/IPersistent>

/**
 * Dialog for connecting to a multiplayer server manually using an IP address or domain
 * name. The TCP port number can also be optionally provided.
 *
 * The dialog stores the previously used address persistently.
 */
class ManualConnectionDialog : public de::InputDialog, public de::IPersistent
{
    Q_OBJECT

public:
    ManualConnectionDialog(de::String const &name = "manualconnection");

    // Implements IPersistent.
    void operator >> (de::PersistentState &toState) const;
    void operator << (de::PersistentState const &fromState);

public slots:
    void validate();

protected:
    void finish(int result);

private:
    DENG2_PRIVATE(d)
};

#endif // DENG_CLIENT_MANUALCONNECTIONDIALOG_H
