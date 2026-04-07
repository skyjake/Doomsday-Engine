/** @file saveslots.cpp  Map of logical saved game session slots.
 *
 * @authors Copyright © 2003-2017 Jaakko Keränen <jaakko.keranen@iki.fi>
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

#include "common.h"
#include "saveslots.h"
#include "g_common.h"
#include "gamesession.h"
#include "hu_menu.h"
#include "menu/page.h"
#include "menu/widgets/lineeditwidget.h"

#include <doomsday/savegames.h>
#include <de/app.h>
#include <de/folder.h>
#include <de/logbuffer.h>
#include <de/observers.h>
#include <de/writer.h>
#include <de/loop.h>

#include <map>
#include <utility>

using namespace de;
using namespace common;
using namespace common::menu;

DE_PIMPL_NOREF(SaveSlots::Slot)
, DE_OBSERVES(GameStateFolder, MetadataChange)
{
    String id;
    bool userWritable;
    String savePath;
    int menuWidgetId;

    GameStateFolder *session; // Not owned.
    SessionStatus status;

    Impl()
        : userWritable(true)
        , menuWidgetId(0 /*none*/)
        , session     (0)
        , status      (Unused)
    {}

    void updateStatus()
    {
        LOGDEV_XVERBOSE("Updating SaveSlot '%s' status", id);
        status = Unused;
        if (session)
        {
            status = Incompatible;
            // Game identity key missmatch?
            if (!session->metadata().gets("gameIdentityKey", "").compareWithoutCase(gfw_GameId()))
            {
                /// @todo Validate loaded add-ons and checksum the definition database.
                status = Loadable; // It's good!
            }
        }

        // Update the menu widget(s) right away.
        updateMenuWidget("LoadGame");
        updateMenuWidget("SaveGame");
    }

    void updateMenuWidget(String const pageName)
    {
        if (!menuWidgetId) return;

        if (!Hu_MenuHasPage(pageName)) return; // Not initialized yet?

        Page &page = Hu_MenuPage(pageName);
        Widget *wi = page.tryFindWidget(menuWidgetId);
        if (!wi)
        {
            LOG_DEBUG("Failed locating menu widget with id ") << menuWidgetId;
            return;
        }
        LineEditWidget &edit = wi->as<LineEditWidget>();

        // In the Save menu, all slots are available for writing.
        wi->setFlags(Widget::Disabled, pageName == "LoadGame"? SetFlags : UnsetFlags);

        if (status == Loadable)
        {
            edit.setText(session->metadata().gets("userDescription", ""));
            wi->setFlags(Widget::Disabled, UnsetFlags);
        }
        else
        {
            edit.setText("");
        }

        if (Hu_MenuIsActive() && Hu_MenuPagePtr() == &page)
        {
            // Re-open the active page to update focus if necessary.
            Hu_MenuSetPage(&page, true);
        }
    }

    void gameStateFolderMetadataChanged(GameStateFolder &changed)
    {
        DE_ASSERT(&changed == session);
        DE_UNUSED(changed);

        updateStatus();
    }
};

SaveSlots::Slot::Slot(String id, bool userWritable, String saveName, int menuWidgetId)
    : d(new Impl)
{
    d->id           = id;
    d->userWritable = userWritable;
    d->menuWidgetId = menuWidgetId;
    d->savePath     = SaveGames::savePath() / saveName;
    if (d->savePath.fileNameExtension().isEmpty())
    {
        d->savePath += ".save";
    }

    // See if a saved session already exists for this slot.
    setGameStateFolder(App::rootFolder().tryLocate<GameStateFolder>(d->savePath));
}

SaveSlots::Slot::SessionStatus SaveSlots::Slot::sessionStatus() const
{
    return d->status;
}

bool SaveSlots::Slot::isUserWritable() const
{
    return d->userWritable;
}

const String &SaveSlots::Slot::id() const
{
    return d->id;
}

const String &SaveSlots::Slot::savePath() const
{
    return d->savePath;
}

void SaveSlots::Slot::bindSaveName(String newName)
{
    String newPath = SaveGames::savePath() / newName;
    if (newPath.fileNameExtension().isEmpty())
    {
        newPath += ".save";
    }

    if (d->savePath != newPath)
    {
        d->savePath = newPath;
        setGameStateFolder(App::rootFolder().tryLocate<GameStateFolder>(d->savePath));
    }
}

void SaveSlots::Slot::setGameStateFolder(GameStateFolder *newSession)
{
    if (d->session == newSession) return;

    // We want notification of subsequent changes so that we can update the session status
    // (and the menu, in turn).
    if (d->session)
    {
        d->session->audienceForMetadataChange() -= d;
    }

    d->session = newSession;
    d->updateStatus();

    if (d->session)
    {
        d->session->audienceForMetadataChange() += d;
    }

    // Should we announce this?
#if !defined DE_DEBUG // Always
    if (isUserWritable())
#endif
    {
        String statusText;
        if (d->session)
        {
            statusText = Stringf("associated with \"%s\"", d->session->path().c_str());
        }
        else
        {
            statusText = "unused";
        }
        LOG_VERBOSE("Save slot '%s' now %s") << d->id << statusText;
    }
}

void SaveSlots::Slot::updateStatus()
{
    d->updateStatus();
}

DE_PIMPL(SaveSlots)
, DE_OBSERVES(FileIndex, Addition)
, DE_OBSERVES(FileIndex, Removal)
{
    typedef std::map<String, Slot *> Slots;
    typedef std::pair<String, Slot *> SlotItem;
    Slots sslots;
    Dispatch dispatch;

    Impl(Public *i) : Base(i)
    {
        //GameSession::savedIndex().audienceForAvailabilityUpdate() += this;

        SaveGames::get().saveIndex().audienceForAddition() += this;
        SaveGames::get().saveIndex().audienceForRemoval()  += this;
    }

    ~Impl()
    {
        DE_FOR_EACH(Slots, i, sslots) { delete i->second; }
    }

    SaveSlot *slotById(const String &id)
    {
        Slots::const_iterator found = sslots.find(id);
        if (found != sslots.end())
        {
            return found->second;
        }
        return 0; // Not found.
    }

    SaveSlot *slotBySavePath(String path)
    {
        if (!path.isEmpty())
        {
            // Append the .save extension if non exists.
            if (path.fileNameExtension().isEmpty())
            {
                path += ".save";
            }

            DE_FOR_EACH_CONST(Slots, i, sslots)
            {
                if (!i->second->savePath().compareWithoutCase(path))
                {
                    return i->second;
                }
            }
        }
        return nullptr; // Not found.
    }

    void fileAdded(const File &saveFolder, const FileIndex &)
    {
        dispatch += [this, &saveFolder] ()
        {
            if (SaveSlot *sslot = slotBySavePath(saveFolder.path()))
            {
                sslot->setGameStateFolder(const_cast<GameStateFolder *>(&saveFolder.as<GameStateFolder>()));
            }
        };
    }

    void fileRemoved(const File &saveFolder, const FileIndex &)
    {
        DE_FOR_EACH_CONST(Slots, i, sslots)
        {
            SaveSlot *sslot = i->second;
            if (sslot->savePath() == saveFolder.path())
            {
                sslot->setGameStateFolder(nullptr);
            }
        }
    }

    void setAllIndexedSaves()
    {
        const auto &index = SaveGames::get().saveIndex();
        for (File *file : index.files())
        {
            fileAdded(*file, index);
        }
    }
};

SaveSlots::SaveSlots() : d(new Impl(this))
{}

void SaveSlots::add(const String &id, bool userWritable, const String &saveName, int menuWidgetId)
{
    // Ensure the slot identifier is unique.
    if (d->slotById(id)) return;

    // Insert a new save slot.
    d->sslots.insert(Impl::SlotItem(id, new Slot(id, userWritable, saveName, menuWidgetId)));
}

int SaveSlots::count() const
{
    return int(d->sslots.size());
}

bool SaveSlots::has(const String &id) const
{
    return d->slotById(id) != 0;
}

SaveSlots::Slot &SaveSlots::slot(const String &id) const
{
    if (SaveSlot *sslot = d->slotById(id))
    {
        return *sslot;
    }
    /// @throw MissingSlotError An invalid slot was specified.
    throw MissingSlotError("SaveSlots::slot", "Invalid slot id '" + id + "'");
}

SaveSlots::Slot *SaveSlots::slotBySaveName(const String &name) const
{
    return d->slotBySavePath(SaveGames::savePath() / name);
}

SaveSlots::Slot *SaveSlots::slotBySavedUserDescription(const String &description) const
{
    if (!description.isEmpty())
    {
        DE_FOR_EACH_CONST(Impl::Slots, i, d->sslots)
        {
            if (!gfw_Session()->savedUserDescription(i->second->saveName())
                                      .compareWithoutCase(description))
            {
                return i->second;
            }
        }
    }
    return 0; // Not found.
}

SaveSlots::Slot *SaveSlots::slotByUserInput(const String &str) const
{
    // Perhaps a user description of a saved session?
    if (Slot *sslot = slotBySavedUserDescription(str))
    {
        return sslot;
    }

    // Perhaps a saved session file name?
    if (Slot *sslot = slotBySaveName(str))
    {
        return sslot;
    }

    // Perhaps a unique slot identifier?
    String id = str;

    // Translate slot id mnemonics.
    if (!id.compareWithoutCase("last") || !id.compareWithoutCase("<last>"))
    {
        id = String::asText(Con_GetInteger("game-save-last-slot"));
    }
    else if (!id.compareWithoutCase("quick") || !id.compareWithoutCase("<quick>"))
    {
        id = String::asText(Con_GetInteger("game-save-quick-slot"));
    }

    return d->slotById(id);
}

void SaveSlots::updateAll()
{
    d->setAllIndexedSaves();
    DE_FOR_EACH(Impl::Slots, i, d->sslots)
    {
        i->second->updateStatus();
    }
}

namespace {
int cvarLastSlot;  ///< @c -1= Not yet loaded/saved in this game session.
int cvarQuickSlot; ///< @c -1= Not yet chosen/determined.
}

void SaveSlots::consoleRegister() // static
{
    cvarLastSlot  = -1;
    cvarQuickSlot = -1;
    C_VAR_INT("game-save-last-slot",    &cvarLastSlot,  CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, -1, 0);
    C_VAR_INT("game-save-quick-slot",   &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);
}
