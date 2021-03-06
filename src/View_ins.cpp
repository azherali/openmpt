#include "stdafx.h"
#include "mainfrm.h"
#include "childfrm.h"
#include "moddoc.h"
#include "globals.h"
#include "ctrl_ins.h"
#include "view_ins.h"
#include "legacy_soundlib/dlsbank.h"
#include "channelManagerDlg.h"
#include "ScaleEnvPointsDlg.h"
#include "view_ins.h"
#include "legacy_soundlib/midi.h"

#include "tracker/types.h"

using namespace modplug::tracker;

#define ENV_ZOOM                            4.0f
#define ENV_MIN_ZOOM                    2.0f
#define ENV_MAX_ZOOM                    100.0f
#define ENV_DRAGLOOPSTART            (MAX_ENVPOINTS + 1)
#define ENV_DRAGLOOPEND                    (MAX_ENVPOINTS + 2)
#define ENV_DRAGSUSTAINSTART    (MAX_ENVPOINTS + 3)
#define ENV_DRAGSUSTAINEND            (MAX_ENVPOINTS + 4)

// Non-client toolbar
#define ENV_LEFTBAR_CY                    29
#define ENV_LEFTBAR_CXSEP            14
#define ENV_LEFTBAR_CXSPC            3
#define ENV_LEFTBAR_CXBTN            24
#define ENV_LEFTBAR_CYBTN            22


const UINT cLeftBarButtons[ENV_LEFTBAR_BUTTONS] =
{
    ID_ENVSEL_VOLUME,
    ID_ENVSEL_PANNING,
    ID_ENVSEL_PITCH,
            ID_SEPARATOR,
    ID_ENVELOPE_VOLUME,
    ID_ENVELOPE_PANNING,
    ID_ENVELOPE_PITCH,
    ID_ENVELOPE_FILTER,
            ID_SEPARATOR,
    ID_ENVELOPE_SETLOOP,
    ID_ENVELOPE_SUSTAIN,
    ID_ENVELOPE_CARRY,
            ID_SEPARATOR,
    ID_INSTRUMENT_SAMPLEMAP,
            ID_SEPARATOR,
    ID_ENVELOPE_VIEWGRID,                        //rewbs.envRowGrid
            ID_SEPARATOR,
    ID_ENVELOPE_ZOOM_IN,
    ID_ENVELOPE_ZOOM_OUT,
};


IMPLEMENT_SERIAL(CViewInstrument, CModScrollView, 0)

BEGIN_MESSAGE_MAP(CViewInstrument, CModScrollView)
    //{{AFX_MSG_MAP(CViewInstrument)
    ON_WM_ERASEBKGND()
    ON_WM_SIZE()
    ON_WM_NCCALCSIZE()
    ON_WM_NCPAINT()
    ON_WM_NCHITTEST()
    ON_WM_MOUSEMOVE()
    ON_WM_NCMOUSEMOVE()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONUP()
    ON_WM_RBUTTONDOWN()
    ON_WM_MBUTTONDOWN()
    ON_WM_NCLBUTTONDOWN()
    ON_WM_NCLBUTTONUP()
    ON_WM_NCLBUTTONDBLCLK()
    ON_WM_CHAR()
    ON_WM_KEYUP()
    ON_WM_DROPFILES()
    ON_COMMAND(ID_PREVINSTRUMENT,                        OnPrevInstrument)
    ON_COMMAND(ID_NEXTINSTRUMENT,                        OnNextInstrument)
    ON_COMMAND(ID_ENVELOPE_SETLOOP,                        OnEnvLoopChanged)
    ON_COMMAND(ID_ENVELOPE_SUSTAIN,                        OnEnvSustainChanged)
    ON_COMMAND(ID_ENVELOPE_CARRY,                        OnEnvCarryChanged)
    ON_COMMAND(ID_ENVELOPE_INSERTPOINT,                OnEnvInsertPoint)
    ON_COMMAND(ID_ENVELOPE_REMOVEPOINT,                OnEnvRemovePoint)
    ON_COMMAND(ID_ENVELOPE_VOLUME,                        OnEnvVolChanged)
    ON_COMMAND(ID_ENVELOPE_PANNING,                        OnEnvPanChanged)
    ON_COMMAND(ID_ENVELOPE_PITCH,                        OnEnvPitchChanged)
    ON_COMMAND(ID_ENVELOPE_FILTER,                        OnEnvFilterChanged)
    ON_COMMAND(ID_ENVELOPE_VIEWGRID,                OnEnvToggleGrid) //rewbs.envRowGrid
    ON_COMMAND(ID_ENVELOPE_ZOOM_IN,                        OnEnvZoomIn)
    ON_COMMAND(ID_ENVELOPE_ZOOM_OUT,                OnEnvZoomOut)
    ON_COMMAND(ID_ENVSEL_VOLUME,                        OnSelectVolumeEnv)
    ON_COMMAND(ID_ENVSEL_PANNING,                        OnSelectPanningEnv)
    ON_COMMAND(ID_ENVSEL_PITCH,                                OnSelectPitchEnv)
    ON_COMMAND(ID_EDIT_COPY,                                OnEditCopy)
    ON_COMMAND(ID_EDIT_PASTE,                                OnEditPaste)
    ON_COMMAND(ID_INSTRUMENT_SAMPLEMAP,                OnEditSampleMap)
    ON_MESSAGE(WM_MOD_MIDIMSG,                                OnMidiMsg)
    ON_COMMAND(ID_ENVELOPE_TOGGLERELEASENODE, OnEnvToggleReleasNode)
    //}}AFX_MSG_MAP
    ON_COMMAND(ID_ENVELOPE_SCALEPOINTS, OnEnvelopeScalepoints)
    ON_WM_MOUSEWHEEL()
END_MESSAGE_MAP()


///////////////////////////////////////////////////////////////
// CViewInstrument operations

CViewInstrument::CViewInstrument()
//--------------------------------
{
    m_nInstrument = 1;
    m_nEnv = ENV_VOLUME;
    m_rcClient.bottom = 2;
    m_dwStatus = 0;
    m_nBtnMouseOver = 0xFFFF;
    memset(m_dwNotifyPos, 0, sizeof(m_dwNotifyPos));
    memset(m_NcButtonState, 0, sizeof(m_NcButtonState));
    m_bmpEnvBar.Create(IDB_ENVTOOLBAR, 20, 0, RGB(192,192,192));
    memset(m_baPlayingNote, 0, sizeof(bool)*NoteMax);        //rewbs.customKeys
    m_nPlayingChannel = ChannelIndexInvalid;                        //rewbs.customKeys
    //rewbs.envRowGrid
    m_bGrid=true;
    m_bGridForceRedraw=false;
    m_GridSpeed = -1;
    m_GridScrollPos = -1;
    //end rewbs.envRowGrid
    m_nDragItem = 1;
    m_fZoom = ENV_ZOOM;
}


void CViewInstrument::OnInitialUpdate()
//-------------------------------------
{
    ModifyStyleEx(0, WS_EX_ACCEPTFILES);
    CScrollView::OnInitialUpdate();
    UpdateScrollSize();
}


void CViewInstrument::UpdateScrollSize()
//--------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    GetClientRect(&m_rcClient);
    if (m_rcClient.bottom < 2) m_rcClient.bottom = 2;
    if (pModDoc)
    {
            SIZE sizeTotal, sizePage, sizeLine;
            UINT ntickmax = EnvGetTick(EnvGetLastPoint());

            sizeTotal.cx = (INT)((ntickmax + 2) * m_fZoom);
            sizeTotal.cy = 1;
            sizeLine.cx = (INT)m_fZoom;
            sizeLine.cy = 2;
            sizePage.cx = sizeLine.cx * 4;
            sizePage.cy = sizeLine.cy;
            SetScrollSizes(MM_TEXT, sizeTotal, sizePage, sizeLine);
    }
}


// Set instrument (and moddoc) as modified.
void CViewInstrument::SetInstrumentModified()
//-------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return;
// -> CODE#0023
// -> DESC="IT project files (.itp)"
    pModDoc->m_bsInstrumentModified.set(m_nInstrument - 1, true);
    pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_INSNAMES, this);
// -! NEW_FEATURE#0023
    pModDoc->SetModified();
}


BOOL CViewInstrument::SetCurrentInstrument(modplug::tracker::instrumentindex_t nIns, enmEnvelopeTypes nEnv)
//-------------------------------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    uint32_t dwNotify;

    if ((!pModDoc) || (nIns < 1) || (nIns >= MAX_INSTRUMENTS)) return FALSE;
    if (nEnv) m_nEnv = nEnv;
    m_nInstrument = nIns;
    switch(m_nEnv)
    {
    case ENV_PANNING:        dwNotify = MPTNOTIFY_PANENV; break;
    case ENV_PITCH:                dwNotify = MPTNOTIFY_PITCHENV; break;
    default:                        m_nEnv = ENV_VOLUME; dwNotify = MPTNOTIFY_VOLENV; break;
    }
    pModDoc->SetFollowWnd(m_hWnd, dwNotify|m_nInstrument);
    UpdateScrollSize();
    UpdateNcButtonState();
    InvalidateRect(NULL, FALSE);
    return TRUE;
}


LRESULT CViewInstrument::OnModViewMsg(WPARAM wParam, LPARAM lParam)
//-----------------------------------------------------------------
{
    switch(wParam)
    {
    case VIEWMSG_SETCURRENTINSTRUMENT:
            SetCurrentInstrument(lParam & 0xFFFF, m_nEnv);
            break;

    case VIEWMSG_LOADSTATE:
            if (lParam)
            {
                    INSTRUMENTVIEWSTATE *pState = (INSTRUMENTVIEWSTATE *)lParam;
                    if (pState->cbStruct == sizeof(INSTRUMENTVIEWSTATE))
                    {
                            SetCurrentInstrument(m_nInstrument, pState->nEnv);
                            m_bGrid = pState->bGrid;
                    }
            }
            break;

    case VIEWMSG_SAVESTATE:
            if (lParam)
            {
                    INSTRUMENTVIEWSTATE *pState = (INSTRUMENTVIEWSTATE *)lParam;
                    pState->cbStruct = sizeof(INSTRUMENTVIEWSTATE);
                    pState->nEnv = m_nEnv;
                    pState->bGrid = m_bGrid;
            }
            break;

    default:
            return CModScrollView::OnModViewMsg(wParam, lParam);
    }
    return 0;
}


UINT CViewInstrument::EnvGetTick(int nPoint) const
//------------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    if((nPoint >= 0) && (nPoint < (int)envelope->num_nodes))
            return envelope->Ticks[nPoint];
    else
            return 0;
}


UINT CViewInstrument::EnvGetValue(int nPoint) const
//-------------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    if(nPoint >= 0 && nPoint < (int)envelope->num_nodes)
            return envelope->Values[nPoint];
    else
            return 0;
}


bool CViewInstrument::EnvSetValue(int nPoint, int nTick, int nValue)
//------------------------------------------------------------------
{
    if(nPoint < 0) return false;

    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;

    bool bOK = false;
    if (!nPoint) nTick = 0;
    if (nPoint < (int)envelope->num_nodes)
    {
            if (nTick >= 0)
            {
                    int mintick = (nPoint) ? envelope->Ticks[nPoint - 1] : 0;
                    int maxtick = envelope->Ticks[nPoint + 1];
                    if (nPoint + 1 == (int)envelope->num_nodes) maxtick = ENVELOPE_MAX_LENGTH;
                    // Can't have multiple points on same tick
                    if(GetDocument()->GetSoundFile()->IsCompatibleMode(TRK_IMPULSETRACKER|TRK_FASTTRACKER2) && mintick < maxtick - 1)
                    {
                            mintick++;
                            if (nPoint + 1 < (int)envelope->num_nodes) maxtick--;
                    }
                    if (nTick < mintick) nTick = mintick;
                    if (nTick > maxtick) nTick = maxtick;
                    if (nTick != envelope->Ticks[nPoint])
                    {
                            envelope->Ticks[nPoint] = (uint16_t)nTick;
                            bOK = true;
                    }
            }
            if (nValue >= 0)
            {
                    if (nValue > 64) nValue = 64;
                    if (nValue != envelope->Values[nPoint])
                    {
                            envelope->Values[nPoint] = (uint8_t)nValue;
                            bOK = true;
                    }
            }
    }
    return bOK;
}


UINT CViewInstrument::EnvGetNumPoints() const
//-------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->num_nodes;
}


UINT CViewInstrument::EnvGetLastPoint() const
//-------------------------------------------
{
    UINT nPoints = EnvGetNumPoints();
    if (nPoints > 0) return nPoints - 1;
    return 0;
}


// Return if an envelope flag is set.
bool CViewInstrument::EnvGetFlag(const uint32_t dwFlag) const
//--------------------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv != nullptr && pEnv->flags & dwFlag) return true;
    return false;
}


UINT CViewInstrument::EnvGetLoopStart() const
//-------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->loop_start;
}


UINT CViewInstrument::EnvGetLoopEnd() const
//-----------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->loop_end;
}


UINT CViewInstrument::EnvGetSustainStart() const
//----------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->sustain_start;
}


UINT CViewInstrument::EnvGetSustainEnd() const
//--------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->sustain_end;
}


bool CViewInstrument::EnvGetVolEnv() const
//----------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if (pIns) return (pIns->volume_envelope.flags & ENV_ENABLED) ? true : false;
    return false;
}


bool CViewInstrument::EnvGetPanEnv() const
//----------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if (pIns) return (pIns->panning_envelope.flags & ENV_ENABLED) ? true : false;
    return false;
}


bool CViewInstrument::EnvGetPitchEnv() const
//------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if (pIns) return ((pIns->pitch_envelope.flags & (ENV_ENABLED|ENV_FILTER)) == ENV_ENABLED) ? true : false;
    return false;
}


bool CViewInstrument::EnvGetFilterEnv() const
//-------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if (pIns) return ((pIns->pitch_envelope.flags & (ENV_ENABLED|ENV_FILTER)) == (ENV_ENABLED|ENV_FILTER)) ? true : false;
    return false;
}


bool CViewInstrument::EnvSetLoopStart(int nPoint)
//-----------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;
    if(nPoint < 0 || nPoint > (int)EnvGetLastPoint()) return false;

    if (nPoint != envelope->loop_start)
    {
            envelope->loop_start = (uint8_t)nPoint;
            if (envelope->loop_end < nPoint) envelope->loop_end = (uint8_t)nPoint;
            return true;
    } else
    {
            return false;
    }
}


bool CViewInstrument::EnvSetLoopEnd(int nPoint)
//---------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;
    if(nPoint < 0 || nPoint > (int)EnvGetLastPoint()) return false;

    if (nPoint != envelope->loop_end)
    {
            envelope->loop_end = (uint8_t)nPoint;
            if (envelope->loop_start > nPoint) envelope->loop_start = (uint8_t)nPoint;
            return true;
    } else
    {
            return false;
    }
}


bool CViewInstrument::EnvSetSustainStart(int nPoint)
//--------------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;
    if(nPoint < 0 || nPoint > (int)EnvGetLastPoint()) return false;

    // We won't do any security checks here as GetEnvelopePtr() does that for us.
    module_renderer *pSndFile = GetDocument()->GetSoundFile();

    if (nPoint != envelope->sustain_start)
    {
            envelope->sustain_start = (uint8_t)nPoint;
            if ((envelope->sustain_end < nPoint) || (pSndFile->m_nType & MOD_TYPE_XM)) envelope->sustain_end = (uint8_t)nPoint;
            return true;
    } else
    {
            return false;
    }
}


bool CViewInstrument::EnvSetSustainEnd(int nPoint)
//------------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;
    if(nPoint < 0 || nPoint > (int)EnvGetLastPoint()) return false;

    // We won't do any security checks here as GetEnvelopePtr() does that for us.
    module_renderer *pSndFile = GetDocument()->GetSoundFile();

    if (nPoint != envelope->sustain_end)
    {
            envelope->sustain_end = (uint8_t)nPoint;
            if ((envelope->sustain_start > nPoint) || (pSndFile->m_nType & MOD_TYPE_XM)) envelope->sustain_start = (uint8_t)nPoint;
            return true;
    } else
    {
            return false;
    }
}


bool CViewInstrument::EnvToggleReleaseNode(int nPoint)
//----------------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;
    if(nPoint < 1 || nPoint > (int)EnvGetLastPoint()) return false;

    // Don't allow release nodes in IT/XM. GetDocument()/... nullptr check is done in GetEnvelopePtr, so no need to check twice.
    if(!GetDocument()->GetSoundFile()->GetModSpecifications().hasReleaseNode)
    {
            if(envelope->release_node != ENV_RELEASE_NODE_UNSET)
            {
                    envelope->release_node = ENV_RELEASE_NODE_UNSET;
                    return true;
            }
            return false;
    }

    if (envelope->release_node == nPoint)
    {
            envelope->release_node = ENV_RELEASE_NODE_UNSET;
    } else
    {
            envelope->release_node = static_cast<uint8_t>(nPoint);
    }
    return true;
}

// Enable or disable a flag of the current envelope
bool CViewInstrument::EnvSetFlag(const uint32_t dwFlag, const bool bEnable) const
//----------------------------------------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return false;
    if(bEnable)
    {
            if(!(envelope->flags & dwFlag))
            {
                    envelope->flags |= dwFlag;
                    return true;
            }
    } else
    {
            if(envelope->flags & dwFlag)
            {
                    envelope->flags &= ~dwFlag;
                    return true;
            }
    }
    return false;
}


bool CViewInstrument::EnvToggleEnv(modplug::tracker::modenvelope_t *pEnv, module_renderer *pSndFile, modplug::tracker::modinstrument_t *pIns, bool bEnable, uint8_t cDefaultValue, uint32_t dwChanFlag, uint32_t dwExtraFlags)
//-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
{
    if(pEnv == nullptr) return false;

    if (bEnable)
    {
            pEnv->flags |= (ENV_ENABLED|dwExtraFlags);
            if (!pEnv->num_nodes)
            {
                    pEnv->Values[0] = pEnv->Values[1] = cDefaultValue;
                    pEnv->Ticks[0] = 0;
                    pEnv->Ticks[1] = 10;
                    pEnv->num_nodes = 2;
                    InvalidateRect(NULL, FALSE);
            }
    } else
    {
            pEnv->flags &= ~(ENV_ENABLED|dwExtraFlags);
            for(modplug::tracker::chnindex_t nChn = 0; nChn < MAX_VIRTUAL_CHANNELS; nChn++)
            {
                    if(pSndFile->Chn[nChn].instrument == pIns)
                            pSndFile->Chn[nChn].flags &= ~dwChanFlag;
            }
    }
    return true;
}


bool CViewInstrument::EnvSetVolEnv(bool bEnable)
//----------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if(pIns == nullptr) return false;
    module_renderer *pSndFile = GetDocument()->GetSoundFile(); // security checks are done in GetInstrumentPtr()

    return EnvToggleEnv(&pIns->volume_envelope, pSndFile, pIns, bEnable, 64, CHN_VOLENV);
}


bool CViewInstrument::EnvSetPanEnv(bool bEnable)
//----------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if(pIns == nullptr) return false;
    module_renderer *pSndFile = GetDocument()->GetSoundFile(); // security checks are done in GetInstrumentPtr()

    return EnvToggleEnv(&pIns->panning_envelope, pSndFile, pIns, bEnable, 32, CHN_PANENV);
}


bool CViewInstrument::EnvSetPitchEnv(bool bEnable)
//------------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if(pIns == nullptr) return false;
    module_renderer *pSndFile = GetDocument()->GetSoundFile(); // security checks are done in GetInstrumentPtr()

    pIns->pitch_envelope.flags &= ~ENV_FILTER;
    return EnvToggleEnv(&pIns->pitch_envelope, pSndFile, pIns, bEnable, 32, CHN_PITCHENV);
}


bool CViewInstrument::EnvSetFilterEnv(bool bEnable)
//-------------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if(pIns == nullptr) return false;
    module_renderer *pSndFile = GetDocument()->GetSoundFile(); // security checks are done in GetInstrumentPtr()

    pIns->pitch_envelope.flags &= ~ENV_FILTER;
    return EnvToggleEnv(&pIns->pitch_envelope, pSndFile, pIns, bEnable, 64, CHN_PITCHENV, ENV_FILTER);
}


int CViewInstrument::TickToScreen(int nTick) const
//------------------------------------------------
{
    return ((int)((nTick + 1) * m_fZoom)) - GetScrollPos(SB_HORZ);
}

int CViewInstrument::PointToScreen(int nPoint) const
//--------------------------------------------------
{
    return TickToScreen(EnvGetTick(nPoint));
}


int CViewInstrument::ScreenToTick(int x) const
//--------------------------------------------
{
    return (int)(((float)GetScrollPos(SB_HORZ) + (float)x + 1 - m_fZoom) / m_fZoom);
}


int CViewInstrument::QuickScreenToTick(int x, int cachedScrollPos) const
//----------------------------------------------------------------------
{
    return (int)(((float)cachedScrollPos + (float)x + 1 - m_fZoom) / m_fZoom);
}

int CViewInstrument::ScreenToValue(int y) const
//---------------------------------------------
{
    if (m_rcClient.bottom < 2) return 0;
    int n = 64 - ((y * 64 + 1) / (m_rcClient.bottom - 1));
    if (n < 0) return 0;
    if (n > 64) return 64;
    return n;
}


int CViewInstrument::ScreenToPoint(int x0, int y0) const
//------------------------------------------------------
{
    int nPoint = -1;
    int ydist = 0xFFFF, xdist = 0xFFFF;
    int maxpoint = EnvGetLastPoint();
    for (int i=0; i<=maxpoint; i++)
    {
            int dx = x0 - PointToScreen(i);
            int dx2 = dx*dx;
            if (dx2 <= xdist)
            {
                    int dy = y0 - ValueToScreen(EnvGetValue(i));
                    int dy2 = dy*dy;
                    if ((dx2 < xdist) || ((dx2 == xdist) && (dy2 < ydist)))
                    {
                            nPoint = i;
                            xdist = dx2;
                            ydist = dy2;
                    }
            }
    }
    return nPoint;
}


BOOL CViewInstrument::GetNcButtonRect(UINT nBtn, LPRECT lpRect)
//-------------------------------------------------------------
{
    lpRect->left = 4;
    lpRect->top = 3;
    lpRect->bottom = lpRect->top + ENV_LEFTBAR_CYBTN;
    if (nBtn >= ENV_LEFTBAR_BUTTONS) return FALSE;
    for (UINT i=0; i<nBtn; i++)
    {
            if (cLeftBarButtons[i] == ID_SEPARATOR)
            {
                    lpRect->left += ENV_LEFTBAR_CXSEP;
            } else
            {
                    lpRect->left += ENV_LEFTBAR_CXBTN + ENV_LEFTBAR_CXSPC;
            }
    }
    if (cLeftBarButtons[nBtn] == ID_SEPARATOR)
    {
            lpRect->left += ENV_LEFTBAR_CXSEP/2 - 2;
            lpRect->right = lpRect->left + 2;
            return FALSE;
    } else
    {
            lpRect->right = lpRect->left + ENV_LEFTBAR_CXBTN;
    }
    return TRUE;
}


void CViewInstrument::UpdateNcButtonState()
//-----------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    CDC *pDC = NULL;

    if (!pModDoc) return;
    pSndFile = pModDoc->GetSoundFile();
    for (UINT i=0; i<ENV_LEFTBAR_BUTTONS; i++) if (cLeftBarButtons[i] != ID_SEPARATOR)
    {
            uint32_t dwStyle = 0;

            switch(cLeftBarButtons[i])
            {
            case ID_ENVSEL_VOLUME:                if (m_nEnv == ENV_VOLUME) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVSEL_PANNING:                if (m_nEnv == ENV_PANNING) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVSEL_PITCH:                if (!(pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) dwStyle |= NCBTNS_DISABLED;
                                                                    else if (m_nEnv == ENV_PITCH) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_SETLOOP:        if (EnvGetLoop()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_SUSTAIN:        if (EnvGetSustain()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_CARRY:                if (!(pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) dwStyle |= NCBTNS_DISABLED;
                                                                    else if (EnvGetCarry()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_VOLUME:        if (EnvGetVolEnv()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_PANNING:        if (EnvGetPanEnv()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_PITCH:                if (!(pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) dwStyle |= NCBTNS_DISABLED; else
                                                                    if (EnvGetPitchEnv()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_FILTER:        if (!(pSndFile->m_nType & (MOD_TYPE_IT|MOD_TYPE_MPT))) dwStyle |= NCBTNS_DISABLED; else
                                                                    if (EnvGetFilterEnv()) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_VIEWGRID:        if (m_bGrid) dwStyle |= NCBTNS_CHECKED; break;
            case ID_ENVELOPE_ZOOM_IN:        if (m_fZoom >= ENV_MAX_ZOOM) dwStyle |= NCBTNS_DISABLED; break;
            case ID_ENVELOPE_ZOOM_OUT:        if (m_fZoom <= ENV_MIN_ZOOM) dwStyle |= NCBTNS_DISABLED; break;
            }
            if (m_nBtnMouseOver == i)
            {
                    dwStyle |= NCBTNS_MOUSEOVER;
                    if (m_dwStatus & INSSTATUS_NCLBTNDOWN) dwStyle |= NCBTNS_PUSHED;
            }
            if (dwStyle != m_NcButtonState[i])
            {
                    m_NcButtonState[i] = dwStyle;
                    if (!pDC) pDC = GetWindowDC();
                    DrawNcButton(pDC, i);
            }
    }
    if (pDC) ReleaseDC(pDC);
}


////////////////////////////////////////////////////////////////////
// CViewInstrument drawing

void CViewInstrument::UpdateView(uint32_t dwHintMask, CObject *)
//-----------------------------------------------------------
{
    if ((dwHintMask & (HINT_MPTOPTIONS|HINT_MODTYPE))
     || ((dwHintMask & HINT_ENVELOPE) && (m_nInstrument == (dwHintMask >> HINT_SHIFT_INS)))
     || ((dwHintMask & HINT_SPEEDCHANGE))) //rewbs.envRowGrid
    {
            UpdateScrollSize();
            UpdateNcButtonState();
            InvalidateRect(NULL, FALSE);
    }
}

//rewbs.envRowGrid
void CViewInstrument::DrawGrid(CDC *pDC, UINT speed)
//--------------------------------------------------
{
    int cachedScrollPos = GetScrollPos(SB_HORZ);
    bool windowResized = false;

    if (m_dcGrid.GetSafeHdc())
    {
            m_dcGrid.SelectObject(m_pbmpOldGrid);
            m_dcGrid.DeleteDC();
            m_bmpGrid.DeleteObject();
            windowResized = true;
    }


    if (windowResized || m_bGridForceRedraw || (cachedScrollPos != m_GridScrollPos) || (speed != (UINT)m_GridSpeed))
    {

            m_GridSpeed = speed;
            m_GridScrollPos = cachedScrollPos;
            m_bGridForceRedraw = false;

            // create a memory based dc for drawing the grid
            m_dcGrid.CreateCompatibleDC(pDC);
            m_bmpGrid.CreateCompatibleBitmap(pDC,  m_rcClient.right-m_rcClient.left, m_rcClient.bottom-m_rcClient.top);
            m_pbmpOldGrid = m_dcGrid.SelectObject(&m_bmpGrid);

            //do draw
            module_renderer *pSndFile;
            int width = m_rcClient.right - m_rcClient.left;
            int nPrevTick = -1;
            int nTick, nRow;
            int nRowsPerBeat = 1, nRowsPerMeasure = 1;
            if(GetDocument() != nullptr && (pSndFile = GetDocument()->GetSoundFile()) != nullptr)
            {
                    nRowsPerBeat = pSndFile->m_nDefaultRowsPerBeat;
                    nRowsPerMeasure = pSndFile->m_nDefaultRowsPerMeasure;
            }

            for (int x = 3; x < width; x++)
            {
                    nTick = QuickScreenToTick(x, cachedScrollPos);
                    if (nTick != nPrevTick && !(nTick%speed))
                    {
                            nPrevTick = nTick;
                            nRow = nTick / speed;

                            if (nRow % bad_max(1, nRowsPerMeasure) == 0)
                                    m_dcGrid.SelectObject(CMainFrame::penGray80);
                            else if (nRow % bad_max(1, nRowsPerBeat) == 0)
                                    m_dcGrid.SelectObject(CMainFrame::penGray55);
                            else
                                    m_dcGrid.SelectObject(CMainFrame::penGray33);

                            m_dcGrid.MoveTo(x+1, 0);
                            m_dcGrid.LineTo(x+1, m_rcClient.bottom);

                    }
            }
    }

    pDC->BitBlt(m_rcClient.left, m_rcClient.top, m_rcClient.right-m_rcClient.left, m_rcClient.bottom-m_rcClient.top, &m_dcGrid, 0, 0, SRCCOPY);
}
//end rewbs.envRowGrid

void CViewInstrument::OnDraw(CDC *pDC)
//------------------------------------
{
    RECT rect;
    int nScrollPos = GetScrollPos(SB_HORZ);
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    HGDIOBJ oldpen;
    //HDC hdc;
    UINT maxpoint;
    int ymed = (m_rcClient.bottom - 1) / 2;

//rewbs.envRowGrid
    // to avoid flicker, establish a memory dc, draw to it
    // and then BitBlt it to the destination "pDC"

    //check for window resize
    if (m_dcMemMain.GetSafeHdc()) {
            m_dcMemMain.SelectObject(oldBitmap);
            m_dcMemMain.DeleteDC();
            m_bmpMemMain.DeleteObject();
    }

    m_dcMemMain.CreateCompatibleDC(pDC);
    if (!m_dcMemMain)
            return;
    m_bmpMemMain.CreateCompatibleBitmap(pDC, m_rcClient.right-m_rcClient.left, m_rcClient.bottom-m_rcClient.top);
    oldBitmap = (CBitmap *)m_dcMemMain.SelectObject(&m_bmpMemMain);

//end rewbs.envRowGrid

    if ((!pModDoc) || (!pDC)) return;
    pSndFile = pModDoc->GetSoundFile();
    //hdc = pDC->m_hDC;
    oldpen = m_dcMemMain.SelectObject(CMainFrame::penDarkGray);
    m_dcMemMain.FillRect(&m_rcClient, CBrush::FromHandle(CMainFrame::brushBlack));
    if (m_bGrid)
    {
            DrawGrid(&m_dcMemMain, pSndFile->m_nMusicSpeed);
    }

    // Middle line (half volume or pitch / panning center)
    m_dcMemMain.MoveTo(0, ymed);
    m_dcMemMain.LineTo(m_rcClient.right, ymed);

    m_dcMemMain.SelectObject(CMainFrame::penDarkGray);
    // Drawing Loop Start/End
    if (EnvGetLoop())
    {
            int x1 = PointToScreen(EnvGetLoopStart()) - (int)(m_fZoom / 2);
            m_dcMemMain.MoveTo(x1, 0);
            m_dcMemMain.LineTo(x1, m_rcClient.bottom);
            int x2 = PointToScreen(EnvGetLoopEnd()) + (int)(m_fZoom / 2);
            m_dcMemMain.MoveTo(x2, 0);
            m_dcMemMain.LineTo(x2, m_rcClient.bottom);
    }
    // Drawing Sustain Start/End
    if (EnvGetSustain())
    {
            m_dcMemMain.SelectObject(CMainFrame::penHalfDarkGray);
            int nspace = m_rcClient.bottom/4;
            int n1 = EnvGetSustainStart();
            int x1 = PointToScreen(n1) - (int)(m_fZoom / 2);
            int y1 = ValueToScreen(EnvGetValue(n1));
            m_dcMemMain.MoveTo(x1, y1 - nspace);
            m_dcMemMain.LineTo(x1, y1+nspace);
            int n2 = EnvGetSustainEnd();
            int x2 = PointToScreen(n2) + (int)(m_fZoom / 2);
            int y2 = ValueToScreen(EnvGetValue(n2));
            m_dcMemMain.MoveTo(x2, y2-nspace);
            m_dcMemMain.LineTo(x2, y2+nspace);
    }
    maxpoint = EnvGetNumPoints();
    // Drawing Envelope
    if (maxpoint)
    {
            maxpoint--;
            m_dcMemMain.SelectObject(CMainFrame::penEnvelope);
            UINT releaseNode = EnvGetReleaseNode();
            for (UINT i=0; i<=maxpoint; i++)
            {
                    int x = (int)((EnvGetTick(i) + 1) * m_fZoom) - nScrollPos;
                    int y = ValueToScreen(EnvGetValue(i));
                    rect.left = x - 3;
                    rect.top = y - 3;
                    rect.right = x + 4;
                    rect.bottom = y + 4;
                    if (i)
                    {
                            m_dcMemMain.LineTo(x, y);
                    } else
                    {
                            m_dcMemMain.MoveTo(x, y);
                    }
                    if (i == releaseNode)
                    {
                            m_dcMemMain.FrameRect(&rect, CBrush::FromHandle(CMainFrame::brushHighLightRed));
                            m_dcMemMain.SelectObject(CMainFrame::penEnvelopeHighlight);
                    } else if (i == m_nDragItem - 1)
                    {
                            // currently selected env point
                            m_dcMemMain.FrameRect(&rect, CBrush::FromHandle(CMainFrame::brushYellow));
                    } else
                    {
                            m_dcMemMain.FrameRect(&rect, CBrush::FromHandle(CMainFrame::brushWhite));
                    }

            }
    }
    DrawPositionMarks(m_dcMemMain.m_hDC);
    if (oldpen) m_dcMemMain.SelectObject(oldpen);

    //rewbs.envRowGrid
    pDC->BitBlt(m_rcClient.left, m_rcClient.top, m_rcClient.right-m_rcClient.left, m_rcClient.bottom-m_rcClient.top, &m_dcMemMain, 0, 0, SRCCOPY);

// -> CODE#0015
// -> DESC="channels management dlg"
    CMainFrame * pMainFrm = CMainFrame::GetMainFrame();
    BOOL activeDoc = pMainFrm ? pMainFrm->GetActiveDoc() == GetDocument() : FALSE;

    if(activeDoc && CChannelManagerDlg::sharedInstance(FALSE) && CChannelManagerDlg::sharedInstance()->IsDisplayed())
            CChannelManagerDlg::sharedInstance()->SetDocument((void*)this);

// -! NEW_FEATURE#0015
}

uint8_t CViewInstrument::EnvGetReleaseNode()
//---------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return ENV_RELEASE_NODE_UNSET;
    return envelope->release_node;
}

uint16_t CViewInstrument::EnvGetReleaseNodeValue()
//--------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->Values[EnvGetReleaseNode()];
}

uint16_t CViewInstrument::EnvGetReleaseNodeTick()
//-------------------------------------------
{
    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
    if(envelope == nullptr) return 0;
    return envelope->Ticks[EnvGetReleaseNode()];
}

bool CViewInstrument::EnvRemovePoint(UINT nPoint)
//---------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if ((pModDoc) && (nPoint <= EnvGetLastPoint()))
    {
            module_renderer *pSndFile = pModDoc->GetSoundFile();
            modplug::tracker::modinstrument_t *pIns = pSndFile->Instruments[m_nInstrument];
            if (pIns)
            {
                    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
                    if(envelope == nullptr || envelope->num_nodes == 0) return false;

                    envelope->num_nodes--;
                    for (UINT i = nPoint; i < envelope->num_nodes; i++)
                    {
                            envelope->Ticks[i] = envelope->Ticks[i + 1];
                            envelope->Values[i] = envelope->Values[i + 1];
                    }
                    if (nPoint >= envelope->num_nodes) nPoint = envelope->num_nodes - 1;
                    if (envelope->loop_start > nPoint) envelope->loop_start--;
                    if (envelope->loop_end > nPoint) envelope->loop_end--;
                    if (envelope->sustain_start > nPoint) envelope->sustain_start--;
                    if (envelope->sustain_end > nPoint) envelope->sustain_end--;
                    if (envelope->release_node>nPoint && envelope->release_node != ENV_RELEASE_NODE_UNSET) envelope->release_node--;
                    envelope->Ticks[0] = 0;

                    if(envelope->num_nodes == 1)
                    {
                            // if only one node is left, just disable the envelope completely
                            envelope->num_nodes = envelope->loop_start = envelope->loop_end = envelope->sustain_start = envelope->sustain_end = 0;
                            envelope->flags = 0;
                            envelope->release_node = ENV_RELEASE_NODE_UNSET;
                    }

                    SetInstrumentModified();
                    pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
                    return true;
            }
    }
    return false;
}

// Insert point. Returns 0 if error occured, else point ID + 1.
UINT CViewInstrument::EnvInsertPoint(int nTick, int nValue)
//---------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
    {
            module_renderer *pSndFile = pModDoc->GetSoundFile();
            modplug::tracker::modinstrument_t *pIns = pSndFile->Instruments[m_nInstrument];
            if (pIns)
            {
                    if(nTick < 0) return 0;

                    nValue = CLAMP(nValue, 0, 64);

                    modplug::tracker::modenvelope_t *envelope = GetEnvelopePtr();
                    if(envelope == nullptr) return 0;
                    uint8_t cDefaultValue;

                    switch(m_nEnv)
                    {
                    case ENV_VOLUME:
                            cDefaultValue = 64;
                            break;
                    case ENV_PANNING:
                            cDefaultValue = 32;
                            break;
                    case ENV_PITCH:
                            cDefaultValue = (pIns->pitch_envelope.flags & ENV_FILTER) ? 64 : 32;
                            break;
                    default:
                            return 0;
                    }

                    if (envelope->num_nodes < pSndFile->GetModSpecifications().envelopePointsMax)
                    {
                            if (!envelope->num_nodes)
                            {
                                    envelope->Ticks[0] = 0;
                                    envelope->Values[0] = cDefaultValue;
                                    envelope->num_nodes = 1;
                                    envelope->flags |= ENV_ENABLED;
                            }
                            UINT i = 0;
                            for (i = 0; i < envelope->num_nodes; i++) if (nTick <= envelope->Ticks[i]) break;
                            for (UINT j = envelope->num_nodes; j > i; j--)
                            {
                                    envelope->Ticks[j] = envelope->Ticks[j - 1];
                                    envelope->Values[j] = envelope->Values[j - 1];
                            }
                            envelope->Ticks[i] = (uint16_t)nTick;
                            envelope->Values[i] = (uint8_t)nValue;
                            envelope->num_nodes++;
                            if (envelope->loop_start >= i) envelope->loop_start++;
                            if (envelope->loop_end >= i) envelope->loop_end++;
                            if (envelope->sustain_start >= i) envelope->sustain_start++;
                            if (envelope->sustain_end >= i) envelope->sustain_end++;
                            if (envelope->release_node >= i && envelope->release_node != ENV_RELEASE_NODE_UNSET) envelope->release_node++;

                            SetInstrumentModified();
                            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
                            return i + 1;
                    }
            }
    }
    return 0;
}



void CViewInstrument::DrawPositionMarks(HDC hdc)
//----------------------------------------------
{
    CRect rect;
    for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++) if (m_dwNotifyPos[i] & MPTNOTIFY_POSVALID)
    {
            rect.top = -2;
            rect.left = TickToScreen(m_dwNotifyPos[i] & 0xFFFF);
            rect.right = rect.left + 1;
            rect.bottom = m_rcClient.bottom + 1;
            InvertRect(hdc, &rect);
    }
}


LRESULT CViewInstrument::OnPlayerNotify(MPTNOTIFICATION *pnotify)
//---------------------------------------------------------------
{
    uint32_t dwType;
    CModDoc *pModDoc = GetDocument();
    if ((!pnotify) || (!pModDoc)) return 0;
    switch(m_nEnv)
    {
    case ENV_PANNING:        dwType = MPTNOTIFY_PANENV; break;
    case ENV_PITCH:                dwType = MPTNOTIFY_PITCHENV; break;
    default:                        dwType = MPTNOTIFY_VOLENV; break;
    }
    if (pnotify->dwType & MPTNOTIFY_STOP)
    {
            for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++)
            {
                    if (m_dwNotifyPos[i])
                    {
                            memset(m_dwNotifyPos, 0, sizeof(m_dwNotifyPos));
                            InvalidateEnvelope();
                            break;
                    }
                    memset(m_baPlayingNote, 0, sizeof(bool)*NoteMax);         //rewbs.instViewNNA
                    m_nPlayingChannel = ChannelIndexInvalid;                        //rewbs.instViewNNA
            }
    } else
    if ((pnotify->dwType & dwType) && ((pnotify->dwType & 0xFFFF) == m_nInstrument))
    {
            BOOL bUpdate = FALSE;
            for (UINT i=0; i<MAX_VIRTUAL_CHANNELS; i++)
            {
                    //uint32_t newpos = (pSndFile->m_dwSongFlags & SONG_PAUSED) ? pnotify->dwPos[i] : 0;
                    uint32_t newpos = pnotify->dwPos[i];
                    if (m_dwNotifyPos[i] != newpos)
                    {
                            bUpdate = TRUE;
                            break;
                    }
            }
            if (bUpdate)
            {
                    HDC hdc = ::GetDC(m_hWnd);
                    DrawPositionMarks(hdc);
                    for (UINT j=0; j<MAX_VIRTUAL_CHANNELS; j++)
                    {
                            //uint32_t newpos = (pSndFile->m_dwSongFlags & SONG_PAUSED) ? pnotify->dwPos[j] : 0;
                            uint32_t newpos = pnotify->dwPos[j];
                            m_dwNotifyPos[j] = newpos;
                    }
                    DrawPositionMarks(hdc);
                    ::ReleaseDC(m_hWnd, hdc);
            }
    }
    return 0;
}


void CViewInstrument::DrawNcButton(CDC *pDC, UINT nBtn)
//-----------------------------------------------------
{
    CRect rect;
    COLORREF crHi = GetSysColor(COLOR_3DHILIGHT);
    COLORREF crDk = GetSysColor(COLOR_3DSHADOW);
    COLORREF crFc = GetSysColor(COLOR_3DFACE);
    COLORREF c1, c2;

    if (GetNcButtonRect(nBtn, &rect))
    {
            uint32_t dwStyle = m_NcButtonState[nBtn];
            COLORREF c3, c4;
            int xofs = 0, yofs = 0, nImage = 0;

            c1 = c2 = c3 = c4 = crFc;
            if (!(CMainFrame::m_dwPatternSetup & PATTERN_FLATBUTTONS))
            {
                    c1 = c3 = crHi;
                    c2 = crDk;
                    c4 = RGB(0,0,0);
            }
            if (dwStyle & (NCBTNS_PUSHED|NCBTNS_CHECKED))
            {
                    c1 = crDk;
                    c2 = crHi;
                    if (!(CMainFrame::m_dwPatternSetup & PATTERN_FLATBUTTONS))
                    {
                            c4 = crHi;
                            c3 = (dwStyle & NCBTNS_PUSHED) ? RGB(0,0,0) : crDk;
                    }
                    xofs = yofs = 1;
            } else
            if ((dwStyle & NCBTNS_MOUSEOVER) && (CMainFrame::m_dwPatternSetup & PATTERN_FLATBUTTONS))
            {
                    c1 = crHi;
                    c2 = crDk;
            }
            switch(cLeftBarButtons[nBtn])
            {
            case ID_ENVSEL_VOLUME:                nImage = IIMAGE_VOLENV; break;
            case ID_ENVSEL_PANNING:                nImage = IIMAGE_PANENV; break;
            case ID_ENVSEL_PITCH:                nImage = (dwStyle & NCBTNS_DISABLED) ? IIMAGE_NOPITCHENV : IIMAGE_PITCHENV; break;
            case ID_ENVELOPE_SETLOOP:        nImage = IIMAGE_LOOP; break;
            case ID_ENVELOPE_SUSTAIN:        nImage = IIMAGE_SUSTAIN; break;
            case ID_ENVELOPE_CARRY:                nImage = (dwStyle & NCBTNS_DISABLED) ? IIMAGE_NOCARRY : IIMAGE_CARRY; break;
            case ID_ENVELOPE_VOLUME:        nImage = IIMAGE_VOLSWITCH; break;
            case ID_ENVELOPE_PANNING:        nImage = IIMAGE_PANSWITCH; break;
            case ID_ENVELOPE_PITCH:                nImage = (dwStyle & NCBTNS_DISABLED) ? IIMAGE_NOPITCHSWITCH : IIMAGE_PITCHSWITCH; break;
            case ID_ENVELOPE_FILTER:        nImage = (dwStyle & NCBTNS_DISABLED) ? IIMAGE_NOFILTERSWITCH : IIMAGE_FILTERSWITCH; break;
            case ID_INSTRUMENT_SAMPLEMAP: nImage = IIMAGE_SAMPLEMAP; break;
            case ID_ENVELOPE_VIEWGRID:        nImage = IIMAGE_GRID; break;
            case ID_ENVELOPE_ZOOM_IN:        nImage = (dwStyle & NCBTNS_DISABLED) ? IIMAGE_NOZOOMIN : IIMAGE_ZOOMIN; break;
            case ID_ENVELOPE_ZOOM_OUT:        nImage = (dwStyle & NCBTNS_DISABLED) ? IIMAGE_NOZOOMOUT : IIMAGE_ZOOMOUT; break;
            }
            pDC->Draw3dRect(rect.left-1, rect.top-1, ENV_LEFTBAR_CXBTN+2, ENV_LEFTBAR_CYBTN+2, c3, c4);
            pDC->Draw3dRect(rect.left, rect.top, ENV_LEFTBAR_CXBTN, ENV_LEFTBAR_CYBTN, c1, c2);
            rect.DeflateRect(1, 1);
            pDC->FillSolidRect(&rect, crFc);
            rect.left += xofs;
            rect.top += yofs;
            if (dwStyle & NCBTNS_CHECKED) m_bmpEnvBar.Draw(pDC, IIMAGE_CHECKED, rect.TopLeft(), ILD_NORMAL);
            m_bmpEnvBar.Draw(pDC, nImage, rect.TopLeft(), ILD_NORMAL);
    } else
    {
            c1 = c2 = crFc;
            if (CMainFrame::m_dwPatternSetup & PATTERN_FLATBUTTONS)
            {
                    c1 = crDk;
                    c2 = crHi;
            }
            pDC->Draw3dRect(rect.left, rect.top, 2, ENV_LEFTBAR_CYBTN, c1, c2);
    }
}


void CViewInstrument::OnNcPaint()
//-------------------------------
{
    RECT rect;

    CModScrollView::OnNcPaint();
    GetWindowRect(&rect);
    // Assumes there is no other non-client items
    rect.bottom = ENV_LEFTBAR_CY;
    rect.right -= rect.left;
    rect.left = 0;
    rect.top = 0;
    if ((rect.left < rect.right) && (rect.top < rect.bottom))
    {
            CDC *pDC = GetWindowDC();
            HDC hdc = pDC->m_hDC;
            HGDIOBJ oldpen = SelectObject(hdc, (HGDIOBJ)CMainFrame::penDarkGray);
            rect.bottom--;
            MoveToEx(hdc, rect.left, rect.bottom, NULL);
            LineTo(hdc, rect.right, rect.bottom);
            if (rect.top < rect.bottom) FillRect(hdc, &rect, CMainFrame::brushGray);
            if (rect.top + 2 < rect.bottom)
            {
                    for (UINT i=0; i<ENV_LEFTBAR_BUTTONS; i++)
                    {
                            DrawNcButton(pDC, i);
                    }
            }
            if (oldpen) SelectObject(hdc, oldpen);
            ReleaseDC(pDC);
    }
}


////////////////////////////////////////////////////////////////////
// CViewInstrument messages


void CViewInstrument::OnSize(UINT nType, int cx, int cy)
//------------------------------------------------------
{
    CScrollView::OnSize(nType, cx, cy);
    if (((nType == SIZE_RESTORED) || (nType == SIZE_MAXIMIZED)) && (cx > 0) && (cy > 0))
    {
            UpdateScrollSize();
    }
}


void CViewInstrument::OnNcCalcSize(BOOL bCalcValidRects, NCCALCSIZE_PARAMS* lpncsp)
//---------------------------------------------------------------------------------
{
    CModScrollView::OnNcCalcSize(bCalcValidRects, lpncsp);
    if (lpncsp)
    {
            lpncsp->rgrc[0].top += ENV_LEFTBAR_CY;
            if (lpncsp->rgrc[0].bottom < lpncsp->rgrc[0].top) lpncsp->rgrc[0].top = lpncsp->rgrc[0].bottom;
    }
}


void CViewInstrument::OnNcMouseMove(UINT nHitTest, CPoint point)
//--------------------------------------------------------------
{
    CRect rect, rcWnd;
    UINT nBtnSel = 0xFFFF;

    GetWindowRect(&rcWnd);
    for (UINT i=0; i<ENV_LEFTBAR_BUTTONS; i++)
    {
            if ((!(m_NcButtonState[i] & NCBTNS_DISABLED)) && (GetNcButtonRect(i, &rect)))
            {
                    rect.OffsetRect(rcWnd.left, rcWnd.top);
                    if (rect.PtInRect(point))
                    {
                            nBtnSel = i;
                            break;
                    }
            }
    }
    if (nBtnSel != m_nBtnMouseOver)
    {
            CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
            if (pMainFrm)
            {
                    CString strText = "";
                    if ((nBtnSel < ENV_LEFTBAR_BUTTONS) && (cLeftBarButtons[nBtnSel] != ID_SEPARATOR))
                    {
                            strText.LoadString(cLeftBarButtons[nBtnSel]);
                    }
                    pMainFrm->SetHelpText(strText);
            }
            m_nBtnMouseOver = nBtnSel;
            UpdateNcButtonState();
    }
    CModScrollView::OnNcMouseMove(nHitTest, point);
}


void CViewInstrument::OnNcLButtonDown(UINT uFlags, CPoint point)
//--------------------------------------------------------------
{
    if (m_nBtnMouseOver < ENV_LEFTBAR_BUTTONS)
    {
            m_dwStatus |= INSSTATUS_NCLBTNDOWN;
            if (cLeftBarButtons[m_nBtnMouseOver] != ID_SEPARATOR)
            {
                    PostMessage(WM_COMMAND, cLeftBarButtons[m_nBtnMouseOver]);
                    UpdateNcButtonState();
            }
    }
    CModScrollView::OnNcLButtonDown(uFlags, point);
}


void CViewInstrument::OnNcLButtonUp(UINT uFlags, CPoint point)
//------------------------------------------------------------
{
    if (m_dwStatus & INSSTATUS_NCLBTNDOWN)
    {
            m_dwStatus &= ~INSSTATUS_NCLBTNDOWN;
            UpdateNcButtonState();
    }
    CModScrollView::OnNcLButtonUp(uFlags, point);
}


void CViewInstrument::OnNcLButtonDblClk(UINT uFlags, CPoint point)
//----------------------------------------------------------------
{
    OnNcLButtonDown(uFlags, point);
}


#if _MFC_VER > 0x0710
LRESULT CViewInstrument::OnNcHitTest(CPoint point)
#else
UINT CViewInstrument::OnNcHitTest(CPoint point)
#endif
//---------------------------------------------
{
    CRect rect;
    GetWindowRect(&rect);
    rect.bottom = rect.top + ENV_LEFTBAR_CY;
    if (rect.PtInRect(point))
    {
            return HTBORDER;
    }
    return CModScrollView::OnNcHitTest(point);
}


void CViewInstrument::OnMouseMove(UINT, CPoint pt)
//------------------------------------------------
{
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if (pIns == nullptr || pEnv == nullptr) return;

    bool bSplitCursor = false;
    CHAR s[256];

    if ((m_nBtnMouseOver < ENV_LEFTBAR_BUTTONS) || (m_dwStatus & INSSTATUS_NCLBTNDOWN))
    {
            m_dwStatus &= ~INSSTATUS_NCLBTNDOWN;
            m_nBtnMouseOver = 0xFFFF;
            UpdateNcButtonState();
            CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
            if (pMainFrm) pMainFrm->SetHelpText("");
    }
    int nTick = ScreenToTick(pt.x);
    int nVal = ScreenToValue(pt.y);
    nVal = CLAMP(nVal, ENVELOPE_MIN, ENVELOPE_MAX);
    if (nTick < 0) nTick = 0;
    if (nTick <= EnvGetReleaseNodeTick() + 1 || EnvGetReleaseNode() == ENV_RELEASE_NODE_UNSET)
    {
            // ticks before release node (or no release node)
            const int displayVal = (m_nEnv != ENV_VOLUME && !(m_nEnv == ENV_PITCH && (pIns->pitch_envelope.flags & ENV_FILTER))) ? nVal - 32 : nVal;
            if(m_nEnv != ENV_PANNING)
                    wsprintf(s, "Tick %d, [%d]", nTick, displayVal);
            else        // panning envelope: display right/center/left chars
                    wsprintf(s, "Tick %d, [%d %c]", nTick, abs(displayVal), displayVal > 0 ? 'R' : (displayVal < 0 ? 'L' : 'C'));
    } else
    {
            // ticks after release node
            int displayVal = (nVal - EnvGetReleaseNodeValue()) * 2;
            displayVal = (m_nEnv != ENV_VOLUME) ? displayVal - 32 : displayVal;
            wsprintf(s, "Tick %d, [Rel%c%d]",  nTick, displayVal > 0 ? '+' : '-', abs(displayVal));
    }
    UpdateIndicator(s);

    if ((m_dwStatus & INSSTATUS_DRAGGING) && (m_nDragItem))
    {
            bool bChanged = false;
            if (pt.x >= m_rcClient.right - 2) nTick++;
            if (IsDragItemEnvPoint())
            {
                    int nRelTick = pEnv->Ticks[m_nDragItem - 1];
                    bChanged = EnvSetValue(m_nDragItem - 1, nTick, nVal);
            } else
            {
                    int nPoint = ScreenToPoint(pt.x, pt.y);
                    if (nPoint >= 0) switch(m_nDragItem)
                    {
                    case ENV_DRAGLOOPSTART:
                            bChanged = EnvSetLoopStart(nPoint);
                            bSplitCursor = true;
                            break;
                    case ENV_DRAGLOOPEND:
                            bChanged = EnvSetLoopEnd(nPoint);
                            bSplitCursor = true;
                            break;
                    case ENV_DRAGSUSTAINSTART:
                            bChanged = EnvSetSustainStart(nPoint);
                            bSplitCursor = true;
                            break;
                    case ENV_DRAGSUSTAINEND:
                            bChanged = EnvSetSustainEnd(nPoint);
                            bSplitCursor = true;
                            break;
                    }
            }
            if (bChanged)
            {
                    if (pt.x <= 0)
                    {
                            UpdateScrollSize();
                            OnScrollBy(CSize(pt.x - (int)m_fZoom, 0), TRUE);
                    }
                    if (pt.x >= m_rcClient.right-1)
                    {
                            UpdateScrollSize();
                            OnScrollBy(CSize((int)m_fZoom + pt.x - m_rcClient.right, 0), TRUE);
                    }
                    CModDoc *pModDoc = GetDocument();
                    if(pModDoc)
                    {
                            SetInstrumentModified();
                            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
                    }
                    UpdateWindow(); //rewbs: TODO - optimisation here so we don't redraw whole view.
            }
    } else
    {
            CRect rect;
            if (EnvGetSustain())
            {
                    int nspace = m_rcClient.bottom/4;
                    rect.top = ValueToScreen(EnvGetValue(EnvGetSustainStart())) - nspace;
                    rect.bottom = rect.top + nspace * 2;
                    rect.right = PointToScreen(EnvGetSustainStart()) + 1;
                    rect.left = rect.right - (int)(m_fZoom * 2);
                    if (rect.PtInRect(pt))
                    {
                            bSplitCursor = true; // ENV_DRAGSUSTAINSTART;
                    } else
                    {
                            rect.top = ValueToScreen(EnvGetValue(EnvGetSustainEnd())) - nspace;
                            rect.bottom = rect.top + nspace * 2;
                            rect.left = PointToScreen(EnvGetSustainEnd()) - 1;
                            rect.right = rect.left + (int)(m_fZoom *2);
                            if (rect.PtInRect(pt)) bSplitCursor = true; // ENV_DRAGSUSTAINEND;
                    }
            }
            if (EnvGetLoop())
            {
                    rect.top = m_rcClient.top;
                    rect.bottom = m_rcClient.bottom;
                    rect.right = PointToScreen(EnvGetLoopStart()) + 1;
                    rect.left = rect.right - (int)(m_fZoom * 2);
                    if (rect.PtInRect(pt))
                    {
                            bSplitCursor = true; // ENV_DRAGLOOPSTART;
                    } else
                    {
                            rect.left = PointToScreen(EnvGetLoopEnd()) - 1;
                            rect.right = rect.left + (int)(m_fZoom * 2);
                            if (rect.PtInRect(pt)) bSplitCursor = true; // ENV_DRAGLOOPEND;
                    }
            }
    }
    // Update the mouse cursor
    if (bSplitCursor)
    {
            if (!(m_dwStatus & INSSTATUS_SPLITCURSOR))
            {
                    m_dwStatus |= INSSTATUS_SPLITCURSOR;
                    if (!(m_dwStatus & INSSTATUS_DRAGGING)) SetCapture();
                    SetCursor(CMainFrame::curVSplit);
            }
    } else
    {
            if (m_dwStatus & INSSTATUS_SPLITCURSOR)
            {
                    m_dwStatus &= ~INSSTATUS_SPLITCURSOR;
                    SetCursor(CMainFrame::curArrow);
                    if (!(m_dwStatus & INSSTATUS_DRAGGING))        ReleaseCapture();
            }
    }
}


void CViewInstrument::OnLButtonDown(UINT, CPoint pt)
//--------------------------------------------------
{
    if (!(m_dwStatus & INSSTATUS_DRAGGING))
    {
            CRect rect;
            // Look if dragging a point
            UINT maxpoint = EnvGetLastPoint();
            m_nDragItem = 0;
            for (UINT i = 0; i <= maxpoint; i++)
            {
                    int x = PointToScreen(i);
                    int y = ValueToScreen(EnvGetValue(i));
                    rect.SetRect(x-6, y-6, x+7, y+7);
                    if (rect.PtInRect(pt))
                    {
                            m_nDragItem = i + 1;
                            break;
                    }
            }
            if ((!m_nDragItem) && (EnvGetSustain()))
            {
                    int nspace = m_rcClient.bottom/4;
                    rect.top = ValueToScreen(EnvGetValue(EnvGetSustainStart())) - nspace;
                    rect.bottom = rect.top + nspace * 2;
                    rect.right = PointToScreen(EnvGetSustainStart()) + 1;
                    rect.left = rect.right - (int)(m_fZoom * 2);
                    if (rect.PtInRect(pt))
                    {
                            m_nDragItem = ENV_DRAGSUSTAINSTART;
                    } else
                    {
                            rect.top = ValueToScreen(EnvGetValue(EnvGetSustainEnd())) - nspace;
                            rect.bottom = rect.top + nspace * 2;
                            rect.left = PointToScreen(EnvGetSustainEnd()) - 1;
                            rect.right = rect.left + (int)(m_fZoom * 2);
                            if (rect.PtInRect(pt)) m_nDragItem = ENV_DRAGSUSTAINEND;
                    }
            }
            if ((!m_nDragItem) && (EnvGetLoop()))
            {
                    rect.top = m_rcClient.top;
                    rect.bottom = m_rcClient.bottom;
                    rect.right = PointToScreen(EnvGetLoopStart()) + 1;
                    rect.left = rect.right - (int)(m_fZoom * 2);
                    if (rect.PtInRect(pt))
                    {
                            m_nDragItem = ENV_DRAGLOOPSTART;
                    } else
                    {
                            rect.left = PointToScreen(EnvGetLoopEnd()) - 1;
                            rect.right = rect.left + (int)(m_fZoom * 2);
                            if (rect.PtInRect(pt)) m_nDragItem = ENV_DRAGLOOPEND;
                    }
            }
            if (m_nDragItem)
            {
                    SetCapture();
                    m_dwStatus |= INSSTATUS_DRAGGING;
                    // refresh active node colour
                    InvalidateRect(NULL, FALSE);
            }
    }
}


void CViewInstrument::OnLButtonUp(UINT, CPoint)
//---------------------------------------------
{
    if (m_dwStatus & INSSTATUS_SPLITCURSOR)
    {
            m_dwStatus &= ~INSSTATUS_SPLITCURSOR;
            SetCursor(CMainFrame::curArrow);
    }
    if (m_dwStatus & INSSTATUS_DRAGGING)
    {
            m_dwStatus &= ~INSSTATUS_DRAGGING;
            ReleaseCapture();
    }
}


void CViewInstrument::OnRButtonDown(UINT, CPoint pt)
//--------------------------------------------------
{
    const CModDoc *pModDoc = GetDocument();
    if(!pModDoc) return;
    const module_renderer *pSndFile = GetDocument()->GetSoundFile();
    if(!pSndFile) return;

    CMenu Menu;
    if (m_dwStatus & INSSTATUS_DRAGGING) return;
    if ((pModDoc) && (Menu.LoadMenu(IDR_ENVELOPES)))
    {
            CMenu* pSubMenu = Menu.GetSubMenu(0);
            if (pSubMenu != NULL)
            {
                    m_nDragItem = ScreenToPoint(pt.x, pt.y) + 1;
                    const UINT maxpoint = (pSndFile->m_nType == MOD_TYPE_XM) ? 11 : 24;
                    const UINT lastpoint = EnvGetLastPoint();
                    const bool forceRelease = !pSndFile->GetModSpecifications().hasReleaseNode && (EnvGetReleaseNode() != ENV_RELEASE_NODE_UNSET);
                    pSubMenu->EnableMenuItem(ID_ENVELOPE_INSERTPOINT, (lastpoint < maxpoint) ? MF_ENABLED : MF_GRAYED);
                    pSubMenu->EnableMenuItem(ID_ENVELOPE_REMOVEPOINT, ((m_nDragItem) && (lastpoint > 0)) ? MF_ENABLED : MF_GRAYED);
                    pSubMenu->EnableMenuItem(ID_ENVELOPE_CARRY, (pSndFile->GetType() & (MOD_TYPE_IT|MOD_TYPE_MPT)) ? MF_ENABLED : MF_GRAYED);
                    pSubMenu->EnableMenuItem(ID_ENVELOPE_TOGGLERELEASENODE, ((pSndFile->GetModSpecifications().hasReleaseNode && m_nEnv == ENV_VOLUME) || forceRelease) ? MF_ENABLED : MF_GRAYED);
                    pSubMenu->CheckMenuItem(ID_ENVELOPE_SETLOOP, (EnvGetLoop()) ? MF_CHECKED : MF_UNCHECKED);
                    pSubMenu->CheckMenuItem(ID_ENVELOPE_SUSTAIN, (EnvGetSustain()) ? MF_CHECKED : MF_UNCHECKED);
                    pSubMenu->CheckMenuItem(ID_ENVELOPE_CARRY, (EnvGetCarry()) ? MF_CHECKED : MF_UNCHECKED);
                    pSubMenu->CheckMenuItem(ID_ENVELOPE_TOGGLERELEASENODE, (EnvGetReleaseNode() == m_nDragItem - 1) ? MF_CHECKED : MF_UNCHECKED);
                    m_ptMenu = pt;
                    ClientToScreen(&pt);
                    pSubMenu->TrackPopupMenu(TPM_LEFTALIGN | TPM_RIGHTBUTTON,pt.x,pt.y,this);
            }
    }
}

void CViewInstrument::OnMButtonDown(UINT, CPoint pt)
//--------------------------------------------------
{
    // Middle mouse button: Remove envelope point
    if(EnvGetLastPoint() == 0) return;
    m_nDragItem = ScreenToPoint(pt.x, pt.y) + 1;
    EnvRemovePoint(m_nDragItem - 1);
}


void CViewInstrument::OnPrevInstrument()
//--------------------------------------
{
    SendCtrlMessage(CTRLMSG_INS_PREVINSTRUMENT);
}


void CViewInstrument::OnNextInstrument()
//--------------------------------------
{
    SendCtrlMessage(CTRLMSG_INS_NEXTINSTRUMENT);
}


void CViewInstrument::OnEditSampleMap()
//-------------------------------------
{
    SendCtrlMessage(CTRLMSG_INS_SAMPLEMAP);
}


void CViewInstrument::OnSelectVolumeEnv()
//---------------------------------------
{
    if (m_nEnv != ENV_VOLUME) SetCurrentInstrument(m_nInstrument, ENV_VOLUME);
}


void CViewInstrument::OnSelectPanningEnv()
//----------------------------------------
{
    if (m_nEnv != ENV_PANNING) SetCurrentInstrument(m_nInstrument, ENV_PANNING);
}


void CViewInstrument::OnSelectPitchEnv()
//--------------------------------------
{
    if (m_nEnv != ENV_PITCH) SetCurrentInstrument(m_nInstrument, ENV_PITCH);
}


void CViewInstrument::OnEnvLoopChanged()
//--------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if ((pModDoc) && (EnvSetLoop(!EnvGetLoop())))
    {
            SetInstrumentModified();
            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
            UpdateNcButtonState();
    }
}


void CViewInstrument::OnEnvSustainChanged()
//-----------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if ((pModDoc) && (EnvSetSustain(!EnvGetSustain())))
    {
            SetInstrumentModified();
            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
            UpdateNcButtonState();
    }
}


void CViewInstrument::OnEnvCarryChanged()
//---------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if ((pModDoc) && (EnvSetCarry(!EnvGetCarry())))
    {
            SetInstrumentModified();
            UpdateNcButtonState();
    }
}

void CViewInstrument::OnEnvToggleReleasNode()
//-------------------------------------------
{
    if(IsDragItemEnvPoint() && EnvToggleReleaseNode(m_nDragItem - 1))
    {
            SetInstrumentModified();
            InvalidateRect(NULL, FALSE);
    }
}


void CViewInstrument::OnEnvVolChanged()
//-------------------------------------
{
    if (EnvSetVolEnv(!EnvGetVolEnv()))
    {
            SetInstrumentModified();
            UpdateNcButtonState();
    }
}


void CViewInstrument::OnEnvPanChanged()
//-------------------------------------
{
    if (EnvSetPanEnv(!EnvGetPanEnv()))
    {
            SetInstrumentModified();
            UpdateNcButtonState();
    }
}


void CViewInstrument::OnEnvPitchChanged()
//---------------------------------------
{
    if (EnvSetPitchEnv(!EnvGetPitchEnv()))
    {
            SetInstrumentModified();
            UpdateNcButtonState();
    }
}


void CViewInstrument::OnEnvFilterChanged()
//----------------------------------------
{
    if (EnvSetFilterEnv(!EnvGetFilterEnv()))
    {
            SetInstrumentModified();
            UpdateNcButtonState();
    }
}

//rewbs.envRowGrid
void CViewInstrument::OnEnvToggleGrid()
//-------------------------------------
{
    m_bGrid = !m_bGrid;
    if (m_bGrid)
            m_bGridForceRedraw;
    CModDoc *pModDoc = GetDocument();
    if (pModDoc)
            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);

}
//end rewbs.envRowGrid

void CViewInstrument::OnEnvRemovePoint()
//--------------------------------------
{
    if(m_nDragItem > 0)
    {
            EnvRemovePoint(m_nDragItem - 1);
    }
}

void CViewInstrument::OnEnvInsertPoint()
//--------------------------------------
{

    EnvInsertPoint(ScreenToTick(m_ptMenu.x), ScreenToValue(m_ptMenu.y));
}


void CViewInstrument::OnEditCopy()
//--------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc) pModDoc->CopyEnvelope(m_nInstrument, m_nEnv);
}


void CViewInstrument::OnEditPaste()
//---------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if (pModDoc) pModDoc->PasteEnvelope(m_nInstrument, m_nEnv);
}

static uint32_t nLastNotePlayed = 0;
static uint32_t nLastScanCode = 0;


void CViewInstrument::PlayNote(UINT note)
//---------------------------------------
{
    CMainFrame *pMainFrm = CMainFrame::GetMainFrame();
    CModDoc *pModDoc = GetDocument();
    if ((pModDoc) && (pMainFrm) && (note > 0) && (note<128))
    {
            CHAR s[64];
            const size_t sizeofS = sizeof(s) / sizeof(s[0]);
            if (note >= 0xFE)
            {
                    pModDoc->NoteOff(0, (note == NoteNoteCut) ? TRUE : FALSE, m_nInstrument);
                    pMainFrm->SetInfoText("");
            } else
            if (m_nInstrument && !m_baPlayingNote[note])
            {
                    //rewbs.instViewNNA
            /*        CSoundFile *pSoundFile = pModDoc->GetSoundFile();
                    if (pSoundFile && m_baPlayingNote>0 && m_nPlayingChannel>=0)
                    {
                            MODCHANNEL *pChn = &(pSoundFile->Chn[m_nPlayingChannel]); //Get pointer to channel playing last note.
                            if (pChn->pHeader)        //is it valid?
                            {
                                    uint32_t tempflags = pChn->flags;
                                    pChn->flags = 0;
                                    pModDoc->GetSoundFile()->CheckNNA(m_nPlayingChannel, m_nInstrument, note, FALSE); //if so, apply NNA
                                    pChn->flags = tempflags;
                            }
                    }
            */
                    modplug::tracker::modinstrument_t *pIns = pModDoc->GetSoundFile()->Instruments[m_nInstrument];
                    if ((!pIns) || (!pIns->Keyboard[note-1] && !pIns->nMixPlug)) return;
                    m_baPlayingNote[note] = true;                                                                                        //rewbs.instViewNNA
                    m_nPlayingChannel = pModDoc->PlayNote(note, m_nInstrument, 0, FALSE); //rewbs.instViewNNA
                    s[0] = 0;
                    if ((note) && (note <= NoteMax))
                    {
                            const std::string temp = pModDoc->GetSoundFile()->GetNoteName(static_cast<int16_t>(note), m_nInstrument);
                            if(temp.size() >= sizeofS)
                                    wsprintf(s, "%s", "...");
                            else
                                    wsprintf(s, "%s", temp.c_str());
                    }
                    pMainFrm->SetInfoText(s);
            }
    }
}

void CViewInstrument::OnChar(UINT nChar, UINT nRepCnt, UINT nFlags)
//-----------------------------------------------------------------
{
    CModScrollView::OnChar(nChar, nRepCnt, nFlags);
}


void CViewInstrument::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
//------------------------------------------------------------------
{
    CModScrollView::OnKeyUp(nChar, nRepCnt, nFlags);
}


void CViewInstrument::OnDropFiles(HDROP hDropInfo)
//------------------------------------------------
{
    UINT nFiles = ::DragQueryFile(hDropInfo, (UINT)-1, NULL, 0);
    if (nFiles)
    {
            TCHAR szFileName[_MAX_PATH] = "";
            CMainFrame::GetMainFrame()->SetForegroundWindow();
            ::DragQueryFile(hDropInfo, 0, szFileName, _MAX_PATH);
            if (szFileName[0]) SendCtrlMessage(CTRLMSG_INS_OPENFILE, (LPARAM)szFileName);
    }
    ::DragFinish(hDropInfo);
}


BOOL CViewInstrument::OnDragonDrop(BOOL bDoDrop, LPDRAGONDROP lpDropInfo)
//-----------------------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile;
    BOOL bCanDrop = FALSE;
    BOOL bUpdate;

    if ((!lpDropInfo) || (!pModDoc)) return FALSE;
    pSndFile = pModDoc->GetSoundFile();
    switch(lpDropInfo->dwDropType)
    {
    case DRAGONDROP_INSTRUMENT:
            if (lpDropInfo->pModDoc == pModDoc)
            {
                    bCanDrop = ((lpDropInfo->dwDropItem)
                                     && (lpDropInfo->dwDropItem <= pSndFile->m_nInstruments)
                                     && (lpDropInfo->pModDoc == pModDoc));
            } else
            {
                    bCanDrop = ((lpDropInfo->dwDropItem)
                                     && ((lpDropInfo->lDropParam) || (lpDropInfo->pModDoc)));
            }
            break;

    case DRAGONDROP_DLS:
            bCanDrop = ((lpDropInfo->dwDropItem < MAX_DLS_BANKS)
                             && (CTrackApp::gpDLSBanks[lpDropInfo->dwDropItem]));
            break;

    case DRAGONDROP_SOUNDFILE:
    case DRAGONDROP_MIDIINSTR:
            bCanDrop = ((lpDropInfo->lDropParam)
                             && (*((LPCSTR)lpDropInfo->lDropParam)));
            break;
    }
    if ((!bCanDrop) || (!bDoDrop)) return bCanDrop;
    if ((!pSndFile->m_nInstruments) && (pSndFile->m_nType & (MOD_TYPE_XM|MOD_TYPE_IT|MOD_TYPE_MPT)))
    {
            SendCtrlMessage(CTRLMSG_INS_NEWINSTRUMENT);
    }
    if ((!m_nInstrument) || (m_nInstrument > pSndFile->m_nInstruments)) return FALSE;
    // Do the drop
    bUpdate = FALSE;
    BeginWaitCursor();
    switch(lpDropInfo->dwDropType)
    {
    case DRAGONDROP_INSTRUMENT:
            if (lpDropInfo->pModDoc == pModDoc)
            {
                    SendCtrlMessage(CTRLMSG_SETCURRENTINSTRUMENT, lpDropInfo->dwDropItem);
            } else
            {
                    SendCtrlMessage(CTRLMSG_INS_SONGDROP, (LPARAM)lpDropInfo);
            }
            break;

    case DRAGONDROP_MIDIINSTR:
            if (CDLSBank::IsDLSBank((LPCSTR)lpDropInfo->lDropParam))
            {
                    CDLSBank dlsbank;
                    if (dlsbank.Open((LPCSTR)lpDropInfo->lDropParam))
                    {
                            DLSINSTRUMENT *pDlsIns;
                            UINT nIns = 0, nRgn = 0xFF;
                            // Drums
                            if (lpDropInfo->dwDropItem & 0x80)
                            {
                                    UINT key = lpDropInfo->dwDropItem & 0x7F;
                                    pDlsIns = dlsbank.FindInstrument(TRUE, 0xFFFF, 0xFF, key, &nIns);
                                    if (pDlsIns) nRgn = dlsbank.GetRegionFromKey(nIns, key);
                            } else
                            // Melodic
                            {
                                    pDlsIns = dlsbank.FindInstrument(FALSE, 0xFFFF, lpDropInfo->dwDropItem, 60, &nIns);
                                    if (pDlsIns) nRgn = dlsbank.GetRegionFromKey(nIns, 60);
                            }
                            bCanDrop = FALSE;
                            if (pDlsIns)
                            {
                                    BEGIN_CRITICAL();
                                    bCanDrop = dlsbank.ExtractInstrument(pSndFile, m_nInstrument, nIns, nRgn);
                                    END_CRITICAL();
                            }
                            bUpdate = TRUE;
                            break;
                    }
            }
            // Instrument file -> fall through
    case DRAGONDROP_SOUNDFILE:
            SendCtrlMessage(CTRLMSG_INS_OPENFILE, lpDropInfo->lDropParam);
            break;

    case DRAGONDROP_DLS:
            {
                    CDLSBank *pDLSBank = CTrackApp::gpDLSBanks[lpDropInfo->dwDropItem];
                    UINT nIns = lpDropInfo->lDropParam & 0x7FFF;
                    UINT nRgn;
                    // Drums:        (0x80000000) | (Region << 16) | (Instrument)
                    if (lpDropInfo->lDropParam & 0x80000000)
                    {
                            nRgn = (lpDropInfo->lDropParam & 0xFF0000) >> 16;
                    } else
                    // Melodic: (Instrument)
                    {
                            nRgn = pDLSBank->GetRegionFromKey(nIns, 60);
                    }
                    BEGIN_CRITICAL();
                    bCanDrop = pDLSBank->ExtractInstrument(pSndFile, m_nInstrument, nIns, nRgn);
                    END_CRITICAL();
                    bUpdate = TRUE;
            }
            break;
    }
    if (bUpdate)
    {
            SetInstrumentModified();
            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_INSTRUMENT | HINT_ENVELOPE | HINT_INSNAMES, NULL);
    }
    CMDIChildWnd *pMDIFrame = (CMDIChildWnd *)GetParentFrame();
    if (pMDIFrame)
    {
            pMDIFrame->MDIActivate();
            pMDIFrame->SetActiveView(this);
            SetFocus();
    }
    EndWaitCursor();
    return bCanDrop;
}


LRESULT CViewInstrument::OnMidiMsg(WPARAM dwMidiDataParam, LPARAM)
//-----------------------------------------------------------
{
    const uint32_t dwMidiData = dwMidiDataParam;
    static uint8_t midivolume = 127;

    CModDoc *pModDoc = GetDocument();
    uint8_t midiByte1 = GetFromMIDIMsg_DataByte1(dwMidiData);
    uint8_t midiByte2 = GetFromMIDIMsg_DataByte2(dwMidiData);

    module_renderer* pSndFile = (pModDoc) ? pModDoc->GetSoundFile() : NULL;
    if (!pSndFile) return 0;

    const uint8_t nNote  = midiByte1 + 1;                // +1 is for MPT, where middle C is 61
    int nVol   = midiByte2;
    uint8_t event  = GetFromMIDIMsg_Event(dwMidiData);
    if ((event == 0x9) && !nVol) event = 0x8;        //Convert event to note-off if req'd

    uint8_t mappedIndex = 0, paramValue = 0;
    uint32_t paramIndex = 0;
    if(pSndFile->GetMIDIMapper().OnMIDImsg(dwMidiData, mappedIndex, paramIndex, paramValue))
            return 0;

    switch(event)
    {
            case 0x8: // Note Off
                    midiByte2 = 0;

            case 0x9: // Note On
                    pModDoc->NoteOff(nNote, FALSE, m_nInstrument);
                    if (midiByte2 & 0x7F)
                    {
                            nVol = ApplyVolumeRelatedMidiSettings(dwMidiData, midivolume);
                            pModDoc->PlayNote(nNote, m_nInstrument, 0, FALSE, nVol);
                    }
            break;

            case 0xB: //Controller change
                    switch(midiByte1)
                    {
                            case 0x7: //Volume
                                    midivolume = midiByte2;
                            break;
                    }
            default:
                    if(CMainFrame::m_dwMidiSetup & MIDISETUP_MIDITOPLUG && CMainFrame::GetMainFrame()->GetModPlaying() == pModDoc)
                    {
                            const modplug::tracker::instrumentindex_t instr = m_nInstrument;
                            IMixPlugin* plug = pSndFile->GetInstrumentPlugin(instr);
                            if(plug)
                            {
                                    plug->MidiSend(dwMidiData);
                                    // Sending midi may modify the plug. For now, if MIDI data
                                    // is not active sensing, set modified.
                                    if(dwMidiData != MIDISTATUS_ACTIVESENSING)
                                            CMainFrame::GetMainFrame()->ThreadSafeSetModified(pModDoc);
                            }
                    }
            break;
    }

    return 0;
}


void CViewInstrument::OnEnvelopeScalepoints()
//--------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    module_renderer *pSndFile = (pModDoc) ? pModDoc->GetSoundFile() : NULL;
    if(pSndFile == nullptr)
            return;

    if(m_nInstrument >= 1 &&
       m_nInstrument <= pSndFile->GetNumInstruments() &&
       pSndFile->Instruments[m_nInstrument])
    {
            // "Center" y value of the envelope. For panning and pitch, this is 32, for volume and filter it is 0 (minimum).
            int nOffset = ((m_nEnv != ENV_VOLUME) && ((GetEnvelopePtr()->flags & ENV_FILTER) == 0)) ? 32 : 0;

            CScaleEnvPointsDlg dlg(this, GetEnvelopePtr(), nOffset);
            if(dlg.DoModal())
            {
                    SetInstrumentModified();
                    pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
            }
    }


}


void CViewInstrument::EnvSetZoom(float fNewZoom)
//----------------------------------------------
{
    m_fZoom = fNewZoom;
    Limit(m_fZoom, ENV_MIN_ZOOM, ENV_MAX_ZOOM);
    InvalidateRect(NULL, FALSE);
    UpdateScrollSize();
    UpdateNcButtonState();
}


////////////////////////////////////////
//  Envelope Editor - Keyboard actions

void CViewInstrument::EnvKbdSelectPrevPoint()
//-------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr) return;
    if(m_nDragItem <= 1 || m_nDragItem > pEnv->num_nodes)
            m_nDragItem = pEnv->num_nodes;
    else
            m_nDragItem--;
    InvalidateRect(NULL, FALSE);
}


void CViewInstrument::EnvKbdSelectNextPoint()
//-------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr) return;
    if(m_nDragItem >= pEnv->num_nodes)
            m_nDragItem = 1;
    else
            m_nDragItem++;
    InvalidateRect(NULL, FALSE);
}


void CViewInstrument::EnvKbdMovePointLeft()
//-----------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(m_nDragItem == 1 || !CanMovePoint(m_nDragItem - 1, -1))
            return;
    pEnv->Ticks[m_nDragItem - 1]--;

    SetInstrumentModified();
    InvalidateRect(NULL, FALSE);
}


void CViewInstrument::EnvKbdMovePointRight()
//------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(m_nDragItem == 1 || !CanMovePoint(m_nDragItem - 1, 1))
            return;
    pEnv->Ticks[m_nDragItem - 1]++;

    SetInstrumentModified();
    InvalidateRect(NULL, FALSE);
}


void CViewInstrument::EnvKbdMovePointUp(uint8_t stepsize)
//----------------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(pEnv->Values[m_nDragItem - 1] <= ENVELOPE_MAX - stepsize)
            pEnv->Values[m_nDragItem - 1] += stepsize;
    else
            pEnv->Values[m_nDragItem - 1] = ENVELOPE_MAX;

    SetInstrumentModified();
    InvalidateRect(NULL, FALSE);
}


void CViewInstrument::EnvKbdMovePointDown(uint8_t stepsize)
//------------------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(pEnv->Values[m_nDragItem - 1] >= ENVELOPE_MIN + stepsize)
            pEnv->Values[m_nDragItem - 1] -= stepsize;
    else
            pEnv->Values[m_nDragItem - 1] = ENVELOPE_MIN;

    SetInstrumentModified();
    InvalidateRect(NULL, FALSE);
}

void CViewInstrument::EnvKbdInsertPoint()
//---------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr) return;
    if(!IsDragItemEnvPoint()) m_nDragItem = pEnv->num_nodes;
    uint16_t newTick = pEnv->Ticks[pEnv->num_nodes - 1] + 4;        // if last point is selected: add point after last point
    uint8_t newVal = pEnv->Values[pEnv->num_nodes - 1];
    // if some other point is selected: interpolate between this and next point (if there's room between them)
    if(m_nDragItem < pEnv->num_nodes && (pEnv->Ticks[m_nDragItem] - pEnv->Ticks[m_nDragItem - 1] > 1))
    {
            newTick = (pEnv->Ticks[m_nDragItem - 1] + pEnv->Ticks[m_nDragItem]) / 2;
            newVal = (pEnv->Values[m_nDragItem - 1] + pEnv->Values[m_nDragItem]) / 2;
    }

    UINT newPoint = EnvInsertPoint(newTick, newVal);
    if(newPoint > 0) m_nDragItem = newPoint;
}


void CViewInstrument::EnvKbdRemovePoint()
//---------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint() || pEnv->num_nodes == 0) return;
    if(m_nDragItem > pEnv->num_nodes) m_nDragItem = pEnv->num_nodes;
    EnvRemovePoint(m_nDragItem - 1);
}


void CViewInstrument::EnvKbdSetLoopStart()
//----------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(!EnvGetLoop())
            EnvSetLoopStart(0);
    EnvSetLoopStart(m_nDragItem - 1);
    GetDocument()->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);        // sanity checks are done in GetEnvelopePtr() already
}


void CViewInstrument::EnvKbdSetLoopEnd()
//--------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(!EnvGetLoop())
    {
            EnvSetLoop(true);
            EnvSetLoopStart(0);
    }
    EnvSetLoopEnd(m_nDragItem - 1);
    GetDocument()->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);        // sanity checks are done in GetEnvelopePtr() already
}


void CViewInstrument::EnvKbdSetSustainStart()
//-------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(!EnvGetSustain())
            EnvSetSustain(true);
    EnvSetSustainStart(m_nDragItem - 1);
    GetDocument()->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);        // sanity checks are done in GetEnvelopePtr() already
}


void CViewInstrument::EnvKbdSetSustainEnd()
//-----------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(!EnvGetSustain())
    {
            EnvSetSustain(true);
            EnvSetSustainStart(0);
    }
    EnvSetSustainEnd(m_nDragItem - 1);
    GetDocument()->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);        // sanity checks are done in GetEnvelopePtr() already
}


void CViewInstrument::EnvKbdToggleReleaseNode()
//---------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr || !IsDragItemEnvPoint()) return;
    if(EnvToggleReleaseNode(m_nDragItem - 1))
    {
            CModDoc *pModDoc = GetDocument();        // sanity checks are done in GetEnvelopePtr() already
            SetInstrumentModified();
            pModDoc->UpdateAllViews(NULL, (m_nInstrument << HINT_SHIFT_INS) | HINT_ENVELOPE, NULL);
    }
}


// Get a pointer to the currently active instrument.
modplug::tracker::modinstrument_t *CViewInstrument::GetInstrumentPtr() const
//------------------------------------------------------
{
    CModDoc *pModDoc = GetDocument();
    if(pModDoc == nullptr) return nullptr;
    module_renderer *pSndFile = pModDoc->GetSoundFile();
    if(pSndFile == nullptr) return nullptr;
    return pSndFile->Instruments[m_nInstrument];
}

// Get a pointer to the currently selected envelope.
// This function also implicitely validates the moddoc and soundfile pointers.
modplug::tracker::modenvelope_t *CViewInstrument::GetEnvelopePtr() const
//---------------------------------------------------------
{
    // First do some standard checks...
    modplug::tracker::modinstrument_t *pIns = GetInstrumentPtr();
    if(pIns == nullptr) return nullptr;

    // Now for the real thing.
    modplug::tracker::modenvelope_t *envelope = nullptr;

    switch(m_nEnv)
    {
    case ENV_VOLUME:
            envelope = &pIns->volume_envelope;
            break;
    case ENV_PANNING:
            envelope = &pIns->panning_envelope;
            break;
    case ENV_PITCH:
            envelope = &pIns->pitch_envelope;
            break;
    }

    return envelope;
}


bool CViewInstrument::CanMovePoint(UINT envPoint, int step)
//---------------------------------------------------------
{
    modplug::tracker::modenvelope_t *pEnv = GetEnvelopePtr();
    if(pEnv == nullptr) return false;

    // Can't move first point
    if(envPoint == 0)
    {
            return false;
    }
    // Can't move left of previous point
    if((step < 0) && (pEnv->Ticks[envPoint] - pEnv->Ticks[envPoint - 1] >= -step + GetDocument()->GetSoundFile()->IsCompatibleMode(TRK_IMPULSETRACKER|TRK_FASTTRACKER2) ? 0 : 1))
    {
            return false;
    }
    // Can't move right of next point
    if((step > 0) && (envPoint < pEnv->num_nodes - 1) && (pEnv->Ticks[envPoint + 1] - pEnv->Ticks[envPoint] >= step + GetDocument()->GetSoundFile()->IsCompatibleMode(TRK_IMPULSETRACKER|TRK_FASTTRACKER2) ? 0 : 1))
    {
            return false;
    }
    // Limit envelope length
    if((envPoint == pEnv->num_nodes - 1) && pEnv->Ticks[envPoint] + step > ENVELOPE_MAX_LENGTH)
    {
            return false;
    }
    return true;
}


BOOL CViewInstrument::OnMouseWheel(UINT nFlags, short zDelta, CPoint pt)
//----------------------------------------------------------------------
{
    // Ctrl + mouse wheel: envelope zoom.
    if (nFlags == MK_CONTROL)
    {
            // Speed up zoom scrolling by some factor (might need some tuning).
            const float speedUpFactor = Util::Max(1.0f, m_fZoom * 7.0f / ENV_MAX_ZOOM);
            EnvSetZoom(m_fZoom + speedUpFactor * (zDelta / WHEEL_DELTA));
    }

    return CModScrollView::OnMouseWheel(nFlags, zDelta, pt);
}
