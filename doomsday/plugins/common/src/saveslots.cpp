/** @file saveslots.cpp  Map of logical game save slots.
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2005-2013 Daniel Swanson <danij@dengine.net>
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
#include "hu_menu.h"
#include <de/game/SavedSessionRepository>
#include <de/Observers>
#include <map>

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

using namespace de;
using de::game::SavedSession;
using de::game::SavedSessionRepository;

DENG2_PIMPL_NOREF(SaveSlots::Slot)
, DENG2_OBSERVES(SavedSession, MetadataChange)
{
    String id;
    bool userWritable;
    String repoPath;
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
        if(session && session->hasFile())
        {
            status = Incompatible;
            // Game identity key missmatch?
            if(!session->metadata().gets("gameIdentityKey", "").compareWithoutCase(G_IdentityKey()))
            {
                /// @todo Validate loaded add-ons and checksum the definition database.
                status = Loadable; // It's good!
            }
        }

        // Update the menu widget right away.
        updateMenuWidget();
    }

    void updateMenuWidget()
    {
        if(!menuWidgetId) return;

        mn_page_t *page = Hu_MenuFindPageByName("LoadGame");
        if(!page) return; // Not initialized yet?

        mn_object_t *ob = MNPage_FindObject(page, 0, menuWidgetId);
        if(!ob)
        {
            LOG_DEBUG("Failed locating menu widget with id ") << menuWidgetId;
            return;
        }
        DENG2_ASSERT(ob->_type == MN_EDIT);

        MNObject_SetFlags(ob, FO_SET, MNF_DISABLED);
        if(status == Loadable)
        {
            MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, session->metadata().gets("userDescription", "").toUtf8().constData());
            MNObject_SetFlags(ob, FO_CLEAR, MNF_DISABLED);
        }
        else
        {
            MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, "");
        }

        if(Hu_MenuIsActive() &&
           (Hu_MenuActivePage() == page || Hu_MenuActivePage() == Hu_MenuFindPageByName("SaveGame")))
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

SaveSlots::Slot::Slot(String id, bool userWritable, String repoPath, int menuWidgetId)
    : d(new Instance())
{
    d->id           = id;
    d->userWritable = userWritable;
    d->repoPath     = repoPath;
    d->menuWidgetId = menuWidgetId;

    // See if a saved session already exists for this slot.
    if(G_SavedSessionRepository().has(d->repoPath))
    {
        setSavedSession(&G_SavedSessionRepository().find(d->repoPath));
    }
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

String const &SaveSlots::Slot::repositoryPath() const
{
    return d->repoPath;
}

void SaveSlots::Slot::bindRepositoryPath(String newPath)
{
    if(d->repoPath != newPath)
    {
        d->repoPath = newPath;
        setSavedSession(G_SavedSessionRepository().findPtr(d->repoPath));
    }
}

bool SaveSlots::Slot::hasSavedSession() const
{
    return d->session != 0;
}

SavedSession &SaveSlots::Slot::savedSession() const
{
    if(d->session)
    {
        return *d->session;
    }
    throw MissingSessionError("SaveSlots::Slot::savedSession", "No saved session exists");
}

void SaveSlots::Slot::setSavedSession(SavedSession *newSession)
{
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
}

void SaveSlots::Slot::clear()
{
    // Should we announce this?
#if !defined DENG_DEBUG // Always
    if(isUserWritable())
#endif
    {
        App_Log(DE2_RES_MSG, "Clearing save slot '%s'", d->id.toLatin1().constData());
    }

    if(d->session)
    {
        d->session->removeFile();
        setSavedSession(0);
    }
}

DENG2_PIMPL(SaveSlots)
, DENG2_OBSERVES(SavedSessionRepository, AvailabilityUpdate)
{
    typedef std::map<String, Slot *> Slots;
    typedef std::pair<String, Slot *> SlotItem;
    Slots sslots;

    Instance(Public *i) : Base(i)
    {
        G_SavedSessionRepository().audienceForAvailabilityUpdate() += this;
    }

    ~Instance()
    {
        G_SavedSessionRepository().audienceForAvailabilityUpdate() -= this;
        DENG2_FOR_EACH(Slots, i, sslots) { delete i->second; }
    }

    SaveSlot *slotById(String id)
    {
        Slots::const_iterator found = sslots.find(id);
        if(found != sslots.end())
        {
            return found->second;
        }
        return 0; // Not found.
    }

    SaveSlot *slotByRepoPath(String path)
    {
        DENG2_FOR_EACH(Slots, i, sslots)
        {
            if(!i->second->repositoryPath().compareWithoutCase(path))
            {
                return i->second;
            }
        }
        return 0; // Not found.
    }

    void repositoryAvailabilityUpdate(SavedSessionRepository const &repo)
    {
        DENG2_FOR_EACH(Slots, i, sslots)
        {
            SaveSlot *sslot = i->second;
            if(!repo.has(sslot->repositoryPath()))
            {
                sslot->setSavedSession(0);
            }
        }

        DENG2_FOR_EACH_CONST(SavedSessionRepository::All, i, repo.all())
        {
            if(SaveSlot *sslot = slotByRepoPath(i->first))
            {
                sslot->setSavedSession(i->second);
            }
        }
    }
};

SaveSlots::SaveSlots() : d(new Instance(this))
{}

void SaveSlots::add(String id, bool userWritable, String repoPath, int menuWidgetId)
{
    // Ensure the slot identifier is unique.
    if(d->slotById(id)) return;

    // Insert a new save slot.
    d->sslots.insert(Instance::SlotItem(id, new Slot(id, userWritable, repoPath, menuWidgetId)));
}

int SaveSlots::count() const
{
    return int(d->sslots.size());
}

bool SaveSlots::has(String value) const
{
    return d->slotById(value) != 0;
}

SaveSlots::Slot &SaveSlots::slot(String id) const
{
    if(SaveSlot *sslot = d->slotById(id))
    {
        return *sslot;
    }
    /// @throw MissingSlotError An invalid slot was specified.
    throw MissingSlotError("SaveSlots::slot", "Invalid slot id '" + id + "'");
}

SaveSlots::Slot *SaveSlots::slot(SavedSession const *session) const
{
    if(session)
    {
        String const repoPath = session->path().fileNameAndPathWithoutExtension();
        DENG2_FOR_EACH_CONST(Instance::Slots, i, d->sslots)
        {
            if(!i->second->repositoryPath().compareWithoutCase(repoPath))
            {
                return i->second;
            }
        }
    }
    return 0; // Not found.
}

void SaveSlots::copySavedSessionFile(String sourceId, String destId)
{
    LOG_AS("SaveSlots::copySavedSessionFile");

    Slot &source = slot(sourceId);
    Slot &dest   = slot(destId);

    // Sanity check.
    if(&source == &dest) return;

    // Clear the saved file package for the destination slot.
    dest.clear();

    if(source.savedSession().hasFile())
    {
        // Copy the saved file package to the destination slot.
        dest.savedSession().copyFile(source.savedSession());
    }

    // Copy the session too.
    dest.setSavedSession(new SavedSession(source.savedSession()));
    // Update the repository path associated with the copied session.
    dest.savedSession().setPath(dest.repositoryPath());
}

void SaveSlots::consoleRegister() // static
{
    C_VAR_INT("game-save-last-slot",    &cvarLastSlot,  CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT("game-save-quick-slot",   &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);
}
