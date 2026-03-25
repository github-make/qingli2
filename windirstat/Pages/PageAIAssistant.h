// WinDirStat - Directory Statistics
// Copyright (C) WinDirStat Team
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#pragma once

#include "pch.h"

class CPageAIAssistant final : public CMFCPropertyPage
{
    DECLARE_DYNAMIC(CPageAIAssistant)

    enum : std::uint8_t { IDD = IDD_PAGE_AI_ASSISTANT };

    CPageAIAssistant();
    ~CPageAIAssistant() override = default;

protected:
    void DoDataExchange(CDataExchange* pDX) override;
    BOOL OnInitDialog() override;
    void OnOK() override;

    CString m_apiUrl;
    CString m_apiKey;
    CString m_model;
    CString m_customRules;
    int m_batchSize = 10;

    DECLARE_MESSAGE_MAP()
    afx_msg void OnBnClickedSetModified();
    afx_msg void OnBnClickedTestConnection();
    afx_msg HBRUSH OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor);
};
