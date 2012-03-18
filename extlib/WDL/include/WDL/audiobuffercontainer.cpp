#include "audiobuffercontainer.h"
#include "queue.h"
#include <assert.h>

void ChannelPinMapper::SetNPins(int nPins)
{
  m_mapping.Resize(nPins);
  int i;
  for (i = m_nPins; i < nPins; ++i) {
    ClearPin(i);
    if (i < m_nCh) {
      SetPin(i, i, true);
    }
  }
  m_nPins = nPins;
}

void ChannelPinMapper::SetNChannels(int nCh)
{
  int i;
  for (i = m_nCh; i < nCh && i < m_nPins; ++i) {
    SetPin(i, i, true);
  }
  m_nCh = nCh;
}

void ChannelPinMapper::Init(WDL_UINT64* pMapping, int nPins)
{
  m_mapping.Resize(nPins);
  memcpy(m_mapping.Get(), pMapping, nPins*sizeof(WDL_UINT64));
  m_nPins = m_nCh = nPins;
}

#define BITMASK64(bitIdx) (((WDL_UINT64)1)<<(bitIdx))
 
void ChannelPinMapper::ClearPin(int pinIdx)
{
  *(m_mapping.Get()+pinIdx) = 0;
}

void ChannelPinMapper::SetPin(int pinIdx, int chIdx, bool on)
{
  if (on) {
    *(m_mapping.Get()+pinIdx) |= BITMASK64(chIdx);
  }
  else {
   *(m_mapping.Get()+pinIdx) &= ~BITMASK64(chIdx);
  }
}

bool ChannelPinMapper::TogglePin(int pinIdx, int chIdx) 
{
  bool on = GetPin(pinIdx, chIdx);
  on = !on;
  SetPin(pinIdx, chIdx, on); 
  return on;
}

bool ChannelPinMapper::GetPin(int pinIdx, int chIdx)
{
  WDL_UINT64 map = *(m_mapping.Get()+pinIdx);
  return !!(map & BITMASK64(chIdx));
}

bool ChannelPinMapper::PinHasMoreMappings(int pinIdx, int chIdx)
{
  WDL_UINT64 map = *(m_mapping.Get()+pinIdx);
  return (chIdx < 64 && map >= BITMASK64(chIdx+1));
}

bool ChannelPinMapper::IsStraightPassthrough()
{
  if (m_nCh != m_nPins) return false;
  WDL_UINT64* pMap = m_mapping.Get();
  int i;
  for (i = 0; i < m_nPins; ++i, ++pMap) {
    if (*pMap != BITMASK64(i)) return false;
  }
  return true;
}

#define PINMAPPER_MAGIC 1000

// return is on the heap
char* ChannelPinMapper::SaveStateNew(int* pLen)
{
  m_cfgret.Clear();
  int magic = PINMAPPER_MAGIC;
  WDL_Queue__AddToLE(&m_cfgret, &magic);
  WDL_Queue__AddToLE(&m_cfgret, &m_nCh);
  WDL_Queue__AddToLE(&m_cfgret, &m_nPins);
  WDL_Queue__AddDataToLE(&m_cfgret, m_mapping.Get(), m_mapping.GetSize()*sizeof(WDL_UINT64), sizeof(WDL_UINT64));
  *pLen = m_cfgret.GetSize();
  return (char*)m_cfgret.Get();
}

bool ChannelPinMapper::LoadState(char* buf, int len)
{
  WDL_Queue chunk;
  chunk.Add(buf, len);
  int* pMagic = WDL_Queue__GetTFromLE(&chunk, (int*)0);
  if (!pMagic || *pMagic != PINMAPPER_MAGIC) return false;
  int* pNCh = WDL_Queue__GetTFromLE(&chunk, (int*) 0);
  int* pNPins = WDL_Queue__GetTFromLE(&chunk, (int*) 0);
  if (!pNCh || !pNPins || !(*pNCh) || !(*pNPins)) return false;
  SetNPins(*pNCh);
  SetNChannels(*pNCh);
  int maplen = *pNPins*sizeof(WDL_UINT64);
  if (chunk.Available() < maplen) return false;
  void* pMap = WDL_Queue__GetDataFromLE(&chunk, maplen, sizeof(WDL_UINT64));
  memcpy(m_mapping.Get(), pMap, maplen);
  return true;
}

template <class TDEST, class TSRC> void BufConvertT(TDEST* dest, TSRC* src, int nFrames, int destStride, int srcStride)
{
  int i;
  for (i = 0; i < nFrames; ++i)
  {
    dest[i*destStride] = (TDEST)src[i*srcStride];
  }
}

template <class T> void BufMixT(T* dest, T* src, int nFrames, bool addToDest, double wt_start, double wt_end)
{
  int i;
  
  if (wt_start == 1.0 && wt_end == 1.0)
  {
    if (addToDest)
    {
      for (i = 0; i < nFrames; ++i)
      {
        dest[i] += src[i];    
      }
    }
    else
    {
      memcpy(dest, src, nFrames*sizeof(T));
    }  
  }
  else
  {
    double dw = (wt_end-wt_start)/(double)nFrames;
    double cw = wt_start;
   
    if (addToDest)
    {
      for (i = 0; i < nFrames; ++i)
      {
        dest[i] += (T)(1.0-cw)*dest[i]+(T)cw*src[i];
        cw += dw;  
      }
    }
    else
    {
      for (i = 0; i < nFrames; ++i)
      {
        dest[i] = (T)(1.0-cw)*dest[i]+(T)cw*src[i];
        cw += dw;  
      }
    }
  }
}

// static 
bool AudioBufferContainer::BufConvert(void* dest, void* src, int destFmt, int srcFmt, int nFrames, int destStride, int srcStride)
{
  if (destFmt == FMT_32FP)
  {
    if (srcFmt == FMT_32FP)
    {
      BufConvertT((float*)dest, (float*)src, nFrames, destStride, srcStride);
      return true;
    }
    else if (srcFmt == FMT_64FP)
    {
      BufConvertT((float*)dest, (double*)src, nFrames, destStride, srcStride);
      return true;
    }
  }
  else if (destFmt == FMT_64FP)
  {
    if (srcFmt == FMT_32FP)
    {
      BufConvertT((double*)dest, (float*)src, nFrames, destStride, srcStride);
      return true;
    }
    else if (srcFmt == FMT_64FP)
    {
      BufConvertT((double*)dest, (double*)src, nFrames, destStride, srcStride);
      return true;
    }
  }
  return false;
}

AudioBufferContainer::AudioBufferContainer()
{
  m_nCh = 0;
  m_nFrames = 0;
  m_fmt = FMT_32FP;
  m_interleaved = true;
  m_hasData = false;
}

void AudioBufferContainer::Resize(int nCh, int nFrames, bool preserveData)
{
  if (!m_hasData) 
  {
    preserveData = false;
  }

  int newsz = nCh*nFrames*(int)m_fmt;

  if (preserveData && (nCh != m_nCh || nFrames != m_nFrames))
  {
    GetAllChannels(m_fmt, true);  // causes m_data to be interleaved
  }
  
  m_data.Resize(newsz);
  m_hasData = preserveData;
  m_nCh = nCh;
  m_nFrames = nFrames;
}

void AudioBufferContainer::Reformat(int fmt, bool preserveData)
{
  if (!m_hasData) 
  {
    preserveData = false;   
  }
  
  int newsz = m_nCh*m_nFrames*(int)fmt; 
  
  if (preserveData && fmt != m_fmt)
  {
    int oldsz = m_data.GetSize();
    void* src = m_data.Resize(oldsz+newsz);
    void* dest = (unsigned char*)src+oldsz;
    BufConvert(dest, src, fmt, m_fmt, m_nCh*m_nFrames, 1, 1);
    memmove(src, dest, newsz); 
  }
  
  m_data.Resize(newsz);    
  m_hasData = preserveData;
  m_fmt = fmt;
}

// src=NULL to memset(0)
void* AudioBufferContainer::SetAllChannels(int fmt, void* src, int nCh, int nFrames)
{
  Reformat(fmt, false);
  Resize(nCh, nFrames, false);
  
  int sz = nCh*nFrames*(int)fmt;
  void* dest = GetAllChannels(fmt, false);
  if (src)
  {
    memcpy(dest, src, sz);
  }
  else
  {
    memset(dest, 0, sz);
  }
  
  m_interleaved = true;
  m_hasData = true;  
  return dest;
}

// src=NULL to memset(0)
void* AudioBufferContainer::SetChannel(int fmt, void* src, int chIdx, int nFrames)
{
  Reformat(fmt, true);
  if (nFrames > m_nFrames || chIdx >= m_nCh) 
  {
    int maxframes = (nFrames > m_nFrames ? nFrames : m_nFrames);
    Resize(chIdx+1, maxframes, true);        
  }
  
  int sz = nFrames*(int)fmt;
  void* dest = GetChannel(fmt, chIdx, true);
  if (src)
  {
    memcpy(dest, src, sz);
  }
  else
  {
    memset(dest, 0, sz);
  }
   
  m_interleaved = false;
  m_hasData = true;
  return dest;
}

void* AudioBufferContainer::MixChannel(int fmt, void* src, int chIdx, int nFrames, bool addToDest, double wt_start, double wt_end)
{
  Reformat(fmt, true);
  if (nFrames > m_nFrames || chIdx >= m_nCh) 
  {
    int maxframes = (nFrames > m_nFrames ? nFrames : m_nFrames);
    Resize(chIdx+1, maxframes, true);        
  }
  
  void* dest = GetChannel(fmt, chIdx, true);
  
  if (fmt == FMT_32FP)
  {
    BufMixT((float*)dest, (float*)src, nFrames, addToDest, wt_start, wt_end);
  }
  else if (fmt == FMT_64FP)
  {
    BufMixT((double*)dest, (double*)src, nFrames, addToDest, wt_start, wt_end);
  }
  
  m_interleaved = false;
  m_hasData = true;
  return dest;
}


void* AudioBufferContainer::GetAllChannels(int fmt, bool preserveData)
{
  Reformat(fmt, preserveData);
  ReLeave(true, preserveData);
  
  m_hasData = true;   // because caller may use the returned pointer to populate the container
  
  return m_data.Get();
}

void* AudioBufferContainer::GetChannel(int fmt, int chIdx, bool preserveData)
{
  Reformat(fmt, preserveData); 
  if (chIdx >= m_nCh)
  {
    Resize(chIdx+1, m_nFrames, true);
  }
  ReLeave(false, preserveData);
  
  m_hasData = true;   // because caller may use the returned pointer to populate the container
  
  int offsz = chIdx*m_nFrames*(int)fmt;
  return (unsigned char*)m_data.Get()+offsz;
}

void AudioBufferContainer::ReLeave(bool interleave, bool preserveData)
{
  if (interleave != m_interleaved && preserveData && m_hasData)
  {
    int elemsz = (int)m_fmt;
    int chansz = m_nFrames*elemsz;
    int bufsz = m_nCh*chansz;
    int i;

    unsigned char* src = (unsigned char*)m_data.Resize(bufsz*2);
    unsigned char* dest = src+bufsz;    
    
    if (interleave)
    { 
      for (i = 0; i < m_nCh; ++i)
      {
        BufConvert((void*)(dest+i*elemsz), (void*)(src+i*chansz), m_fmt, m_fmt, m_nFrames, m_nCh, 1);
      }
    }
    else
    {
      for (i = 0; i < m_nCh; ++i)
      {
        BufConvert((void*)(dest+i*chansz), (void*)(src+i*elemsz), m_fmt, m_fmt, m_nFrames, 1, m_nCh);
      }
    }
    
    memcpy(src, dest, bufsz); // no overlap
    m_data.Resize(bufsz);
  }
  
  m_hasData = preserveData;
  m_interleaved = interleave;
}

void AudioBufferContainer::CopyFrom(AudioBufferContainer* rhs)
{
  int sz = rhs->m_data.GetSize();
  void* dest = m_data.Resize(sz);    
  
  if (rhs->m_hasData)
  {
    void* src = rhs->m_data.Get();
    memcpy(dest, src, sz);
  }

  m_nCh = rhs->m_nCh;
  m_nFrames = rhs->m_nFrames;
  m_fmt = rhs->m_fmt;
  m_interleaved = rhs->m_interleaved;
  m_hasData = rhs->m_hasData;
}


void SetPinsFromChannels(AudioBufferContainer* dest, AudioBufferContainer* src, ChannelPinMapper* mapper)
{
  if (mapper->IsStraightPassthrough())
  {
    dest->CopyFrom(src);
    return;
  }

  int nch = mapper->GetNChannels();
  int npins = mapper->GetNPins();
  int nframes = src->GetNFrames();
  int fmt = src->GetFormat();
  
  dest->Resize(npins, nframes, false);
  
  int c, p;
  for (p = 0; p < npins; ++p)
  {
    bool pinused = false;  
    for (c = 0; c < nch; ++c)
    {
      if (mapper->GetPin(p, c))
      {
        void* srcbuf = src->GetChannel(fmt, c, true);
        dest->MixChannel(fmt, srcbuf, p, nframes, pinused, 1.0, 1.0);
        pinused = true;
        
        if (!mapper->PinHasMoreMappings(p, c))
        {
          break;
        }
      }
    }
    
    if (!pinused)
    {
      dest->SetChannel(fmt, 0, p, nframes);   // clear unused pins
    }
  }
}

void SetChannelsFromPins(AudioBufferContainer* dest, AudioBufferContainer* src, ChannelPinMapper* mapper, double wt_start, double wt_end)
{
  if (wt_start == 1.0 && wt_end == 1.0 && mapper->IsStraightPassthrough())
  {
    dest->CopyFrom(src);
    return;
  }   

  int nch = mapper->GetNChannels();
  int npins = mapper->GetNPins();
  int nframes = src->GetNFrames();
  int fmt = src->GetFormat();

  dest->Resize(nch, nframes, true);
  
  int c, p;
  for (c = 0; c < nch; ++c) 
  {
    bool chanused = false;
    for (p = 0; p < npins; ++p) 
    {
      if (mapper->GetPin(p, c)) 
      {
        void* srcbuf = src->GetChannel(fmt, p, true);
        dest->MixChannel(fmt, srcbuf, c, nframes, chanused, wt_start, wt_end);        
        chanused = true;
      }
    }
    // don't clear unused channels
  }
}



