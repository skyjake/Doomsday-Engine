#ifndef DOOMSDAY_API_CONSOLE_H
#define DOOMSDAY_API_CONSOLE_H

#include "api_base.h"

DENG_API_TYPEDEF(Con)
{
    de_api_t api;

    void (*Message)(char const* message, ...);
}
DENG_API_T(Con);

#ifndef DENG_NO_API_MACROS_CONSOLE
#define Con_Message _api_Con.Message
#endif

#ifdef __DOOMSDAY__
DENG_USING_API(Con);
#endif

#endif // DOOMSDAY_API_CONSOLE_H
