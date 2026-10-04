//
// Copyright(C) 1993-1996 Id Software, Inc.
// Copyright(C) 2005-2014 Simon Howard
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//	WAD I/O functions.
//

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "m_misc.h"
#include "w_file.h"
#include "z_zone.h"

extern uint8_t *s_wad_cache;
extern size_t s_wad_cache_len;

typedef struct
{
    wad_file_t wad;
    FILE *fstream;
} stdc_wad_file_t;

extern wad_file_class_t stdc_wad_file;
static stdc_wad_file_t s_stdc_wad;

static wad_file_t *W_StdC_OpenFile(char *path)
{
    FILE *fstream = fopen(path, "rb");
    if (fstream == NULL)
    {
        return NULL;
    }

    s_stdc_wad.wad.file_class = &stdc_wad_file;
    s_stdc_wad.wad.mapped = (byte*)s_wad_cache;
    s_stdc_wad.wad.length = s_wad_cache_len ? (unsigned int)s_wad_cache_len : (unsigned int)M_FileLength(fstream);
    s_stdc_wad.fstream = fstream;

    return &s_stdc_wad.wad;
}

static void W_StdC_CloseFile(wad_file_t *wad)
{
    stdc_wad_file_t *stdc_wad = (stdc_wad_file_t *) wad;
    if (stdc_wad && stdc_wad->fstream)
    {
        fclose(stdc_wad->fstream);
        stdc_wad->fstream = NULL;
    }
}

// Read data from the specified position in the file into the 
// provided buffer.  Returns the number of bytes read.
size_t W_StdC_Read(wad_file_t *wad, unsigned int offset,
                   void *buffer, size_t buffer_len)
{
    if (s_wad_cache && ((size_t)offset + buffer_len <= s_wad_cache_len))
    {
        memcpy(buffer, s_wad_cache + offset, buffer_len);
        return buffer_len;
    }

    stdc_wad_file_t *stdc_wad = (stdc_wad_file_t *) wad;
    if (!stdc_wad || !stdc_wad->fstream) return 0;

    fseek(stdc_wad->fstream, offset, SEEK_SET);
    return fread(buffer, 1, buffer_len, stdc_wad->fstream);
}

wad_file_class_t stdc_wad_file = 
{
    W_StdC_OpenFile,
    W_StdC_CloseFile,
    W_StdC_Read,
};


