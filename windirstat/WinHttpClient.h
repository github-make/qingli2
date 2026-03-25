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
#include <winhttp.h>
#include <string>

struct HttpResponse
{
    DWORD statusCode = 0;
    std::string body;
    std::wstring error;
    bool success = false;
};

class CWinHttpClient
{
    HINTERNET m_hSession = nullptr;

public:
    CWinHttpClient();
    ~CWinHttpClient();

    CWinHttpClient(const CWinHttpClient&) = delete;
    CWinHttpClient& operator=(const CWinHttpClient&) = delete;

    // Synchronous POST (call from worker thread only)
    HttpResponse Post(const std::wstring& url, const std::string& jsonBody, const std::wstring& apiKey);
};
