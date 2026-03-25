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
#include "AISafetyAnalyzer.h"

using ITEMAISAFETYCOLUMNS = enum : std::uint8_t
{
    COL_AISAFETY_NAME,
    COL_AISAFETY_LEVEL,
    COL_AISAFETY_SIZE,
    COL_AISAFETY_REASON
};

class CItemAISafety final : public CTreeListItem
{
    std::shared_mutex m_protect;
    std::vector<CItemAISafety*> m_children;
    CItem* m_item = nullptr;
    SAFETY_LEVEL m_safetyLevel = SL_PENDING;
    std::wstring m_reason;

public:
    CItemAISafety(const CItemAISafety&) = delete;
    CItemAISafety(CItemAISafety&&) = delete;
    CItemAISafety& operator=(const CItemAISafety&) = delete;
    CItemAISafety& operator=(CItemAISafety&&) = delete;
    CItemAISafety() = default;
    CItemAISafety(CItem* item);
    ~CItemAISafety() override;

    static const std::unordered_map<uint8_t, uint8_t> s_columnMap;

    // CTreeListItem Interface
    bool DrawSubItem(int subitem, CDC* pdc, CRect rc, UINT state, int* width, int* focusLeft) override;
    std::wstring GetText(int subitem) const override;
    int CompareSibling(const CTreeListItem* tlib, int subitem) const override;
    int GetTreeListChildCount() const override;
    CTreeListItem* GetTreeListChild(int i) const override;
    HICON GetIcon() override;
    CItem* GetLinkedItem() noexcept override { return m_item; }
    COLORREF GetItemTextColor() const override;

    void SetSafetyResult(SAFETY_LEVEL level, const std::wstring& reason);
    SAFETY_LEVEL GetSafetyLevel() const { return m_safetyLevel; }
    const std::wstring& GetReason() const { return m_reason; }

    void AddChild(CItemAISafety* child);
    void RemoveChild(CItemAISafety* child);
};
