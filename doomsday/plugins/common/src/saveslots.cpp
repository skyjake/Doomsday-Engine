/** @file saveslots.cpp  Map of logical saved game session slots.
 *
 * @authors Copyright © 2003-2014 Jaakko Keränen <jaakko.keranen@iki.fi>
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
#include <de/App>
#include <de/Folder>
#include <de/Observers>
#include <de/Writer>
#include <map>
#include <utility>

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

using namespace de;
using namespace common;
using namespace menu;

using de::game::SavedSession;

DENG2_PIMPL_NOREF(SaveSlots::Slot)
, DENG2_OBSERVES(SavedSession, MetadataChange)
{
    String id;
    bool userWritable;
    String savePath;
    int menuWidgetId;

    SavedSession *session; // Not owned.
    SessionStatus status;

    Instance()
        : userWritable(true)
        , menuWidgetId(0 /*none*/)
        , session     (0)
        , status      (Unused)
    {}

    ~Instance()
    {
        if(session)
        {
            session->audienceForMetadataChange() -= this;
        }
    }

    void updateStatus()
    {
        LOGDEV_XVERBOSE("Updating SaveSlot '%s' status") << id;
        status = Unused;
        if(session)
        {
            status = Incompatible;
            // Game identity key missmatch?
            if(!session->metadata().gets("gameIdentityKey").compareWithoutCase(COMMON_GAMESESSION->gameId()))
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
        if(!menuWidgetId) return;

        Page *page = Hu_MenuFindPageByName(pageName);
        if(!page) return; // Not initialized yet?

        Widget *wi = page->findObject(0, menuWidgetId);
        if(!wi)
        {
            LOG_DEBUG("Failed locating menu widget with id ") << menuWidgetId;
            return;
        }
        LineEditWidget &edit = wi->as<LineEditWidget>();

        wi->setFlags(FO_SET, MNF_DISABLED);
        if(status == Loadable)
        {
            edit.setText(MNEDIT_STF_NO_ACTION, session->metadata().gets("userDescription", "").toUtf8().constData());
            wi->setFlags(FO_CLEAR, MNF_DISABLED);
        }
        else
        {
            edit.setText(MNEDIT_STF_NO_ACTION, "");
        }

        if(Hu_MenuIsActive() && Hu_MenuActivePage() == page)
        {
            // Re-open the active page to update focus if necessary.
            Hu_MenuSetActivePage2(page, true);
        }
    }

    /// Observes SavedSession MetadataChange
    void savedSessionMetadataChanged(SavedSession &changed)
    {
        DENG2_ASSERT(&changed == session);
        DENG2_UNUSED(changed);
        updateStatus();
    }
};

SaveSlots::Slot::Slot(String id, bool userWritable, String saveName, int menuWidgetId)
    : d(new Instance())
{
    d->id           = id;
    d->userWritable = userWritable;
    d->menuWidgetId = menuWidgetId;
    d->savePath     = GameSession::savePath() / saveName;
    if(d->savePath.fileNameExtension().isEmpty())
    {
        d->savePath += ".save";
    }

    // See if a saved session already exists for this slot.
    setSavedSession(App::rootFolder().tryLocate<SavedSession>(d->savePath));
}

SaveSlots::Slot::SessionStatus SaveSlots::Slot::sessionStatus() const
{
    return d->status;
}

bool SaveSlots::Slot::isUserWritable() const
{
    return d->userWritable;
}

String const &SaveSlots::Slot::id() const
{
    return d->id;
}

String const &SaveSlots::Slot::savePath() const
{
    return d->savePath;
}

void SaveSlots::Slot::bindSaveName(String newName)
{
    String newPath = GameSession::savePath() / newName;
    if(newPath.fileNameExtension().isEmpty())
    {
        newPath += ".save";
    }

    if(d->savePath != newPath)
    {
        d->savePath = newPath;
        setSavedSession(App::rootFolder().tryLocate<SavedSession>(d->savePath));
    }
}

void SaveSlots::Slot::setSavedSession(SavedSession *newSession)
{
    if(d->session == newSession) return;

    // We want notification of subsequent changes so that we can update the session status
    // (and the menu, in turn).
    if(d->session)
    {
        d->session->audienceForMetadataChange() -= d;
    }

    d->session = newSession;
    d->updateStatus();

    if(d->session)
    {
        d->session->audienceForMetadataChange() += d;
    }

    // Should we announce this?
#if !defined DENG_DEBUG // Always
    if(isUserWritable())
#endif
    {
        String statusText;
        if(d->session)
        {
            statusText = String("associated with \"%1\"").arg(d->session->path());
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

DENG2_PIMPL(SaveSlots)
, DENG2_OBSERVES(GameSession::SavedIndex, AvailabilityUpdate)
{
    typedef std::map<String, Slot *> Slots;
    typedef std::pair<String, Slot *> SlotItem;
    Slots sslots;

    Instance(Public *i) : Base(i)
    {
        GameSession::savedIndex().audienceForAvailabilityUpdate() += this;
    }

    ~Instance()
    {
        GameSession::savedIndex().audienceForAvailabilityUpdate() -= this;
        DENG2_FOR_EACH(Slots, i, sslots) { delete i->second; }
    }

    SaveSlot *slotById(String const &id)
    {
        Slots::const_iterator found = sslots.find(id);
        if(found != sslots.end())
        {
            return found->second;
        }
        return 0; // Not found.
    }

    SaveSlot *slotBySavePath(String path)
    {
        if(!path.isEmpty())
        {
            // Append the .save extension if non exists.
            if(path.fileNameExtension().isEmpty())
            {
                path += ".save";
            }

            DENG2_FOR_EACH_CONST(Slots, i, sslots)
            {
                if(!i->second->savePath().compareWithoutCase(path))
                {
                    return i->second;
                }
            }
        }
        return 0; // Not found.
    }

    void savedIndexAvailabilityUpdate(GameSession::SavedIndex const &index)
    {
        DENG2_FOR_EACH_CONST(Slots, i, sslots)
        {
            SaveSlot *sslot = i->second;
            if(!index.find(sslot->savePath()))
            {
                sslot->setSavedSession(0);
            }
        }

        DENG2_FOR_EACH_CONST(GameSession::SavedIndex::All, i, index.all())
        {
            if(SaveSlot *sslot = slotBySavePath(i.key()))
            {
                sslot->setSavedSession(i.value());
            }
        }
    }
};

SaveSlots::SaveSlots() : d(new Instance(this))
{}

void SaveSlots::add(String const &id, bool userWritable, String const &saveName, int menuWidgetId)
{
    // Ensure the slot identifier is unique.
    if(d->slotById(id)) return;

    // Insert a new save slot.
    d->sslots.insert(Instance::SlotItem(id, new Slot(id, userWritable, saveName, menuWidgetId)));
}

int SaveSlots::count() const
{
    return int(d->sslots.size());
}

bool SaveSlots::has(String const &id) const
{
    return d->slotById(id) != 0;
}

SaveSlots::Slot &SaveSlots::slot(String const &id) const
{
    if(SaveSlot *sslot = d->slotById(id))
    {
        return *sslot;
    }
    /// @throw MissingSlotError An invalid slot was specified.
    throw MissingSlotError("SaveSlots::slot", "Invalid slot id '" + id + "'");
}

SaveSlots::Slot *SaveSlots::slotBySaveName(String const &name) const
{
    return d->slotBySavePath(GameSession::savePath() / name);
}

SaveSlots::Slot *SaveSlots::slotBySavedUserDescription(String const &description) const
{
    if(!description.isEmpty())
    {
        DENG2_FOR_EACH_CONST(Instance::Slots, i, d->sslots)
        {
            if(!COMMON_GAMESESSION->savedUserDescription(i->second->saveName())
                                      .compareWithoutCase(description))
            {
                return i->second;
            }
        }
    }
    return 0; // Not found.
}

SaveSlots::Slot *SaveSlots::slotByUserInput(String const &str) const
{
    // Perhaps a user description of a saved session?
    if(Slot *sslot = slotBySavedUserDescription(str))
    {
        return sslot;
    }

    // Perhaps a saved session file name?
    if(Slot *sslot = slotBySaveName(str))
    {
        return sslot;
    }

    // Perhaps a unique slot identifier?
    String id = str;

    // Translate slot id mnemonics.
    if(!id.compareWithoutCase("last") || !id.compareWithoutCase("<last>"))
    {
        id = String::number(Con_GetInteger("game-save-last-slot"));
    }
    else if(!id.compareWithoutCase("quick") || !id.compareWithoutCase("<quick>"))
    {
        id = String::number(Con_GetInteger("game-save-quick-slot"));
    }

    return d->slotById(id);
}

void SaveSlots::updateAll()
{
    DENG2_FOR_EACH(Instance::Slots, i, d->sslots)
    {
        i->second->updateStatus();
    }
}

void SaveSlots::consoleRegister() // static
{
    C_VAR_INT("game-save-last-slot",    &cvarLastSlot,  CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT("game-save-quick-slot",   &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);
}
