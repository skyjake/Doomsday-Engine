/* $Id$
 * The master server maintains a list of running public servers.
 */
#ifndef __DOOMSDAY_SYSTEM_MASTER_H__
#define __DOOMSDAY_SYSTEM_MASTER_H__

#include "dd_share.h"

void	N_MasterInit(void);
void	N_MasterShutdown(void);
void	N_MasterAnnounceServer(boolean isOpen);
void	N_MasterRequestList(void);
int		N_MasterGet(int index, serverinfo_t *info);

#endif 