/***************************************************
 ************ LZSS compression routines ************
 ***************************************************

   This compression algorithm is based on the ideas of Lempel and Ziv,
   with the modifications suggested by Storer and Szymanski. The
   algorithm is based on the use of a ring buffer, which initially
   contains zeros.  We read several characters from the file into the
   buffer, and then search the buffer for the longest string that
   matches the characters just read, and output the length and
   position of the match in the buffer.

   With a buffer size of 4096 bytes, the position can be encoded in 12
   bits. If we represent the match length in four bits, the <position,
   length> pair is two bytes long. If the longest match is no more
   than two characters, then we send just one character without
   encoding, and restart the process with the next letter. We must
   send one extra bit each time to tell the decoder whether we are
   sending a <position, length> pair or an unencoded character, and
   these flags are stored as an eight bit mask every eight items.

   This implementation uses binary trees to speed up the search for
   the longest match.

   Original code by Haruhiko Okumura, 4/6/1989.
   12-2-404 Green Heights, 580 Nagasawa, Yokosuka 239, Japan.

   Later modified for use in the Allegro filesystem by Shawn Hargreaves.

   Later still, modified to function as a stand-alone Win32 DLL by
   Jaakko Keränen.  Currently used as a part of the Doomsday Engine:
   http://sf.net/projects/deng/


   Use, distribute, and modify this code freely.
*/

/** \file
* LZSS compression routines
* These are the legacy LZSS compression routines. New code should not
* use these routines, and instead should use zlib's deflate routines
* if LZSS compression or decompression is required.
*
* This code does not build on Win32.
*
* For now LZSS compression is used by FileReader in client/src/m_misc.c
* by plugins/jhexen/src/sv_save.c in many routines, and by
* client/include/net_main.h in the Demo_* rouines.
*/

// HEADER FILES ------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#ifdef UNIX
#  include <unistd.h>
#endif
#ifdef WIN32
#  include <io.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <de/legacy/findfile.h>
#include "lzss.h"

// MACROS ------------------------------------------------------------------

#if !defined (WIN32) && !defined (__CYGWIN__)
//Disable this on Win32 builds because of: warning: "O_BINARY" redefined on mingw
#define O_BINARY 0
#endif

#ifdef UNIX
#define FILE_OPEN(filename, handle)             handle = open(filename, O_RDONLY | O_BINARY)
#define FILE_CREATE(filename, handle)           handle = open(filename, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0664)
#define FILE_CLOSE(handle)                      close(handle)
#define FILE_READ(handle, buf, size, sz)        sz = read(handle, buf, size)
#define FILE_WRITE(handle, buf, size, sz)       sz = write(handle, buf, size)
#endif
#ifdef WIN32
#define lseek _lseek
#define FILE_OPEN(filename, handle)             handle = _open(filename, O_RDONLY | O_BINARY)
#define FILE_CREATE(filename, handle)           handle = _open(filename, O_WRONLY | O_BINARY | O_CREAT | O_TRUNC, 0664)
#define FILE_CLOSE(handle)                      _close(handle)
#define FILE_READ(handle, buf, size, sz)        sz = _read(handle, buf, size)
#define FILE_WRITE(handle, buf, size, sz)       sz = _write(handle, buf, size)
#endif
#define FILE_SEARCH_STRUCT                      FindData
#define FILE_FINDFIRST(filename, attrib, dta)   FindFile_FindFirst(dta, filename)
#define FILE_FINDNEXT(dta)                      FindFile_FindNext(dta)
#define FILE_FINDCLOSE(dta)                     FindFile_Finish(dta)
#define FILE_ATTRIB                             attrib
#define FILE_SIZE                               size
#define FILE_NAME                               name
#define FILE_DATE                               date
#define FILE_TIME                               time

#define TRUE    1
#define FALSE   0

#ifndef MIN
#  define MIN(x,y)     (((x) < (y)) ? (x) : (y))
#  define MAX(x,y)     (((x) > (y)) ? (x) : (y))
#  define MID(x,y,z)   MAX((x), MIN((y), (z)))
#endif

#define N            4096       /* 4k buffers for LZ compression */
#define F            18         /* upper limit for LZ match length */
#define THRESHOLD    2          /* LZ encode string into pos and length
                                   if match size is greater than this */

// TYPES -------------------------------------------------------------------

typedef struct PACK_DATA        /* stuff for doing LZ compression */
{
    int     state;              /* where have we got to in the pack? */
    int     i, c, len, r, s;
    int     last_match_length, code_buf_ptr;
    unsigned char mask;
    char    code_buf[17];
    int     match_position;
    int     match_length;
    int     lson[N + 1];        /* left children, */
    int     rson[N + 257];      /* right children, */
    int     dad[N + 1];         /* and parents, = binary search trees */
    unsigned char text_buf[N + F - 1];  /* ring buffer, with F-1 extra bytes
                                           for string comparison */
} PACK_DATA;

typedef struct UNPACK_DATA      /* for reading LZ files */
{
    int     state;              /* where have we got to? */
    int     i, j, k, r, c;
    int     flags;
    unsigned char text_buf[N + F - 1];  /* ring buffer, with F-1 extra bytes
                                           for string comparison */
} UNPACK_DATA;

// FUNCTION DECLARATIONS ---------------------------------------------------

int     FlushBuffer(LZFILE * f, int last);
int     RefillBuffer(LZFILE * f);

// DATA --------------------------------------------------------------------

char    thepassword[256] = "";
int     _packfile_filesize = 0;
int     _packfile_datasize = 0;

// CODE --------------------------------------------------------------------

/**
 *  Helper function for the lzGetC() macro.
 */
int _sort_out_getc(LZFILE * f)
{
    if(f->buf_size == 0)
    {
        if(f->todo <= 0)
            f->flags |= LZFILE_FLAG_EOF;
        return *(f->buf_pos++);
    }
    return RefillBuffer(f);
}

/**
 *  Helper function for the lzPutC() macro.
 */
int _sort_out_putc(int c, LZFILE * f)
{
    f->buf_size--;

    if(FlushBuffer(f, FALSE))
        return EOF;

    f->buf_size++;
    return (*(f->buf_pos++) = c);
}

/**
 *  For i = 0 to N-1, rson[i] and lson[i] will be the right and left
 *  children of node i. These nodes need not be initialized. Also, dad[i]
 *  is the parent of node i. These are initialized to N, which stands for
 *  'not used.' For i = 0 to 255, rson[N+i+1] is the root of the tree for
 *  strings that begin with character i. These are initialized to N. Note
 *  there are 256 trees.
 */
static void pack_inittree(PACK_DATA * dat)
{
    int     i;

    for(i = N + 1; i <= N + 256; i++)
        dat->rson[i] = N;

    for(i = 0; i < N; i++)
        dat->dad[i] = N;
}

/* pack_insertnode:
 *  Inserts a string of length F, text_buf[r..r+F-1], into one of the trees
 *  (text_buf[r]'th tree) and returns the longest-match position and length
 *  via match_position and match_length. If match_length = F, then removes
 *  the old node in favor of the new one, because the old one will be
 *  deleted sooner. Note r plays double role, as tree node and position in
 *  the buffer.
 */
static void pack_insertnode(int r, PACK_DATA * dat)
{
    int     i, p, cmp;
    unsigned char *key;
    unsigned char *text_buf = dat->text_buf;

    cmp = 1;
    key = &text_buf[r];
    p = N + 1 + key[0];
    dat->rson[r] = dat->lson[r] = N;
    dat->match_length = 0;

    for(;;)
    {

        if(cmp >= 0)
        {
            if(dat->rson[p] != N)
                p = dat->rson[p];
            else
            {
                dat->rson[p] = r;
                dat->dad[r] = p;
                return;
            }
        }
        else
        {
            if(dat->lson[p] != N)
                p = dat->lson[p];
            else
            {
                dat->lson[p] = r;
                dat->dad[r] = p;
                return;
            }
        }

        for(i = 1; i < F; i++)
            if((cmp = key[i] - text_buf[p + i]) != 0)
                break;

        if(i > dat->match_length)
        {
            dat->match_position = p;
            if((dat->match_length = i) >= F)
                break;
        }
    }

    dat->dad[r] = dat->dad[p];
    dat->lson[r] = dat->lson[p];
    dat->rson[r] = dat->rson[p];
    dat->dad[dat->lson[p]] = r;
    dat->dad[dat->rson[p]] = r;
    if(dat->rson[dat->dad[p]] == p)
        dat->rson[dat->dad[p]] = r;
    else
        dat->lson[dat->dad[p]] = r;
    dat->dad[p] = N;            /* remove p */
}

/**
 *  Removes a node from a tree.
 */
static void pack_deletenode(int p, PACK_DATA * dat)
{
    int     q;

    if(dat->dad[p] == N)
        return;                 /* not in tree */

    if(dat->rson[p] == N)
        q = dat->lson[p];
    else if(dat->lson[p] == N)
        q = dat->rson[p];
    else
    {
        q = dat->lson[p];
        if(dat->rson[q] != N)
        {
            do
            {
                q = dat->rson[q];
            } while(dat->rson[q] != N);
            dat->rson[dat->dad[q]] = dat->lson[q];
            dat->dad[dat->lson[q]] = dat->dad[q];
            dat->lson[q] = dat->lson[p];
            dat->dad[dat->lson[p]] = q;
        }
        dat->rson[q] = dat->rson[p];
        dat->dad[dat->rson[p]] = q;
    }

    dat->dad[q] = dat->dad[p];
    if(dat->rson[dat->dad[p]] == p)
        dat->rson[dat->dad[p]] = q;
    else
        dat->lson[dat->dad[p]] = q;

    dat->dad[p] = N;
}

/**
 *  Called by FlushBuffer(). Packs size bytes from buf, using the pack
 *  information contained in dat. Returns 0 on success.
 */
static int pack_write(LZFILE * file, PACK_DATA * dat, int size,
                      unsigned char *buf, int last)
{
    int     i = dat->i;
    int     c = dat->c;
    int     len = dat->len;
    int     r = dat->r;
    int     s = dat->s;
    int     last_match_length = dat->last_match_length;
    int     code_buf_ptr = dat->code_buf_ptr;
    unsigned char mask = dat->mask;
    int     ret = 0;

    if(dat->state == 2)
        goto pos2;
    else if(dat->state == 1)
        goto pos1;

    dat->code_buf[0] = 0;
    /* code_buf[1..16] saves eight units of code, and code_buf[0] works
       as eight flags, "1" representing that the unit is an unencoded
       letter (1 byte), "0" a position-and-length pair (2 bytes).
       Thus, eight units require at most 16 bytes of code. */

    code_buf_ptr = mask = 1;

    s = 0;
    r = N - F;
    pack_inittree(dat);

    for(len = 0; (len < F) && (size > 0); len++)
    {
        dat->text_buf[r + len] = *(buf++);
        if(--size == 0)
        {
            if(!last)
            {
                dat->state = 1;
                goto getout;
            }
        }
      pos1:
        ;
    }

    if(len == 0)
        goto getout;

    for(i = 1; i <= F; i++)
        pack_insertnode(r - i, dat);
    /* Insert the F strings, each of which begins with one or
       more 'space' characters. Note the order in which these
       strings are inserted. This way, degenerate trees will be
       less likely to occur. */

    pack_insertnode(r, dat);
    /* Finally, insert the whole string just read. match_length
       and match_position are set. */

    do
    {
        if(dat->match_length > len)
            dat->match_length = len;    /* match_length may be long near the end */

        if(dat->match_length <= THRESHOLD)
        {
            dat->match_length = 1;  /* not long enough match: send one byte */
            dat->code_buf[0] |= mask;   /* 'send one byte' flag */
            dat->code_buf[code_buf_ptr++] = dat->text_buf[r];   /* send uncoded */
        }
        else
        {
            /* send position and length pair. Note match_length > THRESHOLD */
            dat->code_buf[code_buf_ptr++] =
                (unsigned char) dat->match_position;
            dat->code_buf[code_buf_ptr++] =
                (unsigned char) (((dat->match_position >> 4) & 0xF0) |
                                 (dat->match_length - (THRESHOLD + 1)));
        }

        if((mask <<= 1) == 0)
        {                       /* shift mask left one bit */
            if(*file->password)
            {
                dat->code_buf[0] ^= *file->password;
                file->password++;
                if(!*file->password)
                    file->password = thepassword;
            };

            for(i = 0; i < code_buf_ptr; i++)   /* send at most 8 units of */
                lzPutC(dat->code_buf[i], file); /* code together */
            if(lzError(file))
            {
                ret = EOF;
                goto getout;
            }
            dat->code_buf[0] = 0;
            code_buf_ptr = mask = 1;
        }

        last_match_length = dat->match_length;

        for(i = 0; (i < last_match_length) && (size > 0); i++)
        {
            c = *(buf++);
            if(--size == 0)
            {
                if(!last)
                {
                    dat->state = 2;
                    goto getout;
                }
            }
          pos2:
            pack_deletenode(s, dat);    /* delete old strings and */
            dat->text_buf[s] = c;   /* read new bytes */
            if(s < F - 1)
                dat->text_buf[s + N] = c;   /* if the position is near the end of
                                               buffer, extend the buffer to make
                                               string comparison easier */
            s = (s + 1) & (N - 1);
            r = (r + 1) & (N - 1);  /* since this is a ring buffer,
                                       increment the position modulo N */

            pack_insertnode(r, dat);    /* register the string in
                                           text_buf[r..r+F-1] */
        }

        while(i++ < last_match_length)
        {                       /* after the end of text, */
            pack_deletenode(s, dat);    /* no need to read, but */
            s = (s + 1) & (N - 1);  /* buffer may not be empty */
            r = (r + 1) & (N - 1);
            if(--len)
                pack_insertnode(r, dat);
        }

    } while(len > 0);           /* until length of string to be processed is zero */

    if(code_buf_ptr > 1)
    {                           /* send remaining code */
        if(*file->password)
        {
            dat->code_buf[0] ^= *file->password;
            file->password++;
            if(!*file->password)
                file->password = thepassword;
        };

        for(i = 0; i < code_buf_ptr; i++)
        {
            lzPutC(dat->code_buf[i], file);
            if(lzError(file))
            {
                ret = EOF;
                goto getout;
            }
        }
    }

    dat->state = 0;

  getout:

    dat->i = i;
    dat->c = c;
    dat->len = len;
    dat->r = r;
    dat->s = s;
    dat->last_match_length = last_match_length;
    dat->code_buf_ptr = code_buf_ptr;
    dat->mask = mask;

    return ret;
}

/**
 *  Called by RefillBuffer(). Unpacks from dat into buf, until either
 *  EOF is reached or s bytes have been extracted. Returns the number of
 *  bytes added to the buffer
 */
static int pack_read(LZFILE * file, UNPACK_DATA * dat, int s,
                     unsigned char *buf)
{
    int     i = dat->i;
    int     j = dat->j;
    int     k = dat->k;
    int     r = dat->r;
    int     c = dat->c;
    unsigned int flags = dat->flags;
    int     size = 0;

    if(dat->state == 2)
        goto pos2;
    else if(dat->state == 1)
        goto pos1;

    r = N - F;
    flags = 0;

    for(;;)
    {
        if(((flags >>= 1) & 256) == 0)
        {
            if((c = lzGetC(file)) == EOF)
                break;

            if(*file->password)
            {
                c ^= *file->password;
                file->password++;
                if(!*file->password)
                    file->password = thepassword;
            };

            flags = c | 0xFF00; /* uses higher byte to count eight */
        }

        if(flags & 1)
        {
            if((c = lzGetC(file)) == EOF)
                break;
            dat->text_buf[r++] = c;
            r &= (N - 1);
            *(buf++) = c;
            if(++size >= s)
            {
                dat->state = 1;
                goto getout;
            }
          pos1:
            ;
        }
        else
        {
            if((i = lzGetC(file)) == EOF)
                break;
            if((j = lzGetC(file)) == EOF)
                break;
            i |= ((j & 0xF0) << 4);
            j = (j & 0x0F) + THRESHOLD;
            for(k = 0; k <= j; k++)
            {
                c = dat->text_buf[(i + k) & (N - 1)];
                dat->text_buf[r++] = c;
                r &= (N - 1);
                *(buf++) = c;
                if(++size >= s)
                {
                    dat->state = 2;
                    goto getout;
                }
              pos2:
                ;
            }
        }
    }

    dat->state = 0;

  getout:

    dat->i = i;
    dat->j = j;
    dat->k = k;
    dat->r = r;
    dat->c = c;
    dat->flags = flags;

    return size;
}

/**
 *  Helper for encrypting magic numbers, using the current password.
 */
long Encrypt(long x)
{
    long    mask = 0;
    int     i;

    for(i = 0; thepassword[i]; i++)
        mask ^= ((long) thepassword[i] << ((i & 3) * 8));

    return x ^ mask;
}

/**
 *  Reads a 16 bit word from a file, using intel byte ordering.
 */
int16_t lzGetW(LZFILE * f)
{
        int16_t b1, b2;

    if((b1 = lzGetC(f)) != EOF)
                if((b2 = lzGetC(f)) != EOF)
            return ((b2 << 8) | b1);

    return EOF;
}

/**
 *  Reads a 32 bit long from a file, using intel byte ordering.
 */
int32_t lzGetL(LZFILE * f)
{
        int32_t b1, b2, b3, b4;

    if((b1 = lzGetC(f)) != EOF)
        if((b2 = lzGetC(f)) != EOF)
            if((b3 = lzGetC(f)) != EOF)
                if((b4 = lzGetC(f)) != EOF)
                                        return (((int32_t) b4 << 24) | ((int32_t) b3 << 16) |
                                                        ((int32_t) b2 << 8) | (int32_t) b1);

    return EOF;
}

/**
 *  Writes a 16 bit int to a file, using intel byte ordering.
 */
int16_t lzPutW(int16_t w, LZFILE * f)
{
        int b1, b2;

    b1 = (w & 0xFF00) >> 8;
    b2 = w & 0x00FF;

    if(lzPutC(b2, f) == b2)
        if(lzPutC(b1, f) == b1)
            return w;

    return EOF;
}

/**
 *  Writes a 32 bit long to a file, using intel byte ordering.
 */
int32_t lzPutL(int32_t l, LZFILE * f)
{
    int     b1, b2, b3, b4;

    b1 = (int) ((l & 0xFF000000L) >> 24);
    b2 = (int) ((l & 0x00FF0000L) >> 16);
    b3 = (int) ((l & 0x0000FF00L) >> 8);
    b4 = (int) l & 0x00FF;

    if(lzPutC(b4, f) == b4)
        if(lzPutC(b3, f) == b3)
            if(lzPutC(b2, f) == b2)
                if(lzPutC(b1, f) == b1)
                    return l;

    return EOF;
}

/**
 *  Reads a 16 bit int from a file, using motorola byte-ordering.
 */
int lzGetWm(LZFILE * f)
{
    int     b1, b2;

    if((b1 = lzGetC(f)) != EOF)
        if((b2 = lzGetC(f)) != EOF)
            return ((b1 << 8) | b2);

    return EOF;
}

/**
 *  Reads a 32 bit long from a file, using motorola byte-ordering.
 */
long lzGetLm(LZFILE * f)
{
    int     b1, b2, b3, b4;

    if((b1 = lzGetC(f)) != EOF)
        if((b2 = lzGetC(f)) != EOF)
            if((b3 = lzGetC(f)) != EOF)
                if((b4 = lzGetC(f)) != EOF)
                    return (((long) b1 << 24) | ((long) b2 << 16) |
                            ((long) b3 << 8) | (long) b4);

    return EOF;
}

/**
 *  Writes a 16 bit int to a file, using motorola byte-ordering.
 */
int lzPutWm(int w, LZFILE * f)
{
    int     b1, b2;

    b1 = (w & 0xFF00) >> 8;
    b2 = w & 0x00FF;

    if(lzPutC(b1, f) == b1)
        if(lzPutC(b2, f) == b2)
            return w;

    return EOF;
}

/**
 *  Writes a 32 bit long to a file, using motorola byte-ordering.
 */
long lzPutLm(long l, LZFILE * f)
{
    int     b1, b2, b3, b4;

    b1 = (int) ((l & 0xFF000000L) >> 24);
    b2 = (int) ((l & 0x00FF0000L) >> 16);
    b3 = (int) ((l & 0x0000FF00L) >> 8);
    b4 = (int) l & 0x00FF;

    if(lzPutC(b1, f) == b1)
        if(lzPutC(b2, f) == b2)
            if(lzPutC(b3, f) == b3)
                if(lzPutC(b4, f) == b4)
                    return l;

    return EOF;
}

/**
 *  Opens a file according to mode, which may contain any of the flags:
 *  'r': open file for reading.
 *  'w': open file for writing, overwriting any existing data.
 *  'p': open file in 'packed' mode. Data will be compressed as it is
 *       written to the file, and automatically uncompressed during read
 *       operations. Files created in this mode will produce garbage if
 *       they are read without this flag being set.
 *  '!': open file for writing in normal, unpacked mode, but add the value
 *       F_NOPACK_MAGIC to the start of the file, so that it can be opened
 *       in packed mode and Allegro will automatically detect that the
 *       data does not need to be decompressed.
 *
 *  Instead of these flags, one of the constants F_READ, F_WRITE,
 *  F_READ_PACKED, F_WRITE_PACKED or F_WRITE_NOPACK may be used as the second
 *  argument to fopen().
 *
 *  On success, fopen() returns a pointer to a file structure, and on error
 *  it returns NULL and stores an error code in errno. An attempt to read a
 *  normal file in packed mode will cause errno to be set to EDOM.
 */

LZFILE *lzOpen(char const *filename, char const *mode)
{
    LZFILE *f, *f2;
    FILE_SEARCH_STRUCT dta;
    int     c;
    long    header = FALSE;

    errno = 0;

    if((f = malloc(sizeof(LZFILE))) == NULL)
    {
        errno = ENOMEM;
        return NULL;
    }

    f->buf_pos = f->buf;
    f->flags = 0;
    f->buf_size = 0;
    f->filename = NULL;
    f->password = thepassword;

    for(c = 0; mode[c]; c++)
    {
        switch (mode[c])
        {
        case 'r':
        case 'R':
            f->flags &= ~LZFILE_FLAG_WRITE;
            break;
        case 'w':
        case 'W':
            f->flags |= LZFILE_FLAG_WRITE;
            break;
        case 'p':
        case 'P':
            f->flags |= LZFILE_FLAG_PACK;
            break;
        case '!':
            f->flags &= ~LZFILE_FLAG_PACK;
            header = TRUE;
            break;
        }
    }

    if(f->flags & LZFILE_FLAG_WRITE)
    {
        if(f->flags & LZFILE_FLAG_PACK)
        {
            /* write a packed file */
            PACK_DATA *dat = malloc(sizeof(PACK_DATA));

            if(!dat)
            {
                errno = ENOMEM;
                free(f);
                return NULL;
            }
            if((f->parent = lzOpen(filename, F_WRITE)) == NULL)
            {
                free(dat);
                free(f);
                return NULL;
            }
            lzPutLm(Encrypt(F_PACK_MAGIC), f->parent);
            f->todo = 4;
            for(c = 0; c < N - F; c++)
                dat->text_buf[c] = 0;
            dat->state = 0;
            f->pack_data = dat;
        }
        else
        {
            /* write a 'real' file */
            f->parent = NULL;
            f->pack_data = NULL;

            FILE_CREATE(filename, f->hndl);
            if(f->hndl < 0)
            {
                free(f);
                return NULL;
            }
            errno = 0;
            f->todo = 0;
        }
        if(header)
            lzPutLm(Encrypt(F_NOPACK_MAGIC), f);
    }
    else
    {                           /* must be a read */
        if(f->flags & LZFILE_FLAG_PACK)
        {
            /* read a packed file */
            UNPACK_DATA *dat = malloc(sizeof(UNPACK_DATA));

            if(!dat)
            {
                errno = ENOMEM;
                free(f);
                return NULL;
            }
            if((f->parent = lzOpen(filename, F_READ)) == NULL)
            {
                free(dat);
                free(f);
                return NULL;
            }
            header = lzGetLm(f->parent);
            if(header == Encrypt(F_PACK_MAGIC))
            {
                for(c = 0; c < N - F; c++)
                    dat->text_buf[c] = 0;
                dat->state = 0;
                f->todo = LONG_MAX;
                f->pack_data = (char *) dat;
            }
            else
            {
                if(header == Encrypt(F_NOPACK_MAGIC))
                {
                    f2 = f->parent;
                    free(dat);
                    free(f);
                    return f2;
                }
                else
                {
                    lzClose(f->parent);
                    free(dat);
                    free(f);
                    if(errno == 0)
                        errno = EDOM;
                    return NULL;
                }
            }
        }
        else
        {
            /* read a 'real' file */
            f->parent = NULL;
            f->pack_data = NULL;
            errno = FILE_FINDFIRST(filename, A_RDONLY | A_HIDDEN | A_ARCH, &dta);
            if(errno != 0)
            {
                FILE_FINDCLOSE(&dta);
                free(f);
                return NULL;
            }
            f->todo = (long) dta.FILE_SIZE;
            FILE_FINDCLOSE(&dta);

            FILE_OPEN(filename, f->hndl);
            if(f->hndl < 0)
            {
                errno = f->hndl;
                free(f);
                return NULL;
            }
        }
    }
    return f;
}

/**
 *  Closes a file after it has been read or written.
 *  Returns zero on success. On error it returns an error code which is
 *  also stored in errno. This function can fail only when writing to
 *  files: if the file was opened in read mode it will always succeed.
 */

int lzClose(LZFILE * f)
{
    if(f)
    {
        if(f->flags & LZFILE_FLAG_WRITE)
        {
#if 0
            if(f->flags & LZFILE_FLAG_CHUNK)
                return lzClose(lzCloseChunk(f));
#endif
            FlushBuffer(f, TRUE);
        }

        if(f->pack_data)
            free(f->pack_data);

        if(f->parent)
            lzClose(f->parent);
        else
            FILE_CLOSE(f->hndl);

        free(f);
        return errno;
    }
    return 0;
}

/**
 *  Like the stdio fseek() function, but only supports forward seeks
 *  relative to the current file position.
 */
int lzSeek(LZFILE * f, int offset)
{
    int     i;

    if(f->flags & LZFILE_FLAG_WRITE)
        return -1;

    errno = 0;

    /* skip forward through the buffer */
    if(f->buf_size > 0)
    {
        i = MIN(offset, f->buf_size);
        f->buf_size -= i;
        f->buf_pos += i;
        offset -= i;
        if((f->buf_size <= 0) && (f->todo <= 0))
            f->flags |= LZFILE_FLAG_EOF;
    }

    /* need to seek some more? */
    if(offset > 0)
    {
        i = MIN(offset, f->todo);

        if(f->flags & LZFILE_FLAG_PACK)
        {
            /* for compressed files, we just have to read through the data */
            while(i > 0)
            {
                lzGetC(f);
                i--;
            }
        }
        else
        {
            if(f->parent)
            {
                /* pass the seek request on to the parent file */
                lzSeek(f->parent, i);
            }
            else
            {
                /* do a real seek */
                lseek(f->hndl, i, SEEK_CUR);
            }
            f->todo -= i;
            if(f->todo <= 0)
                f->flags |= LZFILE_FLAG_EOF;
        }
    }

    return errno;
}

/**
 *  Reads n bytes from f and stores them at memory location p. Returns the
 *  number of items read, which will be less than n if EOF is reached or an
 *  error occurs. Error codes are stored in errno.
 */
long lzRead(void *p, long n, LZFILE * f)
{
    long    c;                  /* counter of bytes read */
    int     i;
    unsigned char *cp = (unsigned char *) p;

    for(c = 0; c < n; c++)
    {
        if(--(f->buf_size) > 0)
            *(cp++) = *(f->buf_pos++);
        else
        {
            i = _sort_out_getc(f);
            if(i == EOF)
                return c;
            else
                *(cp++) = i;
        }
    }

    return n;
}

/**
 *  Writes n bytes to the file f from memory location p. Returns the number
 *  of items written, which will be less than n if an error occurs. Error
 *  codes are stored in errno.
 */
long lzWrite(void *p, long n, LZFILE * f)
{
    long    c;                  /* counter of bytes written */
    unsigned char *cp = (unsigned char *) p;

    for(c = 0; c < n; c++)
    {
        if(++(f->buf_size) >= F_BUF_SIZE)
        {
            if(_sort_out_putc(*cp, f) != *cp)
                return c;
            cp++;
        }
        else
            *(f->buf_pos++) = *(cp++);
    }

    return n;
}

/**
 *  Reads a line from a text file, storing it at location p. Stops when a
 *  linefeed is encountered, or max characters have been read. Returns a
 *  pointer to where it stored the text, or NULL on error. The end of line
 *  is handled by detecting '\n' characters: '\r' is simply ignored.
 */
char   *lzGetS(char *p, int max, LZFILE * f)
{
    int     c;

    if(lzEOF(f))
    {
        p[0] = 0;
        return NULL;
    }

    for(c = 0; c < max - 1; c++)
    {
        p[c] = lzGetC(f);
        if(p[c] == '\r')
            c--;
        else if(p[c] == '\n')
            break;
    }

    p[c] = 0;

    if(errno)
        return NULL;
    else
        return p;
}

/**
 *  Writes a string to a text file, returning zero on success, -1 on error.
 *  EOL ('\n') characters are expanded to CR/LF ('\r\n') pairs.
 */
int lzPutS(char *p, LZFILE * f)
{
    while(*p)
    {
        if(*p == '\n')
        {
            lzPutC('\r', f);
            lzPutC('\n', f);
        }
        else
            lzPutC(*p, f);

        p++;
    }

    if(errno)
        return -1;
    else
        return 0;
}

/**
 *  Refills the read buffer. The file must have been opened in read mode,
 *  and the buffer must be empty.
 */
int RefillBuffer(LZFILE * f)
{
    int     sz;

    if((f->flags & LZFILE_FLAG_EOF) || (f->todo <= 0))
    {                           /* EOF */
        f->flags |= LZFILE_FLAG_EOF;
        return EOF;
    }

    if(f->parent)
    {
        if(f->flags & LZFILE_FLAG_PACK)
        {
            f->buf_size =
                pack_read(f->parent, (UNPACK_DATA *) f->pack_data,
                          MIN(F_BUF_SIZE, f->todo), f->buf);
        }
        else
        {
            f->buf_size = lzRead(f->buf, MIN(F_BUF_SIZE, f->todo), f->parent);
        }
        if(f->parent->flags & LZFILE_FLAG_EOF)
            f->todo = 0;
        if(f->parent->flags & LZFILE_FLAG_ERROR)
            goto err;
    }
    else
    {
        f->buf_size = MIN(F_BUF_SIZE, f->todo);
        FILE_READ(f->hndl, f->buf, f->buf_size, sz);
        if(sz != f->buf_size)
            goto err;
    }

    f->todo -= f->buf_size;
    f->buf_pos = f->buf;
    f->buf_size--;
    if(f->buf_size <= 0)
        if(f->todo <= 0)
            f->flags |= LZFILE_FLAG_EOF;

    return *(f->buf_pos++);

  err:
    errno = EFAULT;
    f->flags |= LZFILE_FLAG_ERROR;
    return EOF;
}

/**
 * flushes a file buffer to the disk. The file must be open in write mode.
 */
int FlushBuffer(LZFILE * f, int last)
{
    int     sz;

    if(f->buf_size > 0)
    {
        if(f->flags & LZFILE_FLAG_PACK)
        {
            if(pack_write
               (f->parent, (PACK_DATA *) f->pack_data, f->buf_size, f->buf,
                last))
                goto err;
        }
        else
        {
            FILE_WRITE(f->hndl, f->buf, f->buf_size, sz);
            if(sz != f->buf_size)
                goto err;
        }
        f->todo += f->buf_size;
    }
    f->buf_pos = f->buf;
    f->buf_size = 0;
    return 0;

  err:
    errno = EFAULT;
    f->flags |= LZFILE_FLAG_ERROR;
    return EOF;
}

int lzGetC(LZFILE * f)
{
    f->buf_size--;
    if(f->buf_size > 0)
        return *(f->buf_pos++);
    else
        return _sort_out_getc(f);
}

int lzPutC(int c, LZFILE * f)
{
    f->buf_size++;
    if(f->buf_size >= F_BUF_SIZE)
        return _sort_out_putc(c, f);
    else
        return (*(f->buf_pos++) = c);
}

/**
 *  Sets the password to be used by all future read/write operations.
 */
void lzPassword(char *password)
{
    if(password)
    {
        strncpy(thepassword, password, 255);
        thepassword[255] = 0;
    }
    else
        thepassword[0] = 0;
}
