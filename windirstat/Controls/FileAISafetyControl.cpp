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
#include "FileAISafetyControl.h"
#include "FileTreeView.h"

CFileAISafetyControl::CFileAISafetyControl()
    : CTreeListControl(COptions::AISafetyViewColumnOrder.Ptr(), COptions::AISafetyViewColumnWidths.Ptr(), LF_AILIST, false)
{
    m_singleton = this;
}

bool CFileAISafetyControl::GetAscendingDefault(const int column)
{
    return column == COL_AISAFETY_NAME;
}

BEGIN_MESSAGE_MAP(CFileAISafetyControl, CTreeListControl)
END_MESSAGE_MAP()

CFileAISafetyControl* CFileAISafetyControl::m_singleton = nullptr;

void CFileAISafetyControl::AddPendingItem(CItem* item)
{
    if (m_rootItem == nullptr) return;

    // Check if already tracked
    if (m_itemTracker.contains(item)) return;

    auto* safetyItem = new CItemAISafety(item);
    m_itemTracker.emplace(item, safetyItem);
    m_rootItem->AddChild(safetyItem);
}

void CFileAISafetyControl::UpdateResult(CItem* item, SAFETY_LEVEL level, const std::wstring& reason)
{
    const auto it = m_itemTracker.find(item);
    if (it == m_itemTracker.end())
    {
        // Item not yet added - add it first
        AddPendingItem(item);
        const auto it2 = m_itemTracker.find(item);
        if (it2 == m_itemTracker.end()) return;
        it2->second->SetSafetyResult(level, reason);
    }
    else
    {
        it->second->SetSafetyResult(level, reason);
    }

    // Force redraw
    RedrawItems(0, GetItemCount() - 1);
}

void CFileAISafetyControl::ClearResults()
{
    m_itemTracker.clear();

    SetRedraw(FALSE);
    DeleteAllItems();       // Clear list control items first to avoid dangling pointers
    AfterDeleteAllItems();  // Then recreate root
    SetRedraw(TRUE);
}

void CFileAISafetyControl::RemoveItem(CItem* item)
{
    const auto findItem = m_itemTracker.find(item);
    if (findItem == m_itemTracker.end()) return;

    SetRedraw(FALSE);
    m_rootItem->RemoveChild(findItem->second);
    m_itemTracker.erase(findItem);
    SetRedraw(TRUE);
}

void CFileAISafetyControl::AfterDeleteAllItems()
{
    m_itemTracker.clear();

    delete m_rootItem;
    m_rootItem = new CItemAISafety();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
