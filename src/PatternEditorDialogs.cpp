/*
 *
 * PatternEditorDialogs.cpp
 * ------------------------
 * Purpose: Code for various dialogs that are used in the pattern editor
 * Notes  : (currently none)
 * Authors: Olivier Lapicque
 *          OpenMPT Devs
 *
 */

#include "stdafx.h"
#include "Mainfrm.h"
#include "PatternEditorDialogs.h"

#include "tracker/modevent.h"

using namespace modplug::tracker;


// -> CODE#0010
// -> DESC="add extended parameter mechanism to pattern effects"
void getXParam(uint8_t command, UINT nPat, UINT nRow, UINT nChannel, module_renderer *pSndFile, UINT * xparam, UINT * multiplier)
{
    if(xparam == NULL || multiplier == NULL) return;

    modplug::tracker::modevent_t mca = modplug::tracker::modevent_t::empty();
    UINT i,xp = 0, ml = 1;
    int nCmdRow = (int)nRow;

    // If the current command is a parameter extension command
    if(command == CmdExtendedParameter){

            nCmdRow--;

            // Try to find previous command parameter to be extended
            while(nCmdRow >= 0){
                    i = nCmdRow * pSndFile->m_nChannels + nChannel;
                    mca = pSndFile->Patterns[nPat][i];
                    if(mca.command == CmdOffset || mca.command == CmdPatternBreak || mca.command == CmdPatternBreak)
                            break;
                    if(mca.command != CmdExtendedParameter){
                            nCmdRow = -1;
                            break;
                    }
                    nCmdRow--;
            }
    }
    // Else if current row do not own any satisfying command parameter to extend, set return state
    else if(command != CmdOffset && command != CmdPatternBreak && command != CmdTempo) nCmdRow = -1;

    // If an 'extendable' command parameter has been found,
    if(nCmdRow >= 0){
            i = nCmdRow * pSndFile->m_nChannels + nChannel;
            mca = pSndFile->Patterns[nPat][i];

            // Find extension resolution (8 to 24 bits)
            UINT n = 1;
            while(n < 4 && nCmdRow+n < pSndFile->Patterns[nPat].GetNumRows()){
                    i = (nCmdRow+n) * pSndFile->m_nChannels + nChannel;
                    if(pSndFile->Patterns[nPat][i].command != CmdExtendedParameter) break;
                    n++;
            }

            // Parameter extension found (above 8 bits non-standard parameters)
            if(n > 1){
                    // Limit offset command to 24 bits, other commands to 16 bits
                    n = mca.command == CmdOffset ? n : (n > 2 ? 2 : n);

                    // Compute extended value WITHOUT current row parameter value : this parameter
                    // is being currently edited (this is why this function is being called) so we
                    // only need to compute a multiplier so that we can add its contribution while
                    // its value is changed by user
                    for(UINT j = 0 ; j < n ; j++){
                            i = (nCmdRow+j) * pSndFile->m_nChannels + nChannel;
                            mca = pSndFile->Patterns[nPat][i];

                            UINT k = 8*(n-j-1);
                            if(nCmdRow+j == nRow) ml = 1<<k;
                            else xp += (mca.param<<k);
                    }
            }
            // No parameter extension to perform (8 bits standard parameter),
            // just care about offset command special case (16 bits, fake)
            else if(mca.command == CmdOffset) ml <<= 8;
    }

    // Return x-parameter
    *multiplier = ml;
    *xparam = xp;
}
// -! NEW_FEATURE#0010


//////////////////////////////////////////////////////////////////////////////////////////
// Find/Replace Dialog

BEGIN_MESSAGE_MAP(CFindReplaceTab, CPropertyPage)
    ON_CBN_SELCHANGE(IDC_COMBO1,        OnNoteChanged)
    ON_CBN_SELCHANGE(IDC_COMBO2,        OnInstrChanged)
    ON_CBN_SELCHANGE(IDC_COMBO3,        OnVolCmdChanged)
    ON_CBN_SELCHANGE(IDC_COMBO4,        OnVolumeChanged)
    ON_CBN_SELCHANGE(IDC_COMBO5,        OnEffectChanged)
    ON_CBN_SELCHANGE(IDC_COMBO6,        OnParamChanged)
    ON_COMMAND(IDC_CHECK1,                        OnCheckNote)
    ON_COMMAND(IDC_CHECK2,                        OnCheckInstr)
    ON_COMMAND(IDC_CHECK3,                        OnCheckVolCmd)
    ON_COMMAND(IDC_CHECK4,                        OnCheckVolume)
    ON_COMMAND(IDC_CHECK5,                        OnCheckEffect)
    ON_COMMAND(IDC_CHECK6,                        OnCheckParam)

    ON_COMMAND(IDC_CHECK7,                        OnCheckChannelSearch)
END_MESSAGE_MAP()


BOOL CFindReplaceTab::OnInitDialog()
//----------------------------------
{
    CHAR s[256];
    CComboBox *combo;
    module_renderer *pSndFile;

    CPropertyPage::OnInitDialog();
    if (!m_pModDoc) return TRUE;
    pSndFile = m_pModDoc->GetSoundFile();
    // Search flags
    if (m_dwFlags & PATSEARCH_NOTE) CheckDlgButton(IDC_CHECK1, MF_CHECKED);
    if (m_dwFlags & PATSEARCH_INSTR) CheckDlgButton(IDC_CHECK2, MF_CHECKED);
    if (m_dwFlags & PATSEARCH_VOLCMD) CheckDlgButton(IDC_CHECK3, MF_CHECKED);
    if (m_dwFlags & PATSEARCH_VOLUME) CheckDlgButton(IDC_CHECK4, MF_CHECKED);
    if (m_dwFlags & PATSEARCH_COMMAND) CheckDlgButton(IDC_CHECK5, MF_CHECKED);
    if (m_dwFlags & PATSEARCH_PARAM) CheckDlgButton(IDC_CHECK6, MF_CHECKED);
    if (m_bReplace)
    {
            if (m_dwFlags & PATSEARCH_REPLACE) CheckDlgButton(IDC_CHECK7, MF_CHECKED);
            if (m_dwFlags & PATSEARCH_REPLACEALL) CheckDlgButton(IDC_CHECK8, MF_CHECKED);
    } else
    {
            if (m_dwFlags & PATSEARCH_CHANNEL) CheckDlgButton(IDC_CHECK7, MF_CHECKED);
            int nButton = IDC_RADIO1;
            if((m_dwFlags & PATSEARCH_FULLSEARCH))
            {
                    nButton = IDC_RADIO2;
            } else if(/*(m_dwFlags & PATSEARCH_PATSELECTION) &&*/ m_bPatSel)
            {
                    nButton = IDC_RADIO3;
            }
            CheckRadioButton(IDC_RADIO1, IDC_RADIO3, nButton);
            GetDlgItem(IDC_RADIO3)->EnableWindow(m_bPatSel ? TRUE : FALSE);
            SetDlgItemInt(IDC_EDIT1, m_nMinChannel + 1);
            SetDlgItemInt(IDC_EDIT2, m_nMaxChannel + 1);
    }
    // Note
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
            combo->InitStorage(150, 6);
            combo->SetItemData(combo->AddString("..."), 0);
            if (m_bReplace)
            {
                    combo->SetItemData(combo->AddString("note-1"), replaceNoteMinusOne);
                    combo->SetItemData(combo->AddString("note+1"), replaceNotePlusOne);
                    combo->SetItemData(combo->AddString("-1 oct"), replaceNoteMinusOctave);
                    combo->SetItemData(combo->AddString("+1 oct"), replaceNotePlusOctave);
            } else
            {
                    combo->SetItemData(combo->AddString("any"), findAny);
            }
            AppendNotesToControlEx(*combo, pSndFile);

            UINT ncount = combo->GetCount();
            for (UINT i=0; i<ncount; i++) if (m_nNote == combo->GetItemData(i))
            {
                    combo->SetCurSel(i);
                    break;
            }
    }
    // Instrument
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO2)) != NULL)
    {
            combo->SetItemData(combo->AddString(".."), 0);
            if (m_bReplace)
            {
                    combo->SetItemData(combo->AddString("ins-1"), replaceInstrumentMinusOne);
                    combo->SetItemData(combo->AddString("ins+1"), replaceInstrumentPlusOne);
            }
            for (UINT n=1; n<MAX_INSTRUMENTS; n++)
            {
                    if (pSndFile->m_nInstruments)
                    {
                            wsprintf(s, "%03d:%s", n, (pSndFile->Instruments[n]) ? (LPCTSTR)pSndFile->GetInstrumentName(n) : "");
                    } else
                    {
                            wsprintf(s, "%03d:%s", n, pSndFile->m_szNames[n]);
                    }
                    combo->SetItemData(combo->AddString(s), n);
            }
            UINT ncount = combo->GetCount();
            for (UINT i=0; i<ncount; i++)
            {
                    if (m_nInstr == combo->GetItemData(i) || (cInstrRelChange == -1 && combo->GetItemData(i) == replaceInstrumentMinusOne) || (cInstrRelChange == 1 && combo->GetItemData(i) == replaceInstrumentPlusOne))
                    {
                            combo->SetCurSel(i);
                            break;
                    }
            }
    }
    // Volume Command
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO3)) != NULL)
    {
            combo->InitStorage(m_pModDoc->GetNumVolCmds(), 15);
            combo->SetItemData(combo->AddString(" None"), (uint32_t)-1);
            UINT count = m_pModDoc->GetNumVolCmds();
            for (UINT n=0; n<count; n++)
            {
                    m_pModDoc->GetVolCmdInfo(n, s);
                    if (s[0]) combo->SetItemData(combo->AddString(s), n);
            }
            combo->SetCurSel(0);
            UINT fxndx = m_pModDoc->GetIndexFromVolCmd(m_nVolCmd);
            for (UINT i=0; i<=count; i++) if (fxndx == combo->GetItemData(i))
            {
                    combo->SetCurSel(i);
                    break;
            }
    }
    // Volume
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO4)) != NULL)
    {
            combo->InitStorage(64, 4);
            for (UINT n=0; n<=64; n++)
            {
                    wsprintf(s, "%02d", n);
                    combo->SetItemData(combo->AddString(s), n);
            }
            UINT ncount = combo->GetCount();
            for (UINT i=0; i<ncount; i++) if (m_nVol == combo->GetItemData(i))
            {
                    combo->SetCurSel(i);
                    break;
            }
    }
    // Command
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO5)) != NULL)
    {
            combo->InitStorage(m_pModDoc->GetNumEffects(), 20);
            combo->SetItemData(combo->AddString(" None"), (uint32_t)-1);
            UINT count = m_pModDoc->GetNumEffects();
            for (UINT n=0; n<count; n++)
            {
                    m_pModDoc->GetEffectInfo(n, s, true);
                    if (s[0]) combo->SetItemData(combo->AddString(s), n);
            }
            combo->SetCurSel(0);
            UINT fxndx = m_pModDoc->GetIndexFromEffect(m_nCommand, m_nParam);
            for (UINT i=0; i<=count; i++) if (fxndx == combo->GetItemData(i))
            {
                    combo->SetCurSel(i);
                    break;
            }
    }
    ChangeEffect();
    OnCheckChannelSearch();
    return TRUE;
}


void CFindReplaceTab::ChangeEffect()
//----------------------------------
{
    int fxndx = -1;
    CComboBox *combo;
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO5)) != NULL)
    {
            fxndx = combo->GetItemData(combo->GetCurSel());
    }
    // Update Param range
    if (((combo = (CComboBox *)GetDlgItem(IDC_COMBO6)) != NULL) && (m_pModDoc))
    {
            UINT oldcount = combo->GetCount();
            UINT newcount = m_pModDoc->IsExtendedEffect(fxndx) ? 16 : 256;
            if (oldcount != newcount)
            {
                    CHAR s[16];
                    int newpos;
                    if (oldcount) newpos = combo->GetCurSel() % newcount; else newpos = m_nParam % newcount;
                    combo->ResetContent();
                    for (UINT i=0; i<newcount; i++)
                    {
                            wsprintf(s, (newcount == 256) ? "%02X" : "%X", i);
                            combo->SetItemData(combo->AddString(s), i);
                    }
                    combo->SetCurSel(newpos);
            }
    }
}


void CFindReplaceTab::OnCheckChannelSearch()
//------------------------------------------
{
    if (!m_bReplace)
    {
            BOOL b = IsDlgButtonChecked(IDC_CHECK7);
            ::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT1), b);
            ::EnableWindow(::GetDlgItem(m_hWnd, IDC_EDIT2), b);
    }
}


void CFindReplaceTab::OnOK()
//--------------------------
{
    CComboBox *combo;

    // Search flags
    m_dwFlags = 0;
    if (IsDlgButtonChecked(IDC_CHECK1)) m_dwFlags |= PATSEARCH_NOTE;
    if (IsDlgButtonChecked(IDC_CHECK2)) m_dwFlags |= PATSEARCH_INSTR;
    if (IsDlgButtonChecked(IDC_CHECK3)) m_dwFlags |= PATSEARCH_VOLCMD;
    if (IsDlgButtonChecked(IDC_CHECK4)) m_dwFlags |= PATSEARCH_VOLUME;
    if (IsDlgButtonChecked(IDC_CHECK5)) m_dwFlags |= PATSEARCH_COMMAND;
    if (IsDlgButtonChecked(IDC_CHECK6)) m_dwFlags |= PATSEARCH_PARAM;
    if (m_bReplace)
    {
            if (IsDlgButtonChecked(IDC_CHECK7)) m_dwFlags |= PATSEARCH_REPLACE;
            if (IsDlgButtonChecked(IDC_CHECK8)) m_dwFlags |= PATSEARCH_REPLACEALL;
    } else
    {
            if (IsDlgButtonChecked(IDC_CHECK7)) m_dwFlags |= PATSEARCH_CHANNEL;
            if (IsDlgButtonChecked(IDC_RADIO2)) m_dwFlags |= PATSEARCH_FULLSEARCH;
            if (IsDlgButtonChecked(IDC_RADIO3)) m_dwFlags |= PATSEARCH_PATSELECTION;
    }
    // Note
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
            m_nNote = combo->GetItemData(combo->GetCurSel());
    }
    // Instrument
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO2)) != NULL)
    {
            m_nInstr = 0;
            cInstrRelChange = 0;
            switch(combo->GetItemData(combo->GetCurSel()))
            {
            case replaceInstrumentMinusOne:
                    cInstrRelChange = -1;
                    break;
            case replaceInstrumentPlusOne:
                    cInstrRelChange = 1;
                    break;
            default:
                    m_nInstr = combo->GetItemData(combo->GetCurSel());
                    break;
            }
    }
    // Volume Command
    if (((combo = (CComboBox *)GetDlgItem(IDC_COMBO3)) != NULL) && (m_pModDoc))
    {
            m_nVolCmd = m_pModDoc->GetVolCmdFromIndex(combo->GetItemData(combo->GetCurSel()));
    }
    // Volume
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO4)) != NULL)
    {
            m_nVol = combo->GetItemData(combo->GetCurSel());
    }
    // Effect
    int effectIndex = -1;
    if (((combo = (CComboBox *)GetDlgItem(IDC_COMBO5)) != NULL) && (m_pModDoc))
    {
            int n = -1; // unused parameter adjustment
            effectIndex = combo->GetItemData(combo->GetCurSel());
            m_nCommand = m_pModDoc->GetEffectFromIndex(effectIndex, n);
    }
    // Param
    m_nParam = 0;
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO6)) != NULL)
    {
            m_nParam = combo->GetItemData(combo->GetCurSel());

            // Apply parameter value mask if required (e.g. SDx has mask D0).
            if (effectIndex > -1)
            {
                    m_nParam |= m_pModDoc->GetEffectMaskFromIndex(effectIndex);
            }
    }
    // Min/Max channels
    if (!m_bReplace)
    {
            m_nMinChannel = GetDlgItemInt(IDC_EDIT1) - 1;
            m_nMaxChannel = GetDlgItemInt(IDC_EDIT2) - 1;
            if (m_nMaxChannel < m_nMinChannel) m_nMaxChannel = m_nMinChannel;
    }
    CPropertyPage::OnOK();
}


/////////////////////////////////////////////////////////////////////////////////////////////
// CPatternPropertiesDlg

BEGIN_MESSAGE_MAP(CPatternPropertiesDlg, CDialog)
    ON_COMMAND(IDC_BUTTON_HALF,                OnHalfRowNumber)
    ON_COMMAND(IDC_BUTTON_DOUBLE,        OnDoubleRowNumber)
    ON_COMMAND(IDC_CHECK1,                        OnOverrideSignature)
END_MESSAGE_MAP()

BOOL CPatternPropertiesDlg::OnInitDialog()
//----------------------------------------
{
    CComboBox *combo;
    CDialog::OnInitDialog();
    combo = (CComboBox *)GetDlgItem(IDC_COMBO1);
    const module_renderer *pSndFile = (m_pModDoc) ? m_pModDoc->GetSoundFile() : nullptr;
    if ((pSndFile) && (m_nPattern < pSndFile->Patterns.Size()) && (combo))
    {
            CHAR s[256];
            UINT nrows = pSndFile->Patterns[m_nPattern].GetNumRows();

            const CModSpecifications& specs = pSndFile->GetModSpecifications();
            for (UINT irow = specs.patternRowsMin; irow <= specs.patternRowsMax; irow++)
            {
                    wsprintf(s, "%d", irow);
                    combo->AddString(s);
            }
            combo->SetCurSel(nrows - specs.patternRowsMin);
            wsprintf(s, "Pattern #%d: %d row%s (%dK)",
                    m_nPattern,
                    pSndFile->Patterns[m_nPattern].GetNumRows(),
                    (pSndFile->Patterns[m_nPattern].GetNumRows() == 1) ? "" : "s",
                    (pSndFile->Patterns[m_nPattern].GetNumRows() * pSndFile->m_nChannels * sizeof(modplug::tracker::modevent_t)) / 1024);
            SetDlgItemText(IDC_TEXT1, s);

            // Window title
            CHAR szName[MAX_PATTERNNAME + 1];
            pSndFile->Patterns[m_nPattern].GetName(szName, MAX_PATTERNNAME);
            if(strlen(szName))
            {
                    strcat(szName, ")");
            }
            wsprintf(s, "Pattern Properties for Pattern #%d%s%s", m_nPattern, strlen(szName) ? " (" : "", szName);
            SetWindowText(s);

            // pattern time signature
            const bool bOverride = pSndFile->Patterns[m_nPattern].GetOverrideSignature();
            UINT nRPB = pSndFile->Patterns[m_nPattern].GetRowsPerBeat(), nRPM = pSndFile->Patterns[m_nPattern].GetRowsPerMeasure();
            if(nRPB == 0 || !bOverride) nRPB = pSndFile->m_nDefaultRowsPerBeat;
            if(nRPM == 0 || !bOverride) nRPM = pSndFile->m_nDefaultRowsPerMeasure;

            GetDlgItem(IDC_CHECK1)->EnableWindow(pSndFile->GetModSpecifications().hasPatternSignatures ? TRUE : FALSE);
            CheckDlgButton(IDC_CHECK1, bOverride ? MF_CHECKED : MF_UNCHECKED);
            SetDlgItemInt(IDC_EDIT_ROWSPERBEAT, nRPB, FALSE);
            SetDlgItemInt(IDC_EDIT_ROWSPERMEASURE, nRPM, FALSE);
            OnOverrideSignature();
    }
    return TRUE;
}


void CPatternPropertiesDlg::OnHalfRowNumber()
//-------------------------------------------
{
    const module_renderer *pSndFile = (m_pModDoc) ? m_pModDoc->GetSoundFile() : nullptr;
    if(pSndFile == nullptr)
            return;

    UINT nRows = GetDlgItemInt(IDC_COMBO1, NULL, FALSE);
    nRows /= 2;
    if(nRows < pSndFile->GetModSpecifications().patternRowsMin)
            nRows = pSndFile->GetModSpecifications().patternRowsMin;
    SetDlgItemInt(IDC_COMBO1, nRows, FALSE);
}


void CPatternPropertiesDlg::OnDoubleRowNumber()
//---------------------------------------------
{
    const module_renderer *pSndFile = (m_pModDoc) ? m_pModDoc->GetSoundFile() : nullptr;
    if(pSndFile == nullptr)
            return;

    UINT nRows = GetDlgItemInt(IDC_COMBO1, NULL, FALSE);
    nRows *= 2;
    if(nRows > pSndFile->GetModSpecifications().patternRowsMax)
            nRows = pSndFile->GetModSpecifications().patternRowsMax;
    SetDlgItemInt(IDC_COMBO1, nRows, FALSE);
}


void CPatternPropertiesDlg::OnOverrideSignature()
//-----------------------------------------------
{
    GetDlgItem(IDC_EDIT_ROWSPERBEAT)->EnableWindow(IsDlgButtonChecked(IDC_CHECK1));
    GetDlgItem(IDC_EDIT_ROWSPERMEASURE)->EnableWindow(IsDlgButtonChecked(IDC_CHECK1));
}


void CPatternPropertiesDlg::OnOK()
//--------------------------------
{
    module_renderer *pSndFile = (m_pModDoc) ? m_pModDoc->GetSoundFile() : nullptr;
    if(pSndFile)
    {
            // Update pattern signature if necessary
            if(pSndFile->GetModSpecifications().hasPatternSignatures)
            {
                    if(IsDlgButtonChecked(IDC_CHECK1))        // Enable signature
                    {
                            modplug::tracker::rowindex_t nNewBeat = (modplug::tracker::rowindex_t)GetDlgItemInt(IDC_EDIT_ROWSPERBEAT, NULL, FALSE), nNewMeasure = (modplug::tracker::rowindex_t)GetDlgItemInt(IDC_EDIT_ROWSPERMEASURE, NULL, FALSE);
                            if(nNewBeat != pSndFile->Patterns[m_nPattern].GetRowsPerBeat() || nNewMeasure != pSndFile->Patterns[m_nPattern].GetRowsPerMeasure())
                            {
                                    if(!pSndFile->Patterns[m_nPattern].SetSignature(nNewBeat, nNewMeasure))
                                    {
                                            MessageBox("Invalid time signature!", "Pattern Properties", MB_OK|MB_ICONEXCLAMATION);
                                            GetDlgItem(IDC_EDIT_ROWSPERBEAT)->SetFocus();
                                            return;
                                    }
                                    m_pModDoc->SetModified();
                            }
                    } else        // Disable signature
                    {
                            if(pSndFile->Patterns[m_nPattern].GetOverrideSignature())
                            {
                                    pSndFile->Patterns[m_nPattern].RemoveSignature();
                                    m_pModDoc->SetModified();
                            }
                    }
            }

            UINT n = GetDlgItemInt(IDC_COMBO1, NULL, FALSE);
            pSndFile->Patterns[m_nPattern].Resize(n);
    }
    CDialog::OnOK();
}


////////////////////////////////////////////////////////////////////////////////////////////
// CEditCommand

BEGIN_MESSAGE_MAP(CEditCommand, CPropertySheet)
    ON_WM_ACTIVATE()
    ON_WM_CLOSE()
    ON_WM_DESTROY()
END_MESSAGE_MAP()


CEditCommand::CEditCommand()
//--------------------------
{
    m_pModDoc = NULL;
    m_hWndView = NULL;
    m_nPattern = 0;
    m_nRow = 0;
    m_nChannel = 0;
    m_pageNote = NULL;
    m_pageVolume = NULL;
    m_pageEffect = NULL;
    m_bModified = false;
}


BOOL CEditCommand::SetParent(CWnd *parent, CModDoc *pModDoc)
//----------------------------------------------------------
{
    if ((!parent) || (!pModDoc)) return FALSE;
    m_hWndView = parent->m_hWnd;
    m_pModDoc = pModDoc;
    m_pageNote = new CPageEditNote(m_pModDoc, this);
    m_pageVolume = new CPageEditVolume(m_pModDoc, this);
    m_pageEffect = new CPageEditEffect(m_pModDoc, this);
    AddPage(m_pageNote);
    AddPage(m_pageVolume);
    AddPage(m_pageEffect);
    if (!CPropertySheet::Create(parent,
            WS_SYSMENU|WS_POPUP|WS_CAPTION,        WS_EX_DLGMODALFRAME)) return FALSE;
    ModifyStyleEx(0, WS_EX_TOOLWINDOW|WS_EX_PALETTEWINDOW, SWP_FRAMECHANGED);
    return TRUE;
}

void CEditCommand::OnDestroy()
//----------------------------
{
    CPropertySheet::OnDestroy();

    if (m_pageNote) {
            m_pageNote->DestroyWindow();
            delete m_pageNote;
    }
    if (m_pageVolume) {
            m_pageVolume->DestroyWindow();
            delete m_pageVolume;
    }
    if (m_pageEffect) {
            m_pageEffect->DestroyWindow();
            delete m_pageEffect;
    }
}

BOOL CEditCommand::PreTranslateMessage(MSG *pMsg)
//-----------------------------------------------
{
    if ((pMsg) && (pMsg->message == WM_KEYDOWN))
    {
            if ((pMsg->wParam == VK_ESCAPE) || (pMsg->wParam == VK_RETURN) || (pMsg->wParam == VK_APPS))
            {
                    OnClose();
                    return TRUE;
            }
    }
    return CPropertySheet::PreTranslateMessage(pMsg);
}


BOOL CEditCommand::ShowEditWindow(UINT nPat, uint32_t dwCursor)
//----------------------------------------------------------
{
    CHAR s[64];
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    UINT nRow = dwCursor >> 16;
    UINT nChannel = (dwCursor & 0xFFFF) >> 3;

    if ((nPat >= pSndFile->Patterns.Size()) || (!m_pModDoc)
            || (nRow >= pSndFile->Patterns[nPat].GetNumRows()) || (nChannel >= pSndFile->m_nChannels)
            || (!pSndFile->Patterns[nPat])) return FALSE;
    m_Command = pSndFile->Patterns[nPat][nRow * pSndFile->m_nChannels + nChannel];
    m_nRow = nRow;
    m_nChannel = nChannel;
    m_nPattern = nPat;
    m_bModified = false;
    // Init Pages
    if (m_pageNote) m_pageNote->Init(m_Command);
    if (m_pageVolume) m_pageVolume->Init(m_Command);

    // -> CODE#0010
    // -> DESC="add extended parameter mechanism to pattern effects"
    //        if (m_pageEffect) m_pageEffect->Init(m_Command);
    if (m_pageEffect){
            UINT xp = 0, ml = 1;
            getXParam(m_Command.command,nPat,nRow,nChannel,pSndFile,&xp,&ml);
            m_pageEffect->Init(m_Command);
            m_pageEffect->XInit(xp,ml);
    }
    // -! NEW_FEATURE#0010

    // Update Window Title
    wsprintf(s, "Note Properties - Row %d, Channel %d", m_nRow, m_nChannel+1);
    SetTitle(s);
    // Activate Page
    UINT nPage = 2;
    dwCursor &= 7;
    if (dwCursor < 2) nPage = 0;
    else if (dwCursor < 3) nPage = 1;
    SetActivePage(nPage);
    if (m_pageNote) m_pageNote->UpdateDialog();
    if (m_pageVolume) m_pageVolume->UpdateDialog();
    if (m_pageEffect) m_pageEffect->UpdateDialog();
    //ShowWindow(SW_SHOW);
    ShowWindow(SW_RESTORE);
    return TRUE;
}


void CEditCommand::UpdateNote(UINT note, UINT instr)
//--------------------------------------------------
{
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    if ((m_nPattern >= pSndFile->Patterns.Size()) || (!m_pModDoc)
            || (m_nRow >= pSndFile->Patterns[m_nPattern].GetNumRows())
            || (m_nChannel >= pSndFile->m_nChannels)
            || (!pSndFile->Patterns[m_nPattern])) return;
    modplug::tracker::modevent_t *m = pSndFile->Patterns[m_nPattern]+m_nRow*pSndFile->m_nChannels+m_nChannel;
    if ((m->note != note) || (m->instr != instr))
    {
            if(!m_bModified)        // let's create just one undo step.
            {
                    m_pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, m_nChannel, m_nRow, 1, 1);
                    m_bModified = true;
            }
            m->note = note;
            m->instr = instr;
            m_Command = *m;
            m_pModDoc->SetModified();
            // -> CODE#0008
            // -> DESC"#define to set pattern bad_max size (number of rows) limit (now set to 1024 instead of 256)"
            //                m_pModDoc->UpdateAllViews(NULL, (m_nRow << 24) | HINT_PATTERNROW, NULL);
            m_pModDoc->UpdateAllViews(NULL, (m_nRow << HINT_SHIFT_ROW) | HINT_PATTERNROW, NULL);
            // -! BEHAVIOUR_CHANGE#0008
    }
}


void CEditCommand::UpdateVolume(UINT volcmd, UINT vol)
//----------------------------------------------------
{
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    if ((m_nPattern >= pSndFile->Patterns.Size()) || (!m_pModDoc)
            || (m_nRow >= pSndFile->Patterns[m_nPattern].GetNumRows())
            || (m_nChannel >= pSndFile->m_nChannels)
            || (!pSndFile->Patterns[m_nPattern])) return;
    modplug::tracker::modevent_t *m = pSndFile->Patterns[m_nPattern]+m_nRow*pSndFile->m_nChannels+m_nChannel;
    if ((m->volcmd != volcmd) || (m->vol != vol))
    {
            if(!m_bModified)        // let's create just one undo step.
            {
                    m_pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, m_nChannel, m_nRow, 1, 1);
                    m_bModified = true;
            }
            //XXXih: gross
            m->volcmd = (modplug::tracker::volcmd_t) volcmd;
            m->vol = vol;
            m_pModDoc->SetModified();
            // -> CODE#0008
            // -> DESC"#define to set pattern bad_max size (number of rows) limit (now set to 1024 instead of 256)"
            //                m_pModDoc->UpdateAllViews(NULL, (m_nRow << 24) | HINT_PATTERNROW, NULL);
            m_pModDoc->UpdateAllViews(NULL, (m_nRow << HINT_SHIFT_ROW) | HINT_PATTERNROW, NULL);
            // -! BEHAVIOUR_CHANGE#0008
    }
}


void CEditCommand::UpdateEffect(UINT raw_command, UINT param)
//-------------------------------------------------------
{
    auto command = (modplug::tracker::cmd_t) raw_command;
    module_renderer *pSndFile = m_pModDoc->GetSoundFile();
    if ((m_nPattern >= pSndFile->Patterns.Size()) || (!m_pModDoc)
            || (m_nRow >= pSndFile->Patterns[m_nPattern].GetNumRows())
            || (m_nChannel >= pSndFile->m_nChannels)
            || (!pSndFile->Patterns[m_nPattern])) return;
    modplug::tracker::modevent_t *m = pSndFile->Patterns[m_nPattern]+m_nRow*pSndFile->m_nChannels+m_nChannel;

    // -> CODE#0010
    // -> DESC="add extended parameter mechanism to pattern effects"
    if(command == CmdOffset || command == CmdPatternBreak || command == CmdTempo || command == CmdExtendedParameter){
            UINT xp = 0, ml = 1;
            getXParam(command,m_nPattern,m_nRow,m_nChannel,pSndFile,&xp,&ml);
            m_pageEffect->XInit(xp,ml);
            m_pageEffect->UpdateDialog();
    }
    // -! NEW_FEATURE#0010

    if ((m->command != command) || (m->param != param))
    {
            if(!m_bModified)        // let's create just one undo step.
            {
                    m_pModDoc->GetPatternUndo()->PrepareUndo(m_nPattern, m_nChannel, m_nRow, 1, 1);
                    m_bModified = true;
            }
            m->command = command;
            m->param = param;
            m_pModDoc->SetModified();
            // -> CODE#0008
            // -> DESC"#define to set pattern bad_max size (number of rows) limit (now set to 1024 instead of 256)"
            //                m_pModDoc->UpdateAllViews(NULL, (m_nRow << 24) | HINT_PATTERNROW, NULL);
            m_pModDoc->UpdateAllViews(NULL, (m_nRow << HINT_SHIFT_ROW) | HINT_PATTERNROW, NULL);
            // -! BEHAVIOUR_CHANGE#0008
    }
}


void CEditCommand::OnActivate(UINT nState, CWnd *pWndOther, BOOL bMinimized)
//--------------------------------------------------------------------------
{
    CWnd::OnActivate(nState, pWndOther, bMinimized);
    if (nState == WA_INACTIVE) ShowWindow(SW_HIDE);
}


//////////////////////////////////////////////////////////////////////////////////////
// CPageEditCommand

BOOL CPageEditCommand::OnInitDialog()
//-----------------------------------
{
    CPropertyPage::OnInitDialog();
    m_bInitialized = true;
    UpdateDialog();
    return TRUE;
}


//////////////////////////////////////////////////////////////////////////////////////
// CPageEditNote

BEGIN_MESSAGE_MAP(CPageEditNote, CPageEditCommand)
    ON_CBN_SELCHANGE(IDC_COMBO1,        OnNoteChanged)
    ON_CBN_SELCHANGE(IDC_COMBO2,        OnInstrChanged)
END_MESSAGE_MAP()


void CPageEditNote::UpdateDialog()
//--------------------------------
{
    char s[64];
    CComboBox *combo;
    module_renderer *pSndFile;

    if ((!m_bInitialized) || (!m_pModDoc)) return;
    pSndFile = m_pModDoc->GetSoundFile();
    // Note
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
            combo->ResetContent();
            combo->SetItemData(combo->AddString("No note"), 0);
            AppendNotesToControlEx(*combo, pSndFile, m_nInstr);

            if (note_is_valid(m_nNote))
            {
                    // Normal note / no note
                    const modplug::tracker::note_t noteStart = (pSndFile != nullptr) ? pSndFile->GetModSpecifications().noteMin : 1;
                    combo->SetCurSel(m_nNote - (noteStart - 1));
            }
            else
            {
                    // Special notes
                    for(int i = combo->GetCount() - 1; i >= 0; --i)
                    {
                            if(combo->GetItemData(i) == m_nNote)
                            {
                                    combo->SetCurSel(i);
                                    break;
                            }
                    }
            }

    }
    // Instrument
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO2)) != NULL)
    {
            combo->ResetContent();

            if(modplug::tracker::modevent_t::IsPcNote(m_nNote))
            {
                    // control plugin param note
                    combo->SetItemData(combo->AddString("No Effect"), 0);
                    AddPluginNamesToCombobox(*combo, pSndFile->m_MixPlugins, false);
            } else
            {
                    // instrument / sample
                    combo->SetItemData(combo->AddString("No Instrument"), 0);
                    const UINT nmax = pSndFile->GetNumInstruments() ? pSndFile->GetNumInstruments() : pSndFile->GetNumSamples();
                    for (UINT i = 1; i <= nmax; i++)
                    {
                            wsprintf(s, "%02d: ", i);
                            int k = strlen(s);
                            // instrument / sample
                            if (pSndFile->GetNumInstruments())
                            {
                                    if (pSndFile->Instruments[i])
                                            memcpy(s+k, pSndFile->Instruments[i]->name, 32);
                            } else
                                    memcpy(s+k, pSndFile->m_szNames[i], MAX_SAMPLENAME);
                            s[k+32] = 0;
                            combo->SetItemData(combo->AddString(s), i);
                    }
            }
            combo->SetCurSel(m_nInstr);
    }
}


void CPageEditNote::OnNoteChanged()
//---------------------------------
{
    const bool bWasParamControl = modplug::tracker::modevent_t::IsPcNote(m_nNote);

    CComboBox *combo;
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
            int n = combo->GetCurSel();
            if (n >= 0) m_nNote = combo->GetItemData(n);
    }
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO2)) != NULL)
    {
            int n = combo->GetCurSel();
            if(n >= 0)
            {
                    const UINT oldInstr = m_nInstr;
                    module_renderer* pSndFile = m_pModDoc->GetSoundFile();
                    m_nInstr = combo->GetItemData(n);
                    //Checking whether note names should be recreated.
                    if(!modplug::tracker::modevent_t::IsPcNote(m_nNote) && pSndFile && pSndFile->Instruments[m_nInstr] && pSndFile->Instruments[oldInstr])
                    {
                            if(pSndFile->Instruments[m_nInstr]->pTuning != pSndFile->Instruments[oldInstr]->pTuning)
                                    UpdateDialog();
                    }
            }
    }
    const bool bIsNowParamControl = modplug::tracker::modevent_t::IsPcNote(m_nNote);
    if(bWasParamControl != bIsNowParamControl)
            UpdateDialog();

    if (m_pParent) m_pParent->UpdateNote(m_nNote, m_nInstr);
}


void CPageEditNote::OnInstrChanged()
//----------------------------------
{
    OnNoteChanged();
}


//////////////////////////////////////////////////////////////////////////////////////
// CPageEditVolume

BEGIN_MESSAGE_MAP(CPageEditVolume, CPageEditCommand)
    ON_WM_HSCROLL()
    ON_CBN_SELCHANGE(IDC_COMBO1,        OnVolCmdChanged)
END_MESSAGE_MAP()


void CPageEditVolume::UpdateDialog()
//----------------------------------
{
    CComboBox *combo;

    if ((!m_bInitialized) || (!m_pModDoc)) return;
    UpdateRanges();
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
            module_renderer *pSndFile = m_pModDoc->GetSoundFile();
            if (pSndFile->m_nType == MOD_TYPE_MOD || m_bIsParamControl)
            {
                    combo->EnableWindow(FALSE);
                    return;
            }
            combo->EnableWindow(TRUE);
            combo->ResetContent();
            UINT count = m_pModDoc->GetNumVolCmds();
            combo->SetItemData(combo->AddString(" None"), (uint32_t)-1);
            combo->SetCurSel(0);
            UINT fxndx = m_pModDoc->GetIndexFromVolCmd(m_nVolCmd);
            for (UINT i=0; i<count; i++)
            {
                    CHAR s[64];
                    if (m_pModDoc->GetVolCmdInfo(i, s))
                    {
                            int k = combo->AddString(s);
                            combo->SetItemData(k, i);
                            if (i == fxndx) combo->SetCurSel(k);
                    }
            }
    }
}


void CPageEditVolume::UpdateRanges()
//----------------------------------
{
    CSliderCtrl *slider = (CSliderCtrl *)GetDlgItem(IDC_SLIDER1);
    if ((slider) && (m_pModDoc))
    {
            uint32_t rangeMin = 0, rangeMax = 0;
            LONG fxndx = m_pModDoc->GetIndexFromVolCmd(m_nVolCmd);
            BOOL bOk = m_pModDoc->GetVolCmdInfo(fxndx, NULL, &rangeMin, &rangeMax);
            if ((bOk) && (rangeMax > rangeMin))
            {
                    slider->EnableWindow(TRUE);
                    slider->SetRange(rangeMin, rangeMax);
                    UINT pos = m_nVolume;
                    if (pos < rangeMin) pos = rangeMin;
                    if (pos > rangeMax) pos = rangeMax;
                    slider->SetPos(pos);
            } else
            {
                    slider->EnableWindow(FALSE);
            }
    }
}


void CPageEditVolume::OnVolCmdChanged()
//-------------------------------------
{
    CComboBox *combo;
    CSliderCtrl *slider;
    if (((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL) && (m_pModDoc))
    {
            int n = combo->GetCurSel();
            if (n >= 0)
            {
                    UINT volcmd = m_pModDoc->GetVolCmdFromIndex(combo->GetItemData(n));
                    if (volcmd != m_nVolCmd)
                    {
                            m_nVolCmd = volcmd;
                            UpdateRanges();
                    }
            }
    }
    if ((slider = (CSliderCtrl *)GetDlgItem(IDC_SLIDER1)) != NULL)
    {
            m_nVolume = slider->GetPos();
    }
    if (m_pParent) m_pParent->UpdateVolume(m_nVolCmd, m_nVolume);
}


void CPageEditVolume::OnHScroll(UINT, UINT, CScrollBar *)
//-------------------------------------------------------
{
    OnVolCmdChanged();
}


//////////////////////////////////////////////////////////////////////////////////////
// CPageEditEffect

BEGIN_MESSAGE_MAP(CPageEditEffect, CPageEditCommand)
    ON_WM_HSCROLL()
    ON_CBN_SELCHANGE(IDC_COMBO1,        OnCommandChanged)
END_MESSAGE_MAP()


void CPageEditEffect::UpdateDialog()
//----------------------------------
{
    CHAR s[128];
    CComboBox *combo;
    module_renderer *pSndFile;

    if ((!m_pModDoc) || (!m_bInitialized)) return;
    pSndFile = m_pModDoc->GetSoundFile();
    if ((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL)
    {
            combo->ResetContent();
            if(m_bIsParamControl)
            {
                    // plugin param control note
                    AddPluginParameternamesToCombobox(*combo, pSndFile->m_MixPlugins[m_nPlugin]);
                    combo->SetCurSel(m_nPluginParam);
            } else
            {
                    // process as effect
                    UINT numfx = m_pModDoc->GetNumEffects();
                    UINT fxndx = m_pModDoc->GetIndexFromEffect(m_nCommand, m_nParam);
                    combo->SetItemData(combo->AddString(" None"), (uint32_t)-1);
                    if (!m_nCommand) combo->SetCurSel(0);
                    for (UINT i=0; i<numfx; i++)
                    {
                            if (m_pModDoc->GetEffectInfo(i, s, true))
                            {
                                    int k = combo->AddString(s);
                                    combo->SetItemData(k, i);
                                    if (i == fxndx) combo->SetCurSel(k);
                            }
                    }
            }
    }
    UpdateRange(FALSE);
}


void CPageEditEffect::UpdateRange(BOOL bSet)
//------------------------------------------
{
    CSliderCtrl *slider = (CSliderCtrl *)GetDlgItem(IDC_SLIDER1);
    if ((slider) && (m_pModDoc))
    {
            uint32_t rangeMin = 0, rangeMax = 0;
            LONG fxndx = m_pModDoc->GetIndexFromEffect(m_nCommand, m_nParam);
            bool bEnable = ((fxndx >= 0) && (m_pModDoc->GetEffectInfo(fxndx, NULL, false, &rangeMin, &rangeMax)));
            if (bEnable)
            {
                    slider->EnableWindow(TRUE);
                    slider->SetPageSize(1);
                    slider->SetRange(rangeMin, rangeMax);
                    uint32_t pos = m_pModDoc->MapValueToPos(fxndx, m_nParam);
                    if (pos > rangeMax) pos = rangeMin | (pos & 0x0F);
                    if (pos < rangeMin) pos = rangeMin;
                    if (pos > rangeMax) pos = rangeMax;
                    slider->SetPos(pos);
            } else
            {
                    slider->SetRange(0,0);
                    slider->EnableWindow(FALSE);
            }
            UpdateValue(bSet);
    }
}


void CPageEditEffect::UpdateValue(BOOL bSet)
//------------------------------------------
{
    if (m_pModDoc)
    {
            CHAR s[128] = "";
            LONG fxndx = m_pModDoc->GetIndexFromEffect(m_nCommand, m_nParam);
            // -> CODE#0010
            // -> DESC="add extended parameter mechanism to pattern effects"
            //                if (fxndx >= 0) m_pModDoc->GetEffectNameEx(s, fxndx, m_nParam);
            if (fxndx >= 0) m_pModDoc->GetEffectNameEx(s, fxndx, m_nParam * m_nMultiplier + m_nXParam);
            // -! NEW_FEATURE#0010
            SetDlgItemText(IDC_TEXT1, s);
    }
    if ((m_pParent) && (bSet)) m_pParent->UpdateEffect(m_nCommand, m_nParam);
}


void CPageEditEffect::OnCommandChanged()
//--------------------------------------
{
    CComboBox *combo;

    if (((combo = (CComboBox *)GetDlgItem(IDC_COMBO1)) != NULL) && (m_pModDoc))
    {
            BOOL bSet = FALSE;
            int n = combo->GetCurSel();
            if (n >= 0)
            {
                    int param = -1, ndx = combo->GetItemData(n);
                    m_nCommand = (ndx >= 0) ? m_pModDoc->GetEffectFromIndex(ndx, param) : 0;
                    if (param >= 0) m_nParam = param;
                    bSet = TRUE;
            }
            UpdateRange(bSet);
    }
}



void CPageEditEffect::OnHScroll(UINT, UINT, CScrollBar *)
//-------------------------------------------------------
{
    CSliderCtrl *slider = (CSliderCtrl *)GetDlgItem(IDC_SLIDER1);
    if ((slider) && (m_pModDoc))
    {
            LONG fxndx = m_pModDoc->GetIndexFromEffect(m_nCommand, m_nParam);
            if (fxndx >= 0)
            {
                    int pos = slider->GetPos();
                    UINT param = m_pModDoc->MapPosToValue(fxndx, pos);
                    if (param != m_nParam)
                    {
                            m_nParam = param;
                            UpdateValue(TRUE);
                    }
            }
    }
}




////////////////////////////////////////////////////////////////////////////////////////////
// Keyboard Split Settings (pattern editor)

BEGIN_MESSAGE_MAP(CSplitKeyboadSettings, CDialog)
    ON_CBN_SELCHANGE(IDC_COMBO_OCTAVEMODIFIER,        OnOctaveModifierChanged)
END_MESSAGE_MAP()


void CSplitKeyboadSettings::DoDataExchange(CDataExchange* pDX)
//------------------------------------------------------------
{
    CDialog::DoDataExchange(pDX);
    //{{AFX_DATA_MAP(CSplitKeyboadSettings)
    DDX_Control(pDX, IDC_COMBO_SPLITINSTRUMENT,        m_CbnSplitInstrument);
    DDX_Control(pDX, IDC_COMBO_SPLITNOTE,                m_CbnSplitNote);
    DDX_Control(pDX, IDC_COMBO_OCTAVEMODIFIER,        m_CbnOctaveModifier);
    DDX_Control(pDX, IDC_COMBO_SPLITVOLUME,                m_CbnSplitVolume);
    //}}AFX_DATA_MAP
}


BOOL CSplitKeyboadSettings::OnInitDialog()
//----------------------------------------
{
    if(!m_pSndFile || m_pSndFile->GetpModDoc() == nullptr) return FALSE;

    CDialog::OnInitDialog();

    CHAR s[64];

    // Split Notes
    AppendNotesToControl(m_CbnSplitNote, 0, NoteMax - 1);
    m_CbnSplitNote.SetCurSel(m_pOptions->splitNote);

    // Octave modifier
    for(int i = -SPLIT_OCTAVE_RANGE; i < SPLIT_OCTAVE_RANGE + 1; i++){
            wsprintf(s,i < 0 ? "Octave -%d" : i > 0 ? "Octave +%d" : "No Change", abs(i));
            int n = m_CbnOctaveModifier.AddString(s);
            m_CbnOctaveModifier.SetItemData(n, i);
    }

    m_CbnOctaveModifier.SetCurSel(m_pOptions->octaveModifier + SPLIT_OCTAVE_RANGE);
    CheckDlgButton(IDC_PATTERN_OCTAVELINK, (m_pOptions->octaveLink && m_pOptions->octaveModifier != 0) ? MF_CHECKED : MF_UNCHECKED);

    // Volume
    m_CbnSplitVolume.AddString("No Change");
    m_CbnSplitVolume.SetItemData(0, 0);
    for(int i = 1; i <= 64 ; i++){
            wsprintf(s,"%d",i);
            int n = m_CbnSplitVolume.AddString(s);
            m_CbnSplitVolume.SetItemData(n, i);
    }
    m_CbnSplitVolume.SetCurSel(m_pOptions->splitVolume);

    // Instruments
    m_CbnSplitInstrument.ResetContent();
    m_CbnSplitInstrument.SetItemData(m_CbnSplitInstrument.AddString("No Instrument"), 0);

    if (m_pSndFile->m_nInstruments)        {
            for (modplug::tracker::instrumentindex_t nIns = 1; nIns <= m_pSndFile->m_nInstruments; nIns++)
            {
                    if (m_pSndFile->Instruments[nIns] == nullptr)
                            continue;

                    CString displayName = m_pSndFile->GetpModDoc()->GetPatternViewInstrumentName(nIns);
                    int n = m_CbnSplitInstrument.AddString(displayName);
                    m_CbnSplitInstrument.SetItemData(n, nIns);
            }
    } else
    {
            for (modplug::tracker::sampleindex_t nSmp = 1; nSmp <= m_pSndFile->m_nSamples; nSmp++)
            {
                    if ((m_pSndFile->m_szNames[nSmp][0]) || (m_pSndFile->Samples[nSmp].sample_data))
                    {
                            wsprintf(s, "%02d: %s", nSmp, m_pSndFile->m_szNames[nSmp]);
                            int n = m_CbnSplitInstrument.AddString(s);
                            m_CbnSplitInstrument.SetItemData(n, nSmp);
                    }
            }
    }
    m_CbnSplitInstrument.SetCurSel(m_pOptions->splitInstrument);

    return TRUE;
}


void CSplitKeyboadSettings::OnOK()
//--------------------------------
{
    CDialog::OnOK();

    m_pOptions->splitNote = m_CbnSplitNote.GetCurSel();
    m_pOptions->octaveModifier = m_CbnOctaveModifier.GetCurSel() - SPLIT_OCTAVE_RANGE;
    m_pOptions->octaveLink = (IsDlgButtonChecked(IDC_PATTERN_OCTAVELINK) == TRUE) ? true : false;
    m_pOptions->splitVolume = m_CbnSplitVolume.GetCurSel();
    m_pOptions->splitInstrument = m_CbnSplitInstrument.GetItemData(m_CbnSplitInstrument.GetCurSel());
}


void CSplitKeyboadSettings::OnCancel()
//------------------------------------
{
    CDialog::OnCancel();
}


void CSplitKeyboadSettings::OnOctaveModifierChanged()
//---------------------------------------------------
{
    CheckDlgButton(IDC_PATTERN_OCTAVELINK, (m_CbnOctaveModifier.GetCurSel() != 9) ? MF_CHECKED : MF_UNCHECKED);
}


///////////////////////////////////////////////////////////
// Rename a channel from pattern editor

BOOL CChannelRenameDlg::OnInitDialog()
//------------------------------------
{
    CDialog::OnInitDialog();

    CHAR s[32];
    wsprintf(s, "Set name for channel %d:", m_nChannel);
    SetDlgItemText(IDC_STATIC_CHANNEL_NAME, s);
    SetDlgItemText(IDC_EDIT_CHANNEL_NAME, m_sName);
    ((CEdit*)(GetDlgItem(IDC_EDIT_CHANNEL_NAME)))->LimitText(MAX_CHANNELNAME - 1);

    return TRUE;
}


void CChannelRenameDlg::OnOK()
//----------------------------
{
    CHAR sNewName[MAX_CHANNELNAME];
    GetDlgItemText(IDC_EDIT_CHANNEL_NAME, sNewName, MAX_CHANNELNAME);
    if(!strcmp(sNewName, m_sName))
    {
            bChanged = false;
            CDialog::OnCancel();
    } else
    {
            strcpy(m_sName, sNewName);
            bChanged = true;
            CDialog::OnOK();
    }
}
