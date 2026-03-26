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
#include "WinHttpClient.h"
#include "Item.h"

#include <thread>
#include <atomic>
#include <optional>
#include <functional>

enum SAFETY_LEVEL : std::uint8_t
{
    SL_SAFE,     // Green - safe to delete
    SL_LOW,      // Blue - low risk
    SL_MEDIUM,   // Yellow - moderate risk
    SL_HIGH,     // Orange - high risk
    SL_DANGER,   // Red - critical system file
    SL_PENDING,  // Gray - waiting for analysis
    SL_ERROR     // Red cross - API error
};

struct FileAnalysisInfo
{
    CItem* item = nullptr;
    std::wstring path;
    std::wstring name;
    ULONGLONG sizePhysical = 0;
    FILETIME lastChange = {};
    DWORD attributes = 0;
    std::vector<BYTE> contentHead;    // First 256 bytes
    std::vector<BYTE> contentMiddle;  // Middle 256 bytes
    std::vector<BYTE> contentTail;    // Last 256 bytes
};

// Callback: item, safety level, reason
using AISafetyResultCallback = std::function<void(CItem*, SAFETY_LEVEL, const std::wstring&)>;
// Callback: total, completed
using AISafetyProgressCallback = std::function<void(size_t, size_t)>;
// Callback: when analysis finishes
using AISafetyCompleteCallback = std::function<void()>;

class CAISafetyAnalyzer
{
    static CAISafetyAnalyzer* s_singleton;

    CWinHttpClient m_httpClient;
    std::jthread m_workerThread;
    std::atomic<bool> m_running{false};

    AISafetyResultCallback m_resultCallback;
    AISafetyProgressCallback m_progressCallback;
    AISafetyCompleteCallback m_completeCallback;

public:
    CAISafetyAnalyzer();
    ~CAISafetyAnalyzer();

    static CAISafetyAnalyzer* Get();

    CAISafetyAnalyzer(const CAISafetyAnalyzer&) = delete;
    CAISafetyAnalyzer& operator=(const CAISafetyAnalyzer&) = delete;

    void SetResultCallback(AISafetyResultCallback cb) { m_resultCallback = std::move(cb); }
    void SetProgressCallback(AISafetyProgressCallback cb) { m_progressCallback = std::move(cb); }
    void SetCompleteCallback(AISafetyCompleteCallback cb) { m_completeCallback = std::move(cb); }

    void AnalyzeItems(const std::vector<CItem*>& items);
    void Cancel();
    bool IsRunning() const { return m_running; }

private:
    void WorkerProc(std::vector<CItem*> items, std::stop_token stopToken);
    void WorkerProcInner(std::vector<CItem*>& items, std::stop_token& stopToken);

    // Recursively expand folders into leaf file list
    static void ExpandFolders(CItem* item, std::vector<CItem*>& result);

    // Collect file metadata and content snippets
    static FileAnalysisInfo CollectFileInfo(CItem* item);

    // Read up to 'count' bytes from file at given offset
    static std::vector<BYTE> ReadFileSnippet(const std::wstring& path, ULONGLONG offset, DWORD count);

    // Check user-defined custom rules; returns level if matched
    static std::optional<SAFETY_LEVEL> CheckCustomRules(const FileAnalysisInfo& info);

    // Build OpenAI-compatible JSON request for a batch
    static std::string BuildBatchRequest(const std::vector<FileAnalysisInfo>& batch);

    // Parse response JSON and call result callback for each item
    void ParseBatchResponse(const std::string& responseBody, const std::vector<FileAnalysisInfo>& batch);

    // Convert bytes to hex string for prompt
    static std::string BytesToHex(const std::vector<BYTE>& data);

    // Parse safety level from string
    static SAFETY_LEVEL ParseLevel(const std::string& levelStr);
};
