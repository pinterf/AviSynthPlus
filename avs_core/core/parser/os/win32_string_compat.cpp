// Copy of AvxSynth's windowsPorts.cpp; it was missing the licensing
// header there, too.

#include "win32_string_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>

char *_strrev(char *str)
{
    unsigned long nLength = strlen(str);
    for(unsigned long i = 0; i < nLength/2; i++)
    {
        char chTemp = str[i];
        str[i] = str[nLength - i - 1];
        str[nLength - i - 1] = chTemp;
    }
    return str;
}

char *_strupr(char *str)
{
    if (str)
    {
        unsigned long nLength = strlen(str);
        for(unsigned long i = 0; i < nLength; i++)
        {
            str[i] = (char)toupper((unsigned char)str[i]);
        }
    }

	return str;
}

char *_strlwr(char *str)
{
    if (str)
    {
        unsigned long nLength = strlen(str);
        for(unsigned long i = 0; i < nLength; i++)
        {
            str[i] = (char)tolower((unsigned char)str[i]);
        }
    }

    return str;
}
