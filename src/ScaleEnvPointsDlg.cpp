/*
 * ScaleEnvPointsDlg.cpp
 * ---------------------
 * Purpose: Scale instrument envelope points on x and y axis.
 * Notes  : (currently none)
 * Authors: OpenMPT Devs
 */

#include "stdafx.h"
#include "resource.h"
#include "misc_util.h"
#include "ScaleEnvPointsDlg.h"

float CScaleEnvPointsDlg::m_fFactorX = 1;
float CScaleEnvPointsDlg::m_fFactorY = 1;


BOOL CScaleEnvPointsDlg::OnInitDialog()
//-------------------------------------
{
    CDialog::OnInitDialog();

    char buffer[10];
    _snprintf(buffer, 9, "%g", m_fFactorX);
    SetDlgItemText(IDC_EDIT_FACTORX, buffer);
    _snprintf(buffer, 9, "%g", m_fFactorY);
    SetDlgItemText(IDC_EDIT_FACTORY, buffer);
    GetDlgItem(IDC_EDIT_FACTORX)->SetFocus();

    return FALSE;        // return TRUE unless you set the focus to a control
}


void CScaleEnvPointsDlg::OnOK()
//-----------------------------
{
    char buffer[10];
    GetDlgItemText(IDC_EDIT_FACTORX, buffer, 9);
    m_fFactorX = ConvertStrTo<float>(buffer);

    GetDlgItemText(IDC_EDIT_FACTORY, buffer, 9);
    m_fFactorY = ConvertStrTo<float>(buffer);

    if(m_fFactorX > 0 && m_fFactorX != 1)
    {
            for(UINT i = 0; i < m_pEnv->num_nodes; i++)
            {
                    m_pEnv->Ticks[i] = static_cast<uint16_t>(m_fFactorX * m_pEnv->Ticks[i]);

                    // Checking that the order of points is preserved.
                    if(i > 0 && m_pEnv->Ticks[i] <= m_pEnv->Ticks[i - 1])
                            m_pEnv->Ticks[i] = m_pEnv->Ticks[i - 1] + 1;
            }
    }

    if(m_fFactorY != 1)
    {
            for(UINT i = 0; i < m_pEnv->num_nodes; i++)
            {
                    m_pEnv->Values[i] = CLAMP(static_cast<uint8_t>((m_fFactorY * ((int)m_pEnv->Values[i] - m_nCenter)) + m_nCenter), ENVELOPE_MIN, ENVELOPE_MAX);
            }
    }

    CDialog::OnOK();
}
