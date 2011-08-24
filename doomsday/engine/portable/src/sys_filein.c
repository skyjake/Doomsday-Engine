/**\file sys_filein.c
 *\section License
 * License: GPL
 * Online License Link: http://www.gnu.org/licenses/gpl.html
 *
 *\author Copyright © 2003-2011 Jaakko Keränen <jaakko.keranen@iki.fi>
 *\author Copyright © 2006-2011 Daniel Swanson <danij@dengine.net>
 *\author Copyright © 2010 Jamie Jones <jamie_jones_au@yahoo.com.au>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */

#include <ctype.h>
#include <time.h>
#include <sys/stat.h>

#include "de_platform.h"
#include "de_base.h"
#include "de_console.h"
#include "de_filesys.h"

#include "m_md5.h"

typedef struct {
    DFILE* file;
} filehandle_t;

#define FILEIDENTIFIERID_T_MAXLEN 16
#define FILEIDENTIFIERID_T_LASTINDEX 15
typedef byte fileidentifierid_t[FILEIDENTIFIERID_T_MAXLEN];

typedef struct fileidentifier_s {
    fileidentifierid_t hash;
} fileidentifier_t;

static filehandle_t* files;
static uint filesCount;

static uint numReadFiles = 0;
static uint maxReadFiles = 0;
static fileidentifier_t* readFiles = 0;

static fileidentifier_t* findFileIdentifierForId(fileidentifierid_t id)
{
    uint i = 0;
    while(i < numReadFiles)
    {
        if(!memcmp(readFiles[i].hash, id, FILEIDENTIFIERID_T_LASTINDEX))
            return &readFiles[i];
        ++i;
    }
    return 0;
}

static filehandle_t* findUsedFileHandle(void)
{
    uint i;
    for(i = 0; i < filesCount; ++i)
    {
        filehandle_t* fhdl = &files[i];
        if(!fhdl->file)
            return fhdl;
    }
    return 0;
}

static filehandle_t* getFileHandle(void)
{
    filehandle_t* fhdl = findUsedFileHandle();
    if(!fhdl)
    {
        uint firstNewFile = filesCount;

        filesCount *= 2;
        if(filesCount < 16)
            filesCount = 16;

        // Allocate more memory.
        files = (filehandle_t*)realloc(files, sizeof(*files) * filesCount);
        if(!files)
            Con_Error("getFileHandle: Failed on (re)allocation of %lu bytes for file list.",
                (unsigned long) (sizeof(*files) * filesCount));

        // Clear the new handles.
        memset(files + firstNewFile, 0, sizeof(*files) * (filesCount - firstNewFile));

        fhdl = files + firstNewFile;
    }
    return fhdl;
}

static DFILE* getFreeFile(void)
{
    filehandle_t* fhdl = getFileHandle();
    fhdl->file = (DFILE*)calloc(1, sizeof(*fhdl->file));
    if(!fhdl)
        Con_Error("getFreeFile: Failed on allocation of %lu bytes for new DFILE.", (unsigned long) sizeof(*fhdl->file));
    return fhdl->file;
}

void F_GenerateFileId(const char* str, byte identifier[16])
{
    md5_ctx_t context;
    ddstring_t absPath;

    // First normalize the name.
    Str_Init(&absPath); Str_Set(&absPath, str);
    F_MakeAbsolute(&absPath, &absPath);
    F_FixSlashes(&absPath, &absPath);

#if defined(WIN32) || defined(MACOSX)
    // This is a case insensitive operation.
    strupr(Str_Text(&absPath));
#endif

    md5_init(&context);
    md5_update(&context, (byte*) Str_Text(&absPath), (unsigned int) Str_Length(&absPath));
    md5_final(&context, identifier);

    Str_Free(&absPath);
}

boolean F_CheckFileId(const char* path)
{
    assert(path);
    {
    fileidentifierid_t id;

    if(!F_Access(path))
        return false;

    // Calculate the identifier.
    F_GenerateFileId(path, id);

    if(findFileIdentifierForId(id))
        return false;

    // Allocate a new entry.
    numReadFiles++;
    if(numReadFiles > maxReadFiles)
    {
        if(!maxReadFiles)
            maxReadFiles = 16;
        else
            maxReadFiles *= 2;

        readFiles = realloc(readFiles, sizeof(*readFiles) * maxReadFiles);
        memset(readFiles + numReadFiles, 0, sizeof(*readFiles) * (maxReadFiles - numReadFiles));
    }

    memcpy(readFiles[numReadFiles - 1].hash, id, sizeof(id));
    return true;
    }
}

boolean F_ReleaseFileId(const char* path)
{
    fileidentifierid_t id;
    fileidentifier_t* fileIdentifier;

    F_GenerateFileId(path, id);

    fileIdentifier = findFileIdentifierForId(id);
    if(fileIdentifier != 0)
    {
        size_t index = fileIdentifier - readFiles;
        if(index < numReadFiles)
            memmove(readFiles + index, readFiles + index + 1, numReadFiles - index - 1);
        memset(readFiles + numReadFiles, 0, sizeof(*readFiles));
        --numReadFiles;
        return true;
    }
    return false;
}

void F_ResetFileIds(void)
{
    numReadFiles = 0;
}

void F_CloseAll(void)
{
    if(files)
    {
        uint i;
        for(i = 0; i < filesCount; ++i)
        {
            if(files[i].file)
                F_Close(files[i].file);
        }
        free(files); files = 0;
    }
    filesCount = 0;
}

int F_MatchFileName(const char* string, const char* pattern)
{
    const char* in = string, *st = pattern;

    while(*in)
    {
        if(*st == '*')
        {
            st++;
            continue;
        }

        if(*st != '?' && (tolower((unsigned char) *st) != tolower((unsigned char) *in)))
        {
            // A mismatch. Hmm. Go back to a previous '*'.
            while(st >= pattern && *st != '*')
                st--;
            if(st < pattern)
                return false; // No match!
            // The asterisk lets us continue.
        }

        // This character of the pattern is OK.
        st++;
        in++;
    }

    // Match is good if the end of the pattern was reached.
    while(*st == '*')
        st++; // Skip remaining asterisks.

    return *st == 0;
}

int F_Access(const char* path)
{
    // Open for reading, but don't buffer anything.
    DFILE* file = F_Open(path, "rx");
    if(file)
    {
        F_Close(file);
        return true;
    }
    return false;
}

size_t F_Length(DFILE* file)
{
    assert(NULL != file);
    {
    size_t currentPosition = F_Seek(file, 0, SEEK_END);
    size_t length = F_Tell(file);
    F_Seek(file, currentPosition, SEEK_SET);
    return length;
    }
}

unsigned int F_LastModified(DFILE* file)
{
    assert(NULL != file);
    return file->lastModified;
}

void F_Release(DFILE* file)
{
    assert(NULL != file);

    if(files)
    {   // Clear references to the handle.
        uint i;
        for(i = 0; i < filesCount; ++i)
        {
            if(files[i].file == file)
                files[i].file = NULL;
        }
    }

    // File was allocated in getFreeFile.
    free(file);
}

/// \note This only works on real files.
static unsigned int readLastModified(const char* path)
{
#ifdef UNIX
    struct stat s;
    stat(path, &s);
    return s.st_mtime;
#endif

#ifdef WIN32
    struct _stat s;
    _stat(path, &s);
    return s.st_mtime;
#endif
}

DFILE* F_OpenLump(lumpnum_t lumpNum, boolean dontBuffer)
{
    DFILE* file = getFreeFile();

    if(!file)
        return NULL;

    // Init and load in the lump data.
    file->flags.open = true;
    file->flags.file = false;
    file->lastModified = W_LumpLastModified(lumpNum);
    if(!dontBuffer)
    {
        file->size = W_LumpLength(lumpNum);
        file->pos = file->data = (void*) malloc(file->size);
        if(NULL == file->data)
            Con_Error("F_OpenLump: Failed on allocation of %lu bytes for buffered data.",
                (unsigned long) file->size);
#if _DEBUG
        VERBOSE2( Con_Printf("Next FILE read from F_OpenLump.\n") )
#endif
        W_ReadLump(lumpNum, (char*)file->data);
    }

    return file;
}

DFILE* F_OpenStreamLump(abstractfile_t* fsObject, int lumpIdx, boolean dontBuffer)
{
    const lumpinfo_t* info = F_LumpInfo(fsObject, lumpIdx);
    DFILE* file;

    if(!info) return NULL;
    file = getFreeFile();
    if(!file) return NULL;

    // Init and load in the lump data.
    file->flags.open = true;
    file->flags.file = false;
    file->lastModified = info->lastModified;
    if(!dontBuffer)
    {
        file->size = info->size;
        file->pos = file->data = (char*)malloc(file->size);
        if(NULL == file->data)
            Con_Error("F_OpenZip: Failed on allocation of %lu bytes for buffered data.",
                (unsigned long) file->size);
#if _DEBUG
        VERBOSE2( Con_Printf("Next FILE read from F_OpenStreamLump.\n") )
#endif
        F_ReadLumpSection(fsObject, lumpIdx, (uint8_t*)file->data, 0, info->size);
    }
    return file;
}

DFILE* F_OpenStreamFile(FILE* hndl, const char* path)
{
    assert(hndl && path && path[0]);
    { DFILE* file = getFreeFile();
    file->data = (void*)hndl;
    file->flags.open = true;
    file->flags.file = true;
    file->lastModified = readLastModified(path);
    return file;}
}

void F_Close(DFILE* file)
{
    assert(NULL != file);

    if(!file->flags.open)
        return;

    if(file->flags.file)
    {
        fclose(file->data);
    }
    else
    {   // Free the stored data.
        if(file->data)
            free(file->data);
    }
    memset(file, 0, sizeof(*file));

    F_Release(file);
}

size_t F_Read(DFILE* file, void* dest, size_t count)
{
    assert(NULL != file);
    {
    size_t bytesleft;

    if(!file->flags.open)
        return 0;

    if(file->flags.file)
    {   // Normal file.
        count = fread(dest, 1, count, file->data);
        if(feof((FILE*) file->data))
            file->flags.eof = true;
        return count;
    }

    // Is there enough room in the file?
    bytesleft = file->size - (file->pos - (char*) file->data);
    if(count > bytesleft)
    {
        count = bytesleft;
        file->flags.eof = true;
    }

    if(count)
    {
        memcpy(dest, file->pos, count);
        file->pos += count;
    }

    return count;
    }
}

unsigned char F_GetC(DFILE* file)
{
    assert(NULL != file);
    if(file->flags.open)
    {
        unsigned char ch = 0;
        F_Read(file, &ch, 1);
        return ch;
    }
    return 0;
}

size_t F_Tell(DFILE* file)
{
    assert(NULL != file);
    if(!file->flags.open)
        return 0;
    if(file->flags.file)
        return (size_t) ftell(file->data);
    return file->pos - (char*) file->data;
}

size_t F_Seek(DFILE* file, size_t offset, int whence)
{
    assert(NULL != file);
    {
    size_t oldpos = F_Tell(file);

    if(!file->flags.open)
        return 0;

    file->flags.eof = false;
    if(file->flags.file)
    {
        fseek(file->data, (long) offset, whence);
    }
    else
    {
        if(whence == SEEK_SET)
            file->pos = (char *) file->data + offset;
        else if(whence == SEEK_END)
            file->pos = (char *) file->data + (file->size + offset);
        else if(whence == SEEK_CUR)
            file->pos += offset;
    }

    return oldpos;
    }
}

void F_Rewind(DFILE* file)
{
    F_Seek(file, 0, SEEK_SET);
}
