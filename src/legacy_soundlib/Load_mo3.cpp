/*
 * This source code is public domain.
 *
 * Purpose: Load MO3-packed modules using unmo3.dll
 * Authors: Johannes Schultz
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "Loaders.h"
#ifdef MODPLUG_TRACKER
#include "../moddoc.h"
#include "../main.h"
#endif // MODPLUG_TRACKER

// decode a MO3 file (returns the same "exit codes" as UNMO3.EXE, eg. 0=success)
// IN: data/len = MO3 data/len
// OUT: data/len = decoded data/len (if successful)
typedef int (WINAPI * UNMO3_DECODE)(void **data, int *len);
// free the data returned by UNMO3_Decode
typedef void (WINAPI * UNMO3_FREE)(void *data);


bool module_renderer::ReadMO3(const uint8_t * lpStream, const uint32_t dwMemLength)
//-----------------------------------------------------------
{
    // no valid MO3 file (magic bytes: "MO3")
    if(dwMemLength < 4 || lpStream[0] != 'M' || lpStream[1] != 'O' || lpStream[2] != '3')
            return false;

#ifdef NO_MO3_SUPPORT
    /* As of August 2010, the format revision is 5; Versions > 31 are unlikely to exist in the next few years,
    so we will just ignore those if there's no UNMO3 library to tell us if the file is valid or not
    (avoid log entry with .MOD files that have a song name starting with "MO3" */
    if(lpStream[3] > 31) return false;

#ifdef MODPLUG_TRACKER
    if(m_pModDoc != nullptr) m_pModDoc->AddToLog(GetStrI18N(_TEXT("The file appears to be a MO3 file, but this OpenMPT build does not support loading MO3 files.")));
#endif // MODPLUG_TRACKER
    return false;

#else
    bool bResult = false; // result of trying to load the module, false == fail.

    int iLen = static_cast<int>(dwMemLength);
    void **mo3Stream = (void **)&lpStream;

    // try to load unmo3.dll dynamically.
#ifdef MODPLUG_TRACKER
    CHAR szPath[MAX_PATH];
    strcpy(szPath, theApp.GetAppDirPath());
    _tcsncat(szPath, _TEXT("unmo3.dll"), MAX_PATH - (_tcslen(szPath) + 1));
    HMODULE unmo3 = LoadLibrary(szPath);
#else
    HMODULE unmo3 = LoadLibrary(_TEXT("unmo3.dll"));
#endif // MODPLUG_TRACKER
    if(unmo3 == NULL) // Didn't succeed.
    {
#ifdef MODPLUG_TRACKER
            if(m_pModDoc != nullptr) m_pModDoc->AddToLog(GetStrI18N(_TEXT("Loading MO3 file failed because unmo3.dll could not be loaded.")));
#endif // MODPLUG_TRACKER
    }
    else //case: dll loaded succesfully.
    {
            UNMO3_DECODE UNMO3_Decode = (UNMO3_DECODE)GetProcAddress(unmo3, "UNMO3_Decode");
            UNMO3_FREE UNMO3_Free = (UNMO3_FREE)GetProcAddress(unmo3, "UNMO3_Free");

            if(UNMO3_Decode != NULL && UNMO3_Free != NULL)
            {
                    if(UNMO3_Decode(mo3Stream, &iLen) == 0)
                    {
                            /* if decoding was successful, mo3Stream and iLen will keep the new
                               pointers now. */

                            if(iLen > 0)
                            {
                                    bResult = true;
                                    if ((!ReadXM((const uint8_t *)*mo3Stream, (uint32_t)iLen))
                                    && (!ReadIT((const uint8_t *)*mo3Stream, (uint32_t)iLen))
                                    && (!ReadS3M((const uint8_t *)*mo3Stream, (uint32_t)iLen))
                                    #ifndef FASTSOUNDLIB
                                    && (!ReadMTM((const uint8_t *)*mo3Stream, (uint32_t)iLen))
                                    #endif // FASTSOUNDLIB
                                    && (!ReadMod((const uint8_t *)*mo3Stream, (uint32_t)iLen))) bResult = false;
                            }

                            UNMO3_Free(*mo3Stream);
                    }
            }
            FreeLibrary(unmo3);
    }
    return bResult;
#endif // NO_MO3_SUPPORT
}
