//===========================================================================
// GL_PCX.H
//===========================================================================
#ifndef __DOOMSDAY_GRAPHICS_PCX_H__
#define __DOOMSDAY_GRAPHICS_PCX_H__

#pragma pack(1)
typedef struct
{
    char			manufacturer;
    char			version;
    char			encoding;
    char			bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char			reserved;
    char			color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char			filler[58];
    unsigned char	data;			// unbounded
} pcx_t;
#pragma pack()

int		PCX_MemoryGetSize(void *imageData, int *w, int *h);
int		PCX_GetSize(const char *fn, int *w, int *h);
int		PCX_MemoryLoad(byte *imgdata, int len, int buf_w, int buf_h, byte *outBuffer);
byte*	PCX_MemoryAllocLoad(byte *imgdata, int len, int *buf_w, int *buf_h, byte *outBuffer);
void	PCX_Load(const char *fn, int buf_w, int buf_h, byte *outBuffer);
byte*	PCX_AllocLoad(const char *fn, int *buf_w, int *buf_h, byte *outBuffer);

#endif 