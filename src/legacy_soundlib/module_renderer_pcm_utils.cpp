/*
 * OpenMPT
 *
 * Waveform.cpp
 *
 * Authors: Olivier Lapicque <olivierl@jps.net>
 *          OpenMPT devs
*/

#include "stdafx.h"
#include "sndfile.h"


void X86_Cvt16S_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        *p++ = (char)(((int)s[(n << 2)+1] + (int)s[(n << 2)+3]) >> 1);
        dwPos += dwInc;
    }
}


void X86_Cvt8S_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    unsigned char *s = (unsigned char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        *p++ = (char)((((UINT)s[(n << 1)] + (UINT)s[(n << 1)+1]) >> 1) - 0x80);
        dwPos += dwInc;
    }
}


void X86_Cvt16M_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        *p++ = (char)s[(n << 1)+1];
        dwPos += dwInc;
    }
}


void X86_Cvt8M_8M(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        *p++ = (char)(s[dwPos >> 16] - 0x80);
        dwPos += dwInc;
    }
}


void X86_Cvt16S_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        p[0] = s[(n << 2)+1];
        p[1] = s[(n << 2)+3];
        p += 2;
        dwPos += dwInc;
    }
}


void X86_Cvt8S_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        p[0] = (char)(s[n << 1] - 0x80);
        p[1] = (char)(s[(n << 1)+1] - 0x80);
        p += 2;
        dwPos += dwInc;
    }
}


void X86_Cvt16M_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//-------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        UINT n = dwPos >> 16;
        p[1] = p[0] = (char)s[(n << 1)+1];
        p += 2;
        dwPos += dwInc;
    }
}


void X86_Cvt8M_8S(LPBYTE lpSrc, signed char *lpDest, UINT nSamples, uint32_t dwInc)
//------------------------------------------------------------------------------
{
    uint32_t dwPos = 0;
    signed char *p = lpDest;
    signed char *s = (signed char *)lpSrc;
    for (UINT i=0; i<nSamples; i++)
    {
        p[1] = p[0] = (char)(s[dwPos >> 16] - 0x80);
        p += 2;
        dwPos += dwInc;
    }
}



UINT module_renderer::WaveConvert(LPBYTE lpSrc, signed char *lpDest, UINT nSamples)
//----------------------------------------------------------------------------
{
    uint32_t dwInc;
    if ((!lpSrc) || (!lpDest) || (!nSamples)) return 0;
    dwInc = _muldiv(deprecated_global_mixing_freq, 0x10000, 22050);
    if (deprecated_global_channels >= 2)
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Stereo, 16-bit
            X86_Cvt16S_8M(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Stereo, 8-bit
            X86_Cvt8S_8M(lpSrc, lpDest, nSamples, dwInc);
        }
    } else
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Mono, 16-bit
            X86_Cvt16M_8M(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Mono, 8-bit
            X86_Cvt8M_8M(lpSrc, lpDest, nSamples, dwInc);
        }
    }
    return nSamples;
}


UINT module_renderer::WaveStereoConvert(LPBYTE lpSrc, signed char *lpDest, UINT nSamples)
//----------------------------------------------------------------------------------
{
    uint32_t dwInc;
    if ((!lpSrc) || (!lpDest) || (!nSamples)) return 0;
    dwInc = _muldiv(deprecated_global_mixing_freq, 0x10000, 22050);
    if (deprecated_global_channels >= 2)
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Stereo, 16-bit
            X86_Cvt16S_8S(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Stereo, 8-bit
            X86_Cvt8S_8S(lpSrc, lpDest, nSamples, dwInc);
        }
    } else
    {
        if (deprecated_global_bits_per_sample == 16)
        {
            // Mono, 16-bit
            X86_Cvt16M_8S(lpSrc, lpDest, nSamples, dwInc);
        } else
        {
            // Mono, 8-bit
            X86_Cvt8M_8S(lpSrc, lpDest, nSamples, dwInc);
        }
    }
    return nSamples;
}

///////////////////////////////////////////////////////////////////////
// Spectrum Analyzer

extern int SpectrumSinusTable[256*2];


void X86_Spectrum(signed char *pBuffer, UINT nSamples, UINT nInc, UINT nSmpSize, LPLONG lpSinCos)
//-----------------------------------------------------------------------------------------------
{
    signed char *p = pBuffer;
    UINT wt = 0x40;
    LONG ecos = 0, esin = 0;
    for (UINT i=0; i<nSamples; i++)
    {
        int a = *p;
        ecos += a*SpectrumSinusTable[wt & 0x1FF];
        esin += a*SpectrumSinusTable[(wt+0x80) & 0x1FF];
        wt += nInc;
        p += nSmpSize;
    }
    lpSinCos[0] = esin;
    lpSinCos[1] = ecos;
}



LONG module_renderer::SpectrumAnalyzer(signed char *pBuffer, UINT nSamples, UINT nInc, UINT nChannels)
//-----------------------------------------------------------------------------------------------
{
    LONG sincos[2];
    UINT n = nSamples & (~3);
    if ((pBuffer) && (n))
    {
        sincos[0] = sincos[1] = 0;
        X86_Spectrum(pBuffer, n, nInc, nChannels, sincos);
        LONG esin = sincos[0], ecos = sincos[1];
        if (ecos < 0) ecos = -ecos;
        if (esin < 0) esin = -esin;
        int bug = 64*8 + (64*64 / (nInc+5));
        if (ecos > bug) ecos -= bug; else ecos = 0;
        if (esin > bug) esin -= bug; else esin = 0;
        return ((ecos + esin) << 2) / nSamples;
    }
    return 0;
}

#ifndef FASTSOUNDLIB

#undef ASM_NORMALIZE
#ifdef ASM_NORMALIZE

#pragma warning(disable:4100)
#pragma warning(disable:4731)


/*
void normalize_24_bit_buffer(char *buffer, size_t size, int lmax24, int *output) {
    if (256 <= lmax24) {
        lmax24 = 256;
    }
    for (size_t idx = 0; idx < size; ++idx) {
        int bufidx = idx * 3;
        long pbuffer = buffer[bufidx + 2];
        pbuffer = (pbuffer << 16) | (buffer[bufidx + 1] << 8) | buffer[bufidx];

        int scale = 1 << (31 - modplug::mixer::MIXING_ATTENUATION);
        pbuffer *= scale;
        pbuffer /=

        output[idx] = pbuffer;
    }
}

void __cdecl X86_Normalize24BitBuffer(LPBYTE pbuffer, UINT dwSize, uint32_t lmax24, int *poutput)
{
    _asm {
    mov esi, pbuffer    // esi = edi = pbuffer
    mov edx, dwSize            // edx = dwSize
    mov ebx, lmax24            // ebx = bad_max
    mov edi, poutput    // edi = output 32-bit buffer
    push ebp
    mov ebp, edx            // ebp = dwSize
    cmp ebx, 256
    jg normloop
    mov ebx, 256
normloop:
    movsx eax, byte ptr [esi+2]
    movzx edx, byte ptr [esi+1]
    shl eax, 8
    or eax, edx
    movzx edx, byte ptr [esi]
    shl eax, 8
    or eax, edx            // eax = 24-bit sample
    mov edx, 1 << (31-modplug::mixer::MIXING_ATTENUATION)
    add esi, 3
    imul edx
    add edi, 4
    idiv ebx            // eax = 28-bit normalized sample
    dec ebp
    mov dword ptr [edi-4], eax
    jnz normloop
    pop ebp
    }
}
*/

#pragma warning(default:4100)

#endif

extern void MPPASMCALL X86_Dither(int *pBuffer, UINT nSamples, UINT nBits);
extern int MixSoundBuffer[];

UINT module_renderer::Normalize24BitBuffer(LPBYTE pbuffer, UINT dwSize, uint32_t lmax24, uint32_t dwByteInc)
//-----------------------------------------------------------------------------------------------
{
#ifdef ASM_NORMALIZE
    int * const tempbuf = MixSoundBuffer;
    int n = dwSize / 3;
    while (n > 0)
    {
        int nbuf = (n > modplug::mixer::MIX_BUFFER_SIZE*2) ? modplug::mixer::MIX_BUFFER_SIZE*2 : n;
        X86_Normalize24BitBuffer(pbuffer, nbuf, lmax24, tempbuf);
        X86_Dither(tempbuf, nbuf, 8 * dwByteInc);
        switch(dwByteInc)
        {
        case 2:            X86_Convert32To16(pbuffer, tempbuf, nbuf); break;
        case 3:            X86_Convert32To24(pbuffer, tempbuf, nbuf); break;
        default:    X86_Convert32To8(pbuffer, tempbuf, nbuf);
        }
        n -= nbuf;
        pbuffer += dwByteInc * nbuf;
    }
    return (dwSize/3) * dwByteInc;
#else
    LONG lMax = (lmax24 / 128) + 1;
    UINT i = 0;
    for (UINT j=0; j<dwSize; j+=3, i+=dwByteInc)
    {
        LONG l = ((((pbuffer[j+2] << 8) + pbuffer[j+1]) << 8) + pbuffer[j]) << 8;
        l /= lMax;
        if (dwByteInc > 1)
        {
            pbuffer[i] = (uint8_t)(l & 0xFF);
            pbuffer[i+1] = (uint8_t)(l >> 8);
        } else
        {
            pbuffer[i] = (uint8_t)((l + 0x8000) >> 8);
        }
    }
    return i;
#endif
}


#endif // FASTSOUNDLIB