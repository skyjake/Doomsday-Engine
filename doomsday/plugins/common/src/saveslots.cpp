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
#include "p_saveio.h"
#include "savedsessionrepository.h"
#include <de/NativePath>
#include <de/Observers>
#include <de/String>
#include <map>

#define MAX_HUB_MAPS 99

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

using namespace de;

DENG2_PIMPL(SaveSlots::Slot)
, DENG2_OBSERVES(SessionRecord, SessionStatusChange)
, DENG2_OBSERVES(SessionRecord, UserDescriptionChange)
{
    String id;
    bool userWritable;
    String fileName;
    int gameMenuWidgetId;

    Instance(Public *i)
        : Base(i)
        , userWritable    (true)
        , gameMenuWidgetId(0 /*none*/)
    {}

    void updateGameMenuWidget()
    {
        if(!gameMenuWidgetId) return;

        mn_page_t *page = Hu_MenuFindPageByName("LoadGame");
        if(!page) return; // Not initialized yet?

        mn_object_t *ob = MNPage_FindObject(page, 0, gameMenuWidgetId);
        if(!ob)
        {
            LOG_DEBUG("Failed locating menu widget with id ") << gameMenuWidgetId;
            return;
        }
        DENG2_ASSERT(ob->_type == MN_EDIT);

        MNObject_SetFlags(ob, FO_SET, MNF_DISABLED);
        SessionRecord &record = self.sessionRecord();
        if(record.gameSessionIsLoadable())
        {
            MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, record.meta().userDescription.toUtf8().constData());
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

    /// Observes SessionRecord SessionStatusChange
    void sessionRecordStatusChanged(SessionRecord &record)
    {
        DENG2_ASSERT(&record == &self.sessionRecord());
        DENG2_UNUSED(record);
        updateGameMenuWidget();
    }

    /// Observes SessionRecord UserDescriptionChange
    void sessionRecordUserDescriptionChanged(SessionRecord &record)
    {
        DENG2_ASSERT(&record == &self.sessionRecord());
        DENG2_UNUSED(record);
        updateGameMenuWidget();
    }
};

SaveSlots::Slot::Slot(String id, bool userWritable, String const &fileName, int gameMenuWidgetId)
    : d(new Instance(this))
{
    d->id               = id;
    d->userWritable     = userWritable;
    d->fileName         = fileName;
    d->gameMenuWidgetId = gameMenuWidgetId;

    replaceSessionRecord(G_SavedSessionRepository().newRecord(d->fileName));
}

String const &SaveSlots::Slot::id() const
{
    return d->id;
}

bool SaveSlots::Slot::isUserWritable() const
{
    return d->userWritable;
}

String const &SaveSlots::Slot::fileName() const
{
    return d->fileName;
}

void SaveSlots::Slot::bindFileName(String newName)
{
    sessionRecord().setFileName(d->fileName = newName);
}

bool SaveSlots::Slot::isUsed() const
{
    if(!G_SavedSessionRepository().hasRecord(d->fileName)) return false;
    return sessionRecord().gameSessionIsLoadable();
}

SessionMetadata const &SaveSlots::Slot::saveMetadata() const
{
    return sessionRecord().meta();
}

void SaveSlots::Slot::replaceSessionRecord(SessionRecord *newRecord)
{
    DENG2_ASSERT(newRecord != 0);

    G_SavedSessionRepository().replaceRecord(d->fileName, newRecord);
    SessionRecord &record = sessionRecord();

    // Update the menu widget right away.
    d->updateGameMenuWidget();

    if(d->gameMenuWidgetId)
    {
        // We want notification of subsequent changes so that we can update the menu widget.
        record.audienceForSessionStatusChange   += d;
        record.audienceForUserDescriptionChange += d;
    }
}

SessionRecord &SaveSlots::Slot::sessionRecord() const
{
    return G_SavedSessionRepository().record(d->fileName);
}

DENG2_PIMPL_NOREF(SaveSlots)
{
    typedef std::map<String, Slot *> Slots;
    Slots sslots;

    Instance() {}
    ~Instance() { DENG2_FOR_EACH(Slots, i, sslots) { delete i->second; } }

    SaveSlot *slotById(String id)
    {
        Slots::const_iterator found = sslots.find(id);
        if(found != sslots.end())
        {
            return found->second;
        }
        return 0; // Not found.
    }
};

SaveSlots::SaveSlots() : d(new Instance)
{}

void SaveSlots::addSlot(String id, bool userWritable, String fileName, int gameMenuWidgetId)
{
    // Ensure the slot identifier is unique.
    if(d->slotById(id)) return;

    // Also add an empty record to the saved session repository.
    /// @todo the engine should do this itself by scanning the saved game directory.
    G_SavedSessionRepository().addRecord(fileName);

    // Insert a new save slot.
    d->sslots.insert(std::pair<String, Slot *>(id, new Slot(id, userWritable, fileName, gameMenuWidgetId)));
}

void SaveSlots::updateAll()
{
    DENG2_FOR_EACH(Instance::Slots, i, d->sslots)
    {
        i->second->sessionRecord().updateFromFile();
    }
}

int SaveSlots::slotCount() const
{
    return int(d->sslots.size());
}

bool SaveSlots::hasSlot(String value) const
{
    return d->slotById(value) != 0;
}

SaveSlots::Slot &SaveSlots::slot(String slotId) const
{
    if(SaveSlot *sslot = d->slotById(slotId))
    {
        return *sslot;
    }
    /// @throw MissingSlotError An invalid slot was specified.
    throw MissingSlotError("SaveSlots::slot", "Invalid slot id '" + slotId + "'");
}

void SaveSlots::clearSlot(String slotId)
{
    SavedSessionRepository &saveRepo = G_SavedSessionRepository();

    if(saveRepo.savePath().isEmpty())
    {
        return;
    }

    Slot &sslot = slot(slotId);

    // Should we announce this?
#if !defined DENG_DEBUG // Always
    if(sslot.isUserWritable())
#endif
    {
        App_Log(DE2_RES_MSG, "Clearing save slot '%s'", slotId.toLatin1().constData());
    }

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        Uri *mapUri = G_ComposeMapUri(gameEpisode, i);
        SV_RemoveFile(saveRepo.savePath() / sslot.sessionRecord().fileNameForMap(mapUri));
        Uri_Delete(mapUri);
    }

    SV_RemoveFile(saveRepo.savePath() / sslot.sessionRecord().fileName());

    // Force a status update.
    /// @todo move clear logic into SaveInfo.
    sslot.sessionRecord().updateFromFile();
}

void SaveSlots::copySlot(String sourceSlotId, String destSlotId)
{
    LOG_AS("SaveSlots::copySlot");

    SavedSessionRepository &saveRepo = G_SavedSessionRepository();

    if(saveRepo.savePath().isEmpty())
    {
        return;
    }

    Slot &sourceSlot = slot(sourceSlotId);
    Slot &destSlot   = slot(destSlotId);

    // Sanity check.
    if(&sourceSlot == &destSlot)
    {
        return;
    }

    // Clear all save files at destination slot.
    clearSlot(destSlotId);

    for(int i = 0; i < MAX_HUB_MAPS; ++i)
    {
        Uri *mapUri = G_ComposeMapUri(gameEpisode, i);
        SV_CopyFile(saveRepo.savePath() / sourceSlot.sessionRecord().fileNameForMap(mapUri),
                    saveRepo.savePath() / destSlot.sessionRecord().fileNameForMap(mapUri));
        Uri_Delete(mapUri);
    }

    SV_CopyFile(saveRepo.savePath() / sourceSlot.sessionRecord().fileName(),
                saveRepo.savePath() / destSlot.sessionRecord().fileName());

    // Copy save info too.
    destSlot.replaceSessionRecord(new SessionRecord(sourceSlot.sessionRecord()));
    // Update the file path associated with the copied save info.
    destSlot.sessionRecord().setFileName(destSlot.fileName());
}

SaveSlots::Slot *SaveSlots::slotByUserDescription(String description) const
{
    if(!description.isEmpty())
    {
        DENG2_FOR_EACH_CONST(Instance::Slots, i, d->sslots)
        {
            SaveSlot &sslot = *i->second;
            if(!sslot.isUsed()) continue;

            if(!sslot.saveMetadata().userDescription.compareWithoutCase(description))
            {
                return &sslot;
            }
        }
    }
    return 0; // Not found.
}

void SaveSlots::consoleRegister() // static
{
    C_VAR_INT("game-save-last-slot",    &cvarLastSlot,  CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT("game-save-quick-slot",   &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);
}
