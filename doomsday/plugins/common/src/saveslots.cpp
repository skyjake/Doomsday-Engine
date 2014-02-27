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
#include "saveinfo.h"
#include <de/NativePath>
#include <de/Observers>
#include <de/String>
#include <map>

#define MAX_HUB_MAPS 99

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

using namespace de;

DENG2_PIMPL_NOREF(SaveSlots::Slot)
, DENG2_OBSERVES(SaveInfo, SessionStatusChange)
, DENG2_OBSERVES(SaveInfo, UserDescriptionChange)
{
    String id;
    bool userWritable;
    String fileName;
    int gameMenuWidgetId;
    SaveInfo *info;

    Instance()
        : userWritable    (true)
        , gameMenuWidgetId(0 /*none*/)
        , info            (0)
    {}

    ~Instance() { delete info; }

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

        mndata_edit_t *edit = (mndata_edit_t *)ob->_typedata;
        DENG2_ASSERT(edit != 0);

        MNObject_SetFlags(ob, FO_SET, MNF_DISABLED);
        if(info->gameSessionIsLoadable())
        {
            MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, info->userDescription().toUtf8().constData());
            MNObject_SetFlags(ob, FO_CLEAR, MNF_DISABLED);
        }
        else
        {
            MNEdit_SetText(ob, MNEDIT_STF_NO_ACTION, "");
        }

        if(Hu_MenuIsActive() && page == Hu_MenuActivePage())
        {
            // Re-open the active page to update focus if necessary.
            Hu_MenuSetActivePage2(page, true);
        }
    }

    /// Observes SaveInfo SessionStatusChange
    void saveInfoSessionStatusChanged(SaveInfo &saveInfo)
    {
        DENG2_ASSERT(&saveInfo == info);
        DENG2_UNUSED(saveInfo);
        updateGameMenuWidget();
    }

    /// Observes SaveInfo UserDescriptionChange
    void saveInfoUserDescriptionChanged(SaveInfo &saveInfo)
    {
        DENG2_ASSERT(&saveInfo == info);
        DENG2_UNUSED(saveInfo);
        updateGameMenuWidget();
    }
};

SaveSlots::Slot::Slot(String id, bool userWritable, String const &fileName, int gameMenuWidgetId)
    : d(new Instance)
{
    d->id               = id;
    d->userWritable     = userWritable;
    d->fileName         = fileName;
    d->gameMenuWidgetId = gameMenuWidgetId;

    replaceSaveInfo(new SaveInfo(d->fileName));
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
    DENG2_ASSERT(d->info != 0);
    d->info->setFileName(d->fileName = newName);
}

bool SaveSlots::Slot::isUsed() const
{
    if(!d->info) return false;
    return d->info->gameSessionIsLoadable();
}

void SaveSlots::Slot::replaceSaveInfo(SaveInfo *newInfo)
{
    DENG2_ASSERT(newInfo != 0);

    if(newInfo == d->info) return;

    delete d->info;
    d->info = newInfo;

    // Update the menu widget right away.
    d->updateGameMenuWidget();

    if(d->gameMenuWidgetId)
    {
        // We want notification of subsequent changes so that we can update the menu widget.
        d->info->audienceForSessionStatusChange   += d;
        d->info->audienceForUserDescriptionChange += d;
    }
}

SaveInfo &SaveSlots::Slot::saveInfo() const
{
    DENG2_ASSERT(d->info != 0);
    return *d->info;
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

    // Insert a new save slot.
    d->sslots.insert(std::pair<String, Slot *>(id, new Slot(id, userWritable, fileName, gameMenuWidgetId)));
}

void SaveSlots::updateAll()
{
    DENG2_FOR_EACH(Instance::Slots, i, d->sslots)
    {
        i->second->saveInfo().updateFromFile();
    }
}

int SaveSlots::slotCount() const
{
    return int(d->sslots.size());
}

bool SaveSlots::isKnownSlot(String value) const
{
    return d->slotById(value) != 0;
}

SaveSlots::Slot &SaveSlots::slot(String slotId) const
{
    if(SaveSlot *sslot = d->slotById(slotId))
    {
        return *sslot;
    }
    /// @throw InvalidSlotError An invalid slot was specified.
    throw InvalidSlotError("SaveSlots::slot", "Invalid slot id '" + slotId + "'");
}

void SaveSlots::clearSlot(String slotId)
{
    if(SV_SavePath().isEmpty())
    {
        return;
    }

    Slot &sslot        = slot(slotId);
    SaveInfo &saveInfo = sslot.saveInfo();

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
        SV_RemoveFile(SV_SavePath() / saveInfo.fileNameForMap(mapUri));
        Uri_Delete(mapUri);
    }

    SV_RemoveFile(SV_SavePath() / saveInfo.fileName());

    // Force a status update.
    /// @todo move clear logic into SaveInfo.
    saveInfo.updateFromFile();
}

void SaveSlots::copySlot(String sourceSlotId, String destSlotId)
{
    LOG_AS("SaveSlots::copySlot");

    if(SV_SavePath().isEmpty())
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
        SV_CopyFile(SV_SavePath() / sourceSlot.saveInfo().fileNameForMap(mapUri),
                    SV_SavePath() / destSlot.saveInfo().fileNameForMap(mapUri));
        Uri_Delete(mapUri);
    }

    SV_CopyFile(SV_SavePath() / sourceSlot.saveInfo().fileName(),
                SV_SavePath() / destSlot.saveInfo().fileName());

    // Copy save info too.
    destSlot.replaceSaveInfo(new SaveInfo(sourceSlot.saveInfo()));
    // Update the file path associated with the copied save info.
    destSlot.saveInfo().setFileName(destSlot.fileName());
}

String SaveSlots::findSlotWithUserSaveDescription(String description) const
{
    if(!description.isEmpty())
    {
        DENG2_FOR_EACH_CONST(Instance::Slots, i, d->sslots)
        {
            SaveSlot &sslot = *i->second;
            if(!sslot.saveInfo().userDescription().compareWithoutCase(description))
            {
                return sslot.id();
            }
        }
    }
    return ""; // Not found.
}

void SaveSlots::consoleRegister() // static
{
    C_VAR_INT("game-save-last-slot",    &cvarLastSlot,  CVF_NO_MIN|CVF_NO_MAX|CVF_NO_ARCHIVE|CVF_READ_ONLY, 0, 0);
    C_VAR_INT("game-save-quick-slot",   &cvarQuickSlot, CVF_NO_MAX|CVF_NO_ARCHIVE, -1, 0);
}
