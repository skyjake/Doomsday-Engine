#ifndef __DD_SAVEGAME_TEXTURE_ARCHIVE_H__
#define __DD_SAVEGAME_TEXTURE_ARCHIVE_H__

#define MAX_ARCHIVED_TEXTURES	1024

typedef struct
{
	char name[9];
} texentry_t;

typedef struct
{
	texentry_t table[MAX_ARCHIVED_TEXTURES];
	int count;
} texarchive_t;

extern texarchive_t flat_archive;
extern texarchive_t tex_archive;

void SV_InitTextureArchives(void);
unsigned short SV_TextureArchiveNum(int texnum);
unsigned short SV_FlatArchiveNum(int flatnum);
int SV_GetArchiveFlat(int archivenum);
int SV_GetArchiveTexture(int archivenum);
void SV_WriteTextureArchive(void);
void SV_ReadTextureArchive(void);

#endif
