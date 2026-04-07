#ifndef DOOMSDAY_API_CLIENT_H
#define DOOMSDAY_API_CLIENT_H

#include "apis.h"

struct mobj_s;

DE_API_TYPEDEF(Client)
{
    de_api_t api;

    /**
     * Searches through the client mobj hash table for the CURRENT map and
     * returns the clmobj with the specified ID, if that exists. Note that
     * client mobjs are also linked to the thinkers list.
     *
     * @param id  Mobj identifier.
     *
     * @return  Pointer to the mobj.
     */
    struct mobj_s *(*Mobj_Find)(thid_t id);

    /**
     * Enables or disables local action function execution on the client.
     *
     * @param mo  Client mobj.
     * @param enable  @c true to enable local actions, @c false to disable.
     */
    void (*Mobj_EnableLocalActions)(struct mobj_s *mo, dd_bool enable);

    /**
     * Determines if local action functions are enabled for client mobj @a mo.
     */
    dd_bool (*Mobj_LocalActionsEnabled)(struct mobj_s *mo);

    /**
     * Determines whether a client mobj is valid for playsim.
     * The primary reason for it not to be valid is that we haven't received
     * enough information about it yet.
     *
     * @param mo  Mobj to check.
     *
     * @return  @c true, if the mobj is a client mobj; otherwise @c false.
     */
    dd_bool (*Mobj_IsValid)(struct mobj_s *mo);

    /**
     * @param plrNum  Player number.
     *
     * @return  The engineside client mobj of a player, representing a remote mobj on the server.
     */
    struct mobj_s* (*Mobj_PlayerMobj)(int plrNum);
}
DE_API_T(Client);

#ifndef DE_NO_API_MACROS_CLIENT
#define ClMobj_Find                 _api_Client.Mobj_Find
#define ClMobj_EnableLocalActions   _api_Client.Mobj_EnableLocalActions
#define ClMobj_LocalActionsEnabled  _api_Client.Mobj_LocalActionsEnabled
#define ClMobj_IsValid              _api_Client.Mobj_IsValid
#define ClPlayer_ClMobj             _api_Client.Mobj_PlayerMobj
#endif

#ifdef __DOOMSDAY__
DE_USING_API(Client);
#endif

#endif // DOOMSDAY_API_CLIENT_H
