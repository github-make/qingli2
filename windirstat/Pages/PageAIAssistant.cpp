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

#include "pch.h"
#include "PageAIAssistant.h"
#include "WinHttpClient.h"

IMPLEMENT_DYNAMIC(CPageAIAssistant, CMFCPropertyPage)

CPageAIAssistant::CPageAIAssistant() : CMFCPropertyPage(IDD) {}

void CPageAIAssistant::DoDataExchange(CDataExchange* pDX)
{
    CMFCPropertyPage::DoDataExchange(pDX);
    DDX_Text(pDX, IDC_AI_API_URL, m_apiUrl);
    DDX_Text(pDX, IDC_AI_API_KEY, m_apiKey);
    DDX_Text(pDX, IDC_AI_MODEL, m_model);
    DDX_Text(pDX, IDC_AI_CUSTOM_RULES, m_customRules);
    DDX_Text(pDX, IDC_AI_BATCH_SIZE, m_batchSize);
}

BEGIN_MESSAGE_MAP(CPageAIAssistant, CMFCPropertyPage)
    ON_EN_CHANGE(IDC_AI_API_URL, OnBnClickedSetModified)
    ON_EN_CHANGE(IDC_AI_API_KEY, OnBnClickedSetModified)
    ON_EN_CHANGE(IDC_AI_MODEL, OnBnClickedSetModified)
    ON_EN_CHANGE(IDC_AI_CUSTOM_RULES, OnBnClickedSetModified)
    ON_EN_CHANGE(IDC_AI_BATCH_SIZE, OnBnClickedSetModified)
    ON_BN_CLICKED(IDC_AI_TEST_CONNECTION, OnBnClickedTestConnection)
    ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

HBRUSH CPageAIAssistant::OnCtlColor(CDC* pDC, CWnd* pWnd, UINT nCtlColor)
{
    const HBRUSH brush = DarkMode::OnCtlColor(pDC, nCtlColor);
    return brush ? brush : CMFCPropertyPage::OnCtlColor(pDC, pWnd, nCtlColor);
}

BOOL CPageAIAssistant::OnInitDialog()
{
    CMFCPropertyPage::OnInitDialog();

    DarkMode::AdjustControls(GetSafeHwnd());

    m_apiUrl = COptions::AIApiUrl.Obj().c_str();
    m_apiKey = COptions::AIApiKey.Obj().c_str();
    m_model = COptions::AIModel.Obj().c_str();
    m_customRules = COptions::AICustomRules.Obj().c_str();
    m_batchSize = COptions::AIBatchSize;

    UpdateData(FALSE);
    return TRUE;
}

void CPageAIAssistant::OnOK()
{
    UpdateData();

    COptions::AIApiUrl = std::wstring(m_apiUrl);
    COptions::AIApiKey = std::wstring(m_apiKey);
    COptions::AIModel = std::wstring(m_model);
    COptions::AICustomRules = std::wstring(m_customRules);
    COptions::AIBatchSize = m_batchSize;

    CMFCPropertyPage::OnOK();
}

void CPageAIAssistant::OnBnClickedSetModified()
{
    SetModified();
}

void CPageAIAssistant::OnBnClickedTestConnection()
{
    UpdateData();

    if (m_apiKey.IsEmpty() || m_apiUrl.IsEmpty() || m_model.IsEmpty())
    {
        AfxMessageBox(L"Please fill in API URL, API Key, and Model fields.", MB_ICONWARNING);
        return;
    }

    CWaitCursor wait;

    CWinHttpClient client;
    const std::string body = R"({"model":")" +
        WideToUtf8(std::wstring(m_model)) +
        R"(","messages":[{"role":"user","content":"Hello"}],"max_tokens":5})";

    const auto response = client.Post(std::wstring(m_apiUrl), body, std::wstring(m_apiKey));

    if (response.success)
    {
        AfxMessageBox(L"Connection successful!", MB_ICONINFORMATION);
    }
    else
    {
        AfxMessageBox(std::format(L"Connection failed: {}", response.error).c_str(), MB_ICONERROR);
    }
}
