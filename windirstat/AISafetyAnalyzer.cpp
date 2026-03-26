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
#include "AISafetyAnalyzer.h"
#include "Options.h"
#include "MainFrame.h"

#include <sstream>
#include <algorithm>
#include <cctype>

CAISafetyAnalyzer* CAISafetyAnalyzer::s_singleton = nullptr;

CAISafetyAnalyzer::CAISafetyAnalyzer()
{
    s_singleton = this;
}

CAISafetyAnalyzer::~CAISafetyAnalyzer()
{
    Cancel();
    s_singleton = nullptr;
}

CAISafetyAnalyzer* CAISafetyAnalyzer::Get()
{
    if (s_singleton == nullptr)
    {
        s_singleton = new CAISafetyAnalyzer();
    }
    return s_singleton;
}

void CAISafetyAnalyzer::AnalyzeItems(const std::vector<CItem*>& items)
{
    // Cancel any running analysis
    Cancel();

    // Start new worker thread
    m_running = true;
    m_workerThread = std::jthread([this, items](std::stop_token token)
    {
        WorkerProc(items, token);
    });
}

void CAISafetyAnalyzer::Cancel()
{
    if (m_workerThread.joinable())
    {
        m_workerThread.request_stop();
        m_workerThread.join();
    }
    m_running = false;
}

void CAISafetyAnalyzer::WorkerProc(std::vector<CItem*> items, std::stop_token stopToken)
{
    try
    {
        WorkerProcInner(items, stopToken);
    }
    catch (...)
    {
        // Silently handle any exceptions to prevent process termination
    }
    m_running = false;

    if (m_completeCallback)
    {
        CMainFrame::Get()->InvokeInMessageThread([this]()
        {
            m_completeCallback();
        });
    }
}

void CAISafetyAnalyzer::WorkerProcInner(std::vector<CItem*>& items, std::stop_token& stopToken)
{
    // Step 1: Expand all folders into individual files
    std::vector<CItem*> allFiles;
    for (CItem* item : items)
    {
        if (stopToken.stop_requested()) break;
        ExpandFolders(item, allFiles);
    }

    const size_t totalFiles = allFiles.size();
    size_t completed = 0;

    // Notify progress
    if (m_progressCallback)
    {
        CMainFrame::Get()->InvokeInMessageThread([this, totalFiles]()
        {
            m_progressCallback(totalFiles, 0);
        });
    }

    // Step 2: Process files in batches
    const int batchSize = (std::max)(1, static_cast<int>(COptions::AIBatchSize));
    std::vector<FileAnalysisInfo> batch;
    batch.reserve(batchSize);

    for (size_t i = 0; i < allFiles.size(); ++i)
    {
        if (stopToken.stop_requested()) break;

        FileAnalysisInfo info = CollectFileInfo(allFiles[i]);

        // Check custom rules first
        auto ruleResult = CheckCustomRules(info);
        if (ruleResult.has_value())
        {
            // Rule matched - report immediately
            CItem* item = info.item;
            SAFETY_LEVEL level = ruleResult.value();
            std::wstring reason = L"Matched custom rule";
            if (m_resultCallback)
            {
                CMainFrame::Get()->InvokeInMessageThread([this, item, level, reason]()
                {
                    m_resultCallback(item, level, reason);
                });
            }
            completed++;
            if (m_progressCallback)
            {
                CMainFrame::Get()->InvokeInMessageThread([this, totalFiles, completed]()
                {
                    m_progressCallback(totalFiles, completed);
                });
            }
            continue;
        }

        batch.push_back(std::move(info));

        // Send batch when full or at end
        if (static_cast<int>(batch.size()) >= batchSize || i == allFiles.size() - 1)
        {
            if (stopToken.stop_requested()) break;

            // Build and send API request
            const std::string requestJson = BuildBatchRequest(batch);
            const std::wstring apiUrl = COptions::AIApiUrl.Obj();
            const std::wstring apiKey = COptions::AIApiKey.Obj();

            HttpResponse resp = m_httpClient.Post(apiUrl, requestJson, apiKey);

            if (resp.success)
            {
                ParseBatchResponse(resp.body, batch);
            }
            else
            {
                // Report error for all items in batch
                for (const auto& fileInfo : batch)
                {
                    CItem* item = fileInfo.item;
                    std::wstring error = resp.error;
                    if (m_resultCallback)
                    {
                        CMainFrame::Get()->InvokeInMessageThread([this, item, error]()
                        {
                            m_resultCallback(item, SL_ERROR, error);
                        });
                    }
                }
            }

            completed += batch.size();
            if (m_progressCallback)
            {
                CMainFrame::Get()->InvokeInMessageThread([this, totalFiles, completed]()
                {
                    m_progressCallback(totalFiles, completed);
                });
            }

            batch.clear();
        }
    }

    if (m_completeCallback)
    {
        CMainFrame::Get()->InvokeInMessageThread([this]()
        {
            m_completeCallback();
        });
    }
}

void CAISafetyAnalyzer::ExpandFolders(CItem* item, std::vector<CItem*>& result)
{
    if (item == nullptr) return;

    if (item->IsTypeOrFlag(IT_FILE))
    {
        result.push_back(item);
        return;
    }

    if (item->IsTypeOrFlag(IT_DIRECTORY) || item->IsTypeOrFlag(IT_DRIVE))
    {
        if (!item->IsLeaf() && !item->IsTypeOrFlag(IT_HLINKS))
        {
            for (const auto& child : item->GetChildren())
            {
                ExpandFolders(child, result);
            }
        }
    }
}

FileAnalysisInfo CAISafetyAnalyzer::CollectFileInfo(CItem* item)
{
    FileAnalysisInfo info;
    info.item = item;
    info.path = item->GetPath();
    info.name = item->GetName();
    info.sizePhysical = item->GetSizePhysical();
    info.lastChange = item->GetLastChange();
    info.attributes = item->GetAttributes();

    // Read content snippets for files
    if (item->IsTypeOrFlag(IT_FILE) && info.sizePhysical > 0)
    {
        constexpr DWORD snippetSize = 256;

        // Head
        info.contentHead = ReadFileSnippet(info.path, 0, snippetSize);

        // Middle
        if (info.sizePhysical > snippetSize * 2)
        {
            const ULONGLONG midOffset = info.sizePhysical / 2 - snippetSize / 2;
            info.contentMiddle = ReadFileSnippet(info.path, midOffset, snippetSize);
        }

        // Tail
        if (info.sizePhysical > snippetSize)
        {
            const ULONGLONG tailOffset = info.sizePhysical - snippetSize;
            info.contentTail = ReadFileSnippet(info.path, tailOffset, snippetSize);
        }
    }

    return info;
}

std::vector<BYTE> CAISafetyAnalyzer::ReadFileSnippet(const std::wstring& path, ULONGLONG offset, DWORD count)
{
    std::vector<BYTE> result;

    HANDLE hFile = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return result;

    LARGE_INTEGER li;
    li.QuadPart = static_cast<LONGLONG>(offset);
    if (!SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN))
    {
        CloseHandle(hFile);
        return result;
    }

    result.resize(count);
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, result.data(), count, &bytesRead, nullptr))
    {
        CloseHandle(hFile);
        return {};
    }

    result.resize(bytesRead);
    CloseHandle(hFile);
    return result;
}

std::optional<SAFETY_LEVEL> CAISafetyAnalyzer::CheckCustomRules(const FileAnalysisInfo& info)
{
    const std::wstring& rules = COptions::AICustomRules.Obj();
    if (rules.empty()) return std::nullopt;

    // Parse rules: each line is "pattern|LEVEL"
    std::wistringstream stream(rules);
    std::wstring line;
    while (std::getline(stream, line))
    {
        // Trim CR
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        if (line.empty()) continue;

        const auto sepPos = line.rfind(L'|');
        if (sepPos == std::wstring::npos) continue;

        const std::wstring pattern = line.substr(0, sepPos);
        const std::wstring levelStr = line.substr(sepPos + 1);

        // Simple wildcard match using PathMatchSpecW
        if (PathMatchSpecW(info.path.c_str(), pattern.c_str()))
        {
            if (_wcsicmp(levelStr.c_str(), L"SAFE") == 0) return SL_SAFE;
            if (_wcsicmp(levelStr.c_str(), L"LOW") == 0) return SL_LOW;
            if (_wcsicmp(levelStr.c_str(), L"MED") == 0) return SL_MEDIUM;
            if (_wcsicmp(levelStr.c_str(), L"HIGH") == 0) return SL_HIGH;
            if (_wcsicmp(levelStr.c_str(), L"DANGER") == 0) return SL_DANGER;
        }
    }

    return std::nullopt;
}

std::string CAISafetyAnalyzer::BytesToHex(const std::vector<BYTE>& data)
{
    static constexpr char hexChars[] = "0123456789ABCDEF";
    std::string result;
    result.reserve(data.size() * 2);
    for (BYTE b : data)
    {
        result += hexChars[b >> 4];
        result += hexChars[b & 0x0F];
    }
    return result;
}

// Escape a string for JSON
static std::string JsonEscape(const std::string& s)
{
    std::string result;
    result.reserve(s.size() + 16);
    for (char c : s)
    {
        switch (c)
        {
        case '"': result += "\\\""; break;
        case '\\': result += "\\\\"; break;
        case '\b': result += "\\b"; break;
        case '\f': result += "\\f"; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20)
            {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", c);
                result += buf;
            }
            else
            {
                result += c;
            }
        }
    }
    return result;
}

static std::string WideToUtf8(const std::wstring& wide)
{
    if (wide.empty()) return {};
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide.c_str(),
        static_cast<int>(wide.size()), result.data(), size, nullptr, nullptr);
    return result;
}

static std::wstring Utf8ToWide(const std::string& utf8)
{
    if (utf8.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(),
        static_cast<int>(utf8.size()), result.data(), size);
    return result;
}

static std::string AiFormatFileTime(const FILETIME& ft)
{
    SYSTEMTIME st;
    FileTimeToSystemTime(&ft, &st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

static std::string AiFormatAttributes(DWORD attrs)
{
    std::string result;
    if (attrs & FILE_ATTRIBUTE_READONLY) result += "R";
    if (attrs & FILE_ATTRIBUTE_HIDDEN) result += "H";
    if (attrs & FILE_ATTRIBUTE_SYSTEM) result += "S";
    if (attrs & FILE_ATTRIBUTE_ARCHIVE) result += "A";
    if (attrs & FILE_ATTRIBUTE_COMPRESSED) result += "C";
    if (attrs & FILE_ATTRIBUTE_ENCRYPTED) result += "E";
    if (attrs & FILE_ATTRIBUTE_TEMPORARY) result += "T";
    return result.empty() ? "-" : result;
}

std::string CAISafetyAnalyzer::BuildBatchRequest(const std::vector<FileAnalysisInfo>& batch)
{
    const std::string model = WideToUtf8(COptions::AIModel.Obj());

    // Build user message with file details
    std::ostringstream userMsg;
    userMsg << "Analyze these files for deletion safety:\\n";

    for (size_t i = 0; i < batch.size(); ++i)
    {
        const auto& f = batch[i];
        userMsg << "[" << i << "] Path: " << WideToUtf8(f.path)
            << " | Size: " << f.sizePhysical
            << " | Modified: " << AiFormatFileTime(f.lastChange)
            << " | Attrs: " << AiFormatAttributes(f.attributes);

        if (!f.contentHead.empty())
        {
            userMsg << " | Head(hex): " << BytesToHex(f.contentHead);
        }
        if (!f.contentMiddle.empty())
        {
            userMsg << " | Mid(hex): " << BytesToHex(f.contentMiddle);
        }
        if (!f.contentTail.empty())
        {
            userMsg << " | Tail(hex): " << BytesToHex(f.contentTail);
        }
        userMsg << "\\n";
    }

    // Build JSON request (manually to avoid JSON library dependency)
    std::ostringstream json;
    json << "{\"model\":\"" << JsonEscape(model) << "\","
        << "\"messages\":["
        << "{\"role\":\"system\",\"content\":\""
        << "You are a Windows file safety analyzer. For each file, determine if it can be safely deleted. "
        << "Consider: system files, application dependencies, user data, temporary/cache files, logs, etc. "
        << "Respond ONLY with a JSON array: [{\\\"index\\\":0,\\\"level\\\":\\\"SAFE|LOW|MED|HIGH|DANGER\\\",\\\"reason\\\":\\\"brief explanation in Chinese\\\"}]. "
        << "Levels: SAFE=can delete freely, LOW=minor risk(logs/cache), MED=moderate risk(config/preferences), HIGH=important(app data/dependencies), DANGER=critical system file. "
        << "Always respond in Chinese for the reason field."
        << "\"},"
        << "{\"role\":\"user\",\"content\":\"" << JsonEscape(userMsg.str()) << "\"}"
        << "],"
        << "\"temperature\":0.1,"
        << "\"max_tokens\":2048"
        << "}";

    return json.str();
}

SAFETY_LEVEL CAISafetyAnalyzer::ParseLevel(const std::string& levelStr)
{
    if (levelStr == "SAFE") return SL_SAFE;
    if (levelStr == "LOW") return SL_LOW;
    if (levelStr == "MED") return SL_MEDIUM;
    if (levelStr == "HIGH") return SL_HIGH;
    if (levelStr == "DANGER") return SL_DANGER;
    return SL_ERROR;
}

// Simple JSON extraction helpers (no library dependency)
static std::string ExtractJsonString(const std::string& json, const std::string& key, size_t startPos = 0)
{
    const std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey, startPos);
    if (pos == std::string::npos) return {};

    pos = json.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return {};

    // Skip whitespace
    pos = json.find('"', pos + 1);
    if (pos == std::string::npos) return {};
    pos++; // Skip opening quote

    std::string result;
    while (pos < json.size() && json[pos] != '"')
    {
        if (json[pos] == '\\' && pos + 1 < json.size())
        {
            pos++;
            switch (json[pos])
            {
            case '"': result += '"'; break;
            case '\\': result += '\\'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            default: result += json[pos]; break;
            }
        }
        else
        {
            result += json[pos];
        }
        pos++;
    }
    return result;
}

static int ExtractJsonInt(const std::string& json, const std::string& key, size_t startPos = 0)
{
    const std::string searchKey = "\"" + key + "\"";
    size_t pos = json.find(searchKey, startPos);
    if (pos == std::string::npos) return -1;

    pos = json.find(':', pos + searchKey.size());
    if (pos == std::string::npos) return -1;
    pos++;

    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

    return atoi(json.c_str() + pos);
}

void CAISafetyAnalyzer::ParseBatchResponse(const std::string& responseBody, const std::vector<FileAnalysisInfo>& batch)
{
    // Extract content from OpenAI response: {"choices":[{"message":{"content":"..."}}]}
    const std::string content = ExtractJsonString(responseBody, "content");
    if (content.empty())
    {
        // Report error for all items
        for (const auto& info : batch)
        {
            CItem* item = info.item;
            if (m_resultCallback)
            {
                CMainFrame::Get()->InvokeInMessageThread([this, item]()
                {
                    m_resultCallback(item, SL_ERROR, L"Failed to parse API response");
                });
            }
        }
        return;
    }

    // Parse the content JSON array: [{"index":0,"level":"SAFE","reason":"..."},...]
    // Find each object in the array
    size_t searchPos = 0;
    std::vector<bool> processed(batch.size(), false);

    while (searchPos < content.size())
    {
        const size_t objStart = content.find('{', searchPos);
        if (objStart == std::string::npos) break;

        const size_t objEnd = content.find('}', objStart);
        if (objEnd == std::string::npos) break;

        const std::string obj = content.substr(objStart, objEnd - objStart + 1);
        searchPos = objEnd + 1;

        const int index = ExtractJsonInt(obj, "index");
        const std::string level = ExtractJsonString(obj, "level");
        const std::string reason = ExtractJsonString(obj, "reason");

        if (index >= 0 && index < static_cast<int>(batch.size()))
        {
            processed[index] = true;
            CItem* item = batch[index].item;
            SAFETY_LEVEL safetyLevel = ParseLevel(level);
            std::wstring wReason = Utf8ToWide(reason);

            if (m_resultCallback)
            {
                CMainFrame::Get()->InvokeInMessageThread([this, item, safetyLevel, wReason]()
                {
                    m_resultCallback(item, safetyLevel, wReason);
                });
            }
        }
    }

    // Report error for any items not in response
    for (size_t i = 0; i < batch.size(); ++i)
    {
        if (!processed[i])
        {
            CItem* item = batch[i].item;
            if (m_resultCallback)
            {
                CMainFrame::Get()->InvokeInMessageThread([this, item]()
                {
                    m_resultCallback(item, SL_ERROR, L"No result from API");
                });
            }
        }
    }
}
