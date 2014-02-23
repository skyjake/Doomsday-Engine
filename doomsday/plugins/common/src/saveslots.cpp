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

#include "p_saveio.h"
#include "saveinfo.h"
#include <de/NativePath>
#include <de/String>
#include <map>

#define MAX_HUB_MAPS 99

static int cvarLastSlot  = -1; ///< @c -1= Not yet loaded/saved in this game session.
static int cvarQuickSlot = -1; ///< @c -1= Not yet chosen/determined.

using namespace de;

DENG2_PIMPL_NOREF(SaveSlots::Slot)
{
    String id;
    bool userWritable;
    String fileName;
    SaveInfo *info;

    Instance() : userWritable(true), info(0) {}
    ~Instance() { delete info; }
};

SaveSlots::Slot::Slot(String id, bool userWritable, String const &fileName)
    : d(new Instance)
{
    d->id           = id;
    d->userWritable = userWritable;
    d->fileName     = fileName;
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
    if(d->fileName.compareWithoutCase(newName))
    {
        clearSaveInfo();
    }
    d->fileName = newName;
}

bool SaveSlots::Slot::isUsed() const
{
    if(SV_SavePath().isEmpty()) return false;
    if(!hasSaveInfo()) return false;
    return saveInfo().gameSessionIsLoadable();
}

bool SaveSlots::Slot::hasSaveInfo() const
{
    return d->info != 0;
}

void SaveSlots::Slot::clearSaveInfo()
{
    delete d->info; d->info = 0;
}

void SaveSlots::Slot::replaceSaveInfo(SaveInfo *newInfo)
{
    clearSaveInfo();
    d->info = newInfo;
}

SaveInfo &SaveSlots::Slot::saveInfo() const
{
    if(d->info)
    {
        return *d->info;
    }
    /// @throw MissingInfoError Attempted with no SaveInfo present.
    throw MissingInfoError("SaveSlots::Slot::saveInfo", "No SaveInfo exists");
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

    /// Re-build save info by re-scanning the save paths and populating the list.
    void buildInfosIfNeeded(bool update = false)
    {
        DENG2_FOR_EACH(Slots, i, sslots)
        {
            SaveSlot &sslot = *i->second;
            if(!sslot.hasSaveInfo())
            {
                sslot.replaceSaveInfo(new SaveInfo(sslot.fileName()));
            }
            if(update) sslot.saveInfo().updateFromFile();
        }
    }
};

SaveSlots::SaveSlots() : d(new Instance)
{}

void SaveSlots::addSlot(String id, bool userWritable, String fileName)
{
    // Ensure the slot identifier is unique.
    if(d->slotById(id)) return;

    // Insert a new save slot.
    d->sslots.insert(std::pair<String, Slot *>(id, new Slot(id, userWritable, fileName)));
}

void SaveSlots::clearAll()
{
    DENG2_FOR_EACH(Instance::Slots, i, d->sslots)
    {
        i->second->clearSaveInfo();
    }

    // Reset last-used and quick-save slot tracking.
    Con_SetInteger2("game-save-last-slot", -1, SVF_WRITE_OVERRIDE);
    Con_SetInteger("game-save-quick-slot", -1);
}

void SaveSlots::updateAll()
{
    d->buildInfosIfNeeded(true/*update session headers from source files*/);
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
    d->buildInfosIfNeeded();

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

    Slot &sslot = slot(slotId);
    if(!sslot.hasSaveInfo())
    {
        sslot.replaceSaveInfo(new SaveInfo(sslot.fileName()));
    }

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
        SV_RemoveFile(SV_SavePath() / saveInfo.fileNameForMap(i));
    }

    SV_RemoveFile(SV_SavePath() / saveInfo.fileName());

    saveInfo.setUserDescription("");
    saveInfo.setSessionId(0);
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
        SV_CopyFile(SV_SavePath() / sourceSlot.saveInfo().fileNameForMap(i),
                    SV_SavePath() / destSlot.saveInfo().fileNameForMap(i));
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
            if(!sslot.hasSaveInfo()) continue;

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
