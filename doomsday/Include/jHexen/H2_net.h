#ifndef __JHEXEN_NETWORK_H__
#define __JHEXEN_NETWORK_H__

typedef struct
{
	unsigned char	class : 4;
	unsigned char	color : 4;
} plrdata_t;

int H2_NetServerOpen(int before);
int H2_NetServerClose(int before);
int H2_NetServerStarted(int before);
int H2_NetConnect(int before);
int H2_NetDisconnect(int before);
int H2_NetPlayerEvent(int plrNumber, int peType, void *data);

#endif