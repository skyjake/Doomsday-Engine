/* $Id$
 * Socket handling.
 */
#ifndef __DOOMSDAY_SYSTEM_SOCKET_H__
#define __DOOMSDAY_SYSTEM_SOCKET_H__

typedef unsigned int socket_t;

void		N_SockInit(void);
void		N_SockShutdown(void);
void		N_SockPrintf(socket_t s, const char *format, ...);
struct hostent *N_SockGetHost(const char *hostName);
socket_t	N_SockNewStream(void);
boolean		N_SockConnect(socket_t s, struct hostent *host, unsigned short port);
void		N_SockClose(socket_t s);

#endif 