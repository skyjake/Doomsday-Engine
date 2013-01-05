#ifndef DOOMSDAY_API_INTERNALDATA_H
#define DOOMSDAY_API_INTERNALDATA_H

#include "apis.h"
#include "dd_share.h"

/**
 * The data exported out of the Doomsday engine.
 *
 * @deprecated Avoid this API.
 */
DENG_API_TYPEDEF(InternalData)
{
    de_api_t api;

    // Data arrays.
    mobjinfo_t **   mobjInfo;
    state_t **      states;
    sprname_t **    sprNames;
    ddtext_t **     text;

    // General information.
    int *           validCount;
}
DENG_API_T(InternalData);

#ifdef __DOOMSDAY__
DENG_USING_API(InternalData);
#endif

#endif // DOOMSDAY_API_INTERNALDATA_H
