#ifndef DOOMSDAY_API_SERVER_H
#define DOOMSDAY_API_SERVER_H

#include "apis.h"

DENG_API_TYPEDEF(Server)
{
    de_api_t api;

    /**
     * Determines whether the coordinates sent by a player are valid at the
     * moment.
     */
    boolean (*CanTrustClientPos)(int player);
}
DENG_API_T(Server);

#ifndef DENG_NO_API_MACROS_SERVER
#define Sv_CanTrustClientPos    _api_Server.CanTrustClientPos
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Server);
#endif

#endif // DOOMSDAY_API_SERVER_H
