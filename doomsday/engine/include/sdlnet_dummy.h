#ifndef DENG_SDLNET_DUMMY_H
#define DENG_SDLNET_DUMMY_H

typedef void* TCPsocket;
typedef struct {
    int host;
    short port;
} IPaddress;
typedef void* SDLNet_SocketSet;

static int SDLNet_Init(void) { return 0; }
static void SDLNet_Quit(void) {}
static TCPsocket SDLNet_TCP_Open(IPaddress* ip) { return 0; }
static void SDLNet_TCP_Close(TCPsocket sock) {}
static TCPsocket SDLNet_TCP_Accept(TCPsocket server) { return 0; }
static int SDLNet_TCP_Recv(TCPsocket sock, void* data, int maxlen) { return 0; }
static int SDLNet_TCP_Send(TCPsocket sock, const void* data, int len) { return 0; }
static IPaddress* SDLNet_TCP_GetPeerAddress(TCPsocket sock) { return 0; }
static int SDLNet_Read16(void* area) { return 0; }
static int SDLNet_Read32(void* area) { return 0; }
static SDLNet_SocketSet SDLNet_AllocSocketSet(int max) { return 0; }
static void SDLNet_FreeSocketSet(SDLNet_SocketSet set) {}
//static int SDLNet_AddSocket(SDLNet_SocketSet set, void* sock) { return 0; }
//static int SDLNet_DelSocket(SDLNet_SocketSet set, void* sock) { return 0; }
static int SDLNet_TCP_AddSocket(SDLNet_SocketSet set, TCPsocket sock) { return 0; }
static int SDLNet_TCP_DelSocket(SDLNet_SocketSet set, TCPsocket sock) { return 0; }
static int SDLNet_CheckSockets(SDLNet_SocketSet set, int timeout) { return 0; }
static int SDLNet_SocketReady(TCPsocket sock) { return 0; }
static int SDLNet_ResolveHost(IPaddress* a, const char* host, int port) { return 0; }
static const char* SDLNet_GetError() { return "dummy"; }

#endif
