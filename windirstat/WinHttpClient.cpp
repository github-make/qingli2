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
#include "WinHttpClient.h"

#pragma comment(lib, "winhttp.lib")

CWinHttpClient::CWinHttpClient()
{
    m_hSession = WinHttpOpen(L"WinDirStat/2.0",
        WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);
    if (m_hSession != nullptr)
    {
        DWORD timeout = 30000;
        WinHttpSetOption(m_hSession, WINHTTP_OPTION_CONNECT_TIMEOUT, &timeout, sizeof(timeout));
        timeout = 60000;
        WinHttpSetOption(m_hSession, WINHTTP_OPTION_RECEIVE_TIMEOUT, &timeout, sizeof(timeout));
        WinHttpSetOption(m_hSession, WINHTTP_OPTION_SEND_TIMEOUT, &timeout, sizeof(timeout));
    }
}

CWinHttpClient::~CWinHttpClient()
{
    if (m_hSession != nullptr)
    {
        WinHttpCloseHandle(m_hSession);
    }
}

HttpResponse CWinHttpClient::Post(const std::wstring& url, const std::string& jsonBody, const std::wstring& apiKey)
{
    HttpResponse response;

    if (m_hSession == nullptr)
    {
        response.error = L"HTTP session not initialized";
        return response;
    }

    // Parse URL
    URL_COMPONENTS urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {};
    wchar_t urlPath[2048] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = _countof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = _countof(urlPath);

    if (!WinHttpCrackUrl(url.c_str(), static_cast<DWORD>(url.length()), 0, &urlComp))
    {
        response.error = L"Invalid URL: " + url;
        return response;
    }

    // Connect
    HINTERNET hConnect = WinHttpConnect(m_hSession, hostName, urlComp.nPort, 0);
    if (hConnect == nullptr)
    {
        response.error = L"Connection failed";
        return response;
    }

    // Open request
    const DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlPath,
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (hRequest == nullptr)
    {
        WinHttpCloseHandle(hConnect);
        response.error = L"Failed to create request";
        return response;
    }

    // Set headers
    const std::wstring headers = L"Content-Type: application/json\r\nAuthorization: Bearer " + apiKey;
    WinHttpAddRequestHeaders(hRequest, headers.c_str(),
        static_cast<DWORD>(headers.length()), WINHTTP_ADDREQ_FLAG_ADD);

    // Send request
    const BOOL sent = WinHttpSendRequest(hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        const_cast<char*>(jsonBody.data()),
        static_cast<DWORD>(jsonBody.size()),
        static_cast<DWORD>(jsonBody.size()), 0);

    if (!sent || !WinHttpReceiveResponse(hRequest, nullptr))
    {
        const DWORD err = GetLastError();
        response.error = L"Request failed with error: " + std::to_wstring(err);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        return response;
    }

    // Get status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest,
        WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize,
        WINHTTP_NO_HEADER_INDEX);
    response.statusCode = statusCode;

    // Read response body
    DWORD bytesAvailable = 0;
    while (WinHttpQueryDataAvailable(hRequest, &bytesAvailable) && bytesAvailable > 0)
    {
        std::vector<char> buffer(bytesAvailable);
        DWORD bytesRead = 0;
        if (WinHttpReadData(hRequest, buffer.data(), bytesAvailable, &bytesRead))
        {
            response.body.append(buffer.data(), bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);

    response.success = (statusCode >= 200 && statusCode < 300);
    if (!response.success)
    {
        response.error = L"HTTP " + std::to_wstring(statusCode);
    }

    return response;
}
