#ifndef _VIEW_MODTREE_H_
#define _VIEW_MODTREE_H_

class CModDoc;
class CModTree;

#include <vector>

using std::vector;

enum {
    MODITEM_NULL=0,
    MODITEM_ORDER,
    MODITEM_PATTERN,
    MODITEM_SAMPLE,
    MODITEM_INSTRUMENT,
    MODITEM_COMMENTS,
    MODITEM_EFFECT,
    MODITEM_HDR_SONG,
    MODITEM_HDR_ORDERS,
    MODITEM_HDR_PATTERNS,
    MODITEM_HDR_SAMPLES,
    MODITEM_HDR_INSTRUMENTS,
    MODITEM_HDR_INSTRUMENTLIB,
    MODITEM_HDR_MIDILIB,
    MODITEM_HDR_MIDIGROUP,
    MODITEM_MIDIINSTRUMENT,
    MODITEM_MIDIPERCUSSION,
    MODITEM_INSLIB_FOLDER,
    MODITEM_INSLIB_SAMPLE,
    MODITEM_INSLIB_INSTRUMENT,
    MODITEM_INSLIB_SONG,
    MODITEM_DLSBANK_FOLDER,
    MODITEM_DLSBANK_INSTRUMENT,
    MODITEM_SEQUENCE,
};

#define TREESTATUS_RDRAG                    0x01
#define TREESTATUS_LDRAG                    0x02
#define TREESTATUS_SINGLEEXPAND            0x04
#define TREESTATUS_DRAGGING                    (TREESTATUS_RDRAG|TREESTATUS_LDRAG)

#define MODTREE_MAX_DOCUMENTS            32

struct MODTREEDOCINFO
{
    CModDoc *pModDoc;
    // Module information
    modplug::tracker::sequenceindex_t nSeqSel;
    modplug::tracker::orderindex_t nOrdSel;
    // Tree state variables
    HTREEITEM hSong, hPatterns, hSamples, hInstruments, hComments, hOrders, hEffects;
    vector<HTREEITEM> tiPatterns;
    HTREEITEM tiSamples[MAX_SAMPLES];
    HTREEITEM tiInstruments[MAX_INSTRUMENTS];
    vector<vector<HTREEITEM> > tiOrders;
    vector<HTREEITEM> tiSequences;
    HTREEITEM tiEffects[MAX_MIXPLUGINS];
    bool bIsSamplePlaying[MAX_SAMPLES];
    bool bIsInstrPlaying[MAX_INSTRUMENTS];

    MODTREEDOCINFO(const module_renderer* const pSndFile) : nOrdSel(0)
    {
            pModDoc = NULL;
            nSeqSel = modplug::tracker::SequenceIndexInvalid;
            nOrdSel = modplug::tracker::OrderIndexInvalid;
            hSong = hPatterns = hSamples = hInstruments = hComments = hOrders = hEffects = NULL;
            if(pSndFile != NULL)
            {
                    tiPatterns.resize(pSndFile->Patterns.Size(), NULL);
                    tiOrders.resize(pSndFile->Order.GetNumSequences());
                    for(modplug::tracker::sequenceindex_t i = 0; i < tiOrders.size(); i++)
                    {
                            //tiOrders[i].resize(pSndFile->Order.GetSequence(i)->GetLength(), NULL);
                    }
                    tiSequences.resize(pSndFile->Order.GetNumSequences(), NULL);
            }
            memset(tiSamples, 0, sizeof(tiSamples));
            memset(tiInstruments, 0, sizeof(tiInstruments));
            memset(tiEffects, 0, sizeof(tiEffects));
            memset(bIsSamplePlaying, false, MAX_SAMPLES * sizeof(bool));
            memset(bIsInstrPlaying, false, MAX_INSTRUMENTS * sizeof(bool));
    }
};

typedef MODTREEDOCINFO _MODTREEDOCINFO;
typedef MODTREEDOCINFO* PMODTREEDOCINFO;


//=============================================
class CModTreeDropTarget: public COleDropTarget
//=============================================
{
protected:
    CModTree *m_pModTree;

public:
    CModTreeDropTarget() { m_pModTree = NULL; }
    BOOL Register(CModTree *pWnd);

public:
    virtual DROPEFFECT OnDragEnter(CWnd *pWnd, COleDataObject* pDataObject, uint32_t dwKeyState, CPoint point);
    virtual DROPEFFECT OnDragOver(CWnd *pWnd, COleDataObject* pDataObject, uint32_t dwKeyState, CPoint point);
    virtual BOOL OnDrop(CWnd *pWnd, COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);
};


//==============================
class CModTree: public CTreeCtrl
//==============================
{
protected:
    module_renderer m_SongFile;
    CModTreeDropTarget m_DropTarget;
    CModTree *m_pDataTree;
    uint32_t m_dwStatus;
    HWND m_hDropWnd;
    uint64_t m_qwItemDrag;
    BOOL m_bShowAllFiles;
    UINT m_nDocNdx, m_nDragDocNdx;
    HTREEITEM m_hItemDrag, m_hItemDrop;
    HTREEITEM m_hInsLib, m_hMidiLib;
    HTREEITEM m_tiMidiGrp[17];
    HTREEITEM m_tiDLS[MAX_DLS_BANKS];
    HTREEITEM m_tiMidi[128];
    HTREEITEM m_tiPerc[128];
    PMODTREEDOCINFO DocInfo[MODTREE_MAX_DOCUMENTS];
    CHAR m_szInstrLibPath[_MAX_PATH], m_szOldPath[_MAX_PATH], m_szSongName[_MAX_PATH];

public:
    CModTree(CModTree *pDataTree=NULL);
    virtual ~CModTree();

// Attributes
public:
    VOID Init();
    VOID InsLibSetFullPath(LPCSTR pszLibPath, LPCSTR pszSongFolder);
    VOID InsLibGetFullPath(HTREEITEM hItem, LPSTR pszFullPath) const;
    VOID RefreshMidiLibrary();
    VOID RefreshDlsBanks();
    VOID RefreshInstrumentLibrary();
    VOID EmptyInstrumentLibrary();
    VOID FillInstrumentLibrary();
    uint64_t GetModItem(HTREEITEM hItem);
    inline uint32_t GetModItemType(const uint64_t modItem) {return static_cast<uint32_t>(modItem & 0xFFFF);};        // return "item type" part of mod item variable ( & 0xFFFF )
    inline uint32_t GetModItemID(const uint64_t modItem) {return static_cast<uint32_t>(modItem >> 16);};                // return "item ID" part of mod item variable ( >> 16 )
    BOOL SetMidiInstrument(UINT nIns, LPCTSTR lpszFileName);
    BOOL SetMidiPercussion(UINT nPerc, LPCTSTR lpszFileName);
    BOOL ExecuteItem(HTREEITEM hItem);
    BOOL DeleteTreeItem(HTREEITEM hItem);
    BOOL PlayItem(HTREEITEM hItem, UINT nParam=0);
    BOOL OpenTreeItem(HTREEITEM hItem);
    BOOL OpenMidiInstrument(uint32_t dwItem);
    BOOL InstrumentLibraryChDir(LPCSTR lpszDir);
    BOOL GetDropInfo(LPDRAGONDROP pdropinfo, LPSTR lpszPath);
    VOID OnOptionsChanged();
    VOID AddDocument(CModDoc *pModDoc);
    VOID RemoveDocument(CModDoc *pModDoc);
    VOID UpdateView(UINT nDocNdx, uint32_t dwHint);
    VOID OnUpdate(CModDoc *pModDoc, uint32_t dwHint, CObject *pHint);
    bool CanDrop(HTREEITEM hItem, bool bDoDrop);
    VOID UpdatePlayPos(CModDoc *pModDoc, PMPTNOTIFICATION pNotify);
    bool IsItemExpanded(HTREEITEM hItem);

// Drag & Drop operations
public:
    DROPEFFECT OnDragEnter(COleDataObject* pDataObject, uint32_t dwKeyState, CPoint point);
    DROPEFFECT OnDragOver(COleDataObject* pDataObject, uint32_t dwKeyState, CPoint point);
    BOOL OnDrop(COleDataObject* pDataObject, DROPEFFECT dropEffect, CPoint point);

protected:
    static int CALLBACK ModTreeInsLibCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
    static int CALLBACK ModTreeDrumCompareProc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort);
    void ModTreeBuildTVIParam(TV_INSERTSTRUCT &tvis, LPCSTR lpszName, int iImage);
    CModDoc *GetDocumentFromItem(HTREEITEM hItem);
    UINT GetDocumentIDFromModDoc(CModDoc *pModDoc);

// Generated message map functions
protected:
    //{{AFX_MSG(CModTree)
    afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
    afx_msg void OnMouseMove(UINT nFlags, CPoint point);
    afx_msg void OnBeginDrag(HTREEITEM, bool bLeft, LRESULT *pResult);
    afx_msg void OnBeginLDrag(LPNMHDR, LRESULT *pResult);
    afx_msg void OnBeginRDrag(LPNMHDR, LRESULT *pResult);
    afx_msg void OnEndDrag(uint32_t dwMask);
    afx_msg void OnItemDblClk(LPNMHDR phdr, LRESULT *pResult);
    afx_msg void OnItemReturn(LPNMHDR, LRESULT *pResult);
    afx_msg void OnItemLeftClick(LPNMHDR pNMHDR, LRESULT *pResult);
    afx_msg void OnItemRightClick(LPNMHDR, LRESULT *pResult);
    afx_msg void OnItemExpanded(LPNMHDR pnmhdr, LRESULT *pResult);
    afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
    afx_msg void OnRefreshTree();
    afx_msg void OnExecuteItem();
    afx_msg void OnPlayTreeItem();
    afx_msg void OnDeleteTreeItem();
    afx_msg void OnOpenTreeItem();
    afx_msg void OnMuteTreeItem();
    afx_msg void OnSoloTreeItem();
    afx_msg void OnUnmuteAllTreeItem();
    afx_msg void OnDuplicateTreeItem();
    afx_msg void OnInsertTreeItem();
    afx_msg void OnSwitchToTreeItem();        // hack for sequence items to avoid double-click action
    afx_msg void OnCloseItem();

// -> CODE#0023
// -> DESC="IT project files (.itp)"
    afx_msg void OnSetItemPath();
    afx_msg void OnSaveItem();
// -! NEW_FEATURE#0023

    afx_msg void OnAddDlsBank();
    afx_msg void OnImportMidiLib();
    afx_msg void OnExportMidiLib();
    afx_msg void OnSoundBankProperties();
    afx_msg void OnRefreshInstrLib();
    afx_msg void OnShowAllFiles();
    afx_msg void OnShowSoundFiles();
    //}}AFX_MSG
    DECLARE_MESSAGE_MAP()
public:
    afx_msg void OnKillFocus(CWnd* pNewWnd);        //rewbs.customKeys
    afx_msg void OnSetFocus(CWnd* pOldWnd);                //rewbs.customKeys
};


#endif
