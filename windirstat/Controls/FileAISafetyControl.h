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
#include "ItemAISafety.h"
#include "TreeListControl.h"

class CFileAISafetyControl final : public CTreeListControl
{
public:
    CFileAISafetyControl();
    ~CFileAISafetyControl() override { m_singleton = nullptr; }
    bool GetAscendingDefault(int column) override;
    static CFileAISafetyControl* Get() { return m_singleton; }
    CItemAISafety* GetRootItem() const { return m_rootItem; }

    void AddPendingItem(CItem* item);
    void UpdateResult(CItem* item, SAFETY_LEVEL level, const std::wstring& reason);
    void ClearResults();
    void RemoveItem(CItem* item);
    void AfterDeleteAllItems() override;

protected:
    static CFileAISafetyControl* m_singleton;
    CItemAISafety* m_rootItem = nullptr;
    std::unordered_map<CItem*, CItemAISafety*> m_itemTracker;

    DECLARE_MESSAGE_MAP()
};
