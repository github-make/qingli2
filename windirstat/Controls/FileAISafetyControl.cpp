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
    if (m_rootItem == nullptr || item == nullptr) return;

    // Check if already tracked
    if (m_itemTracker.contains(item)) return;

    auto* safetyItem = new CItemAISafety(item);
    m_itemTracker.emplace(item, safetyItem);
    m_rootItem->AddChild(safetyItem);
}

void CFileAISafetyControl::UpdateResult(CItem* item, SAFETY_LEVEL level, const std::wstring& reason)
{
    auto it = m_itemTracker.find(item);
    if (it == m_itemTracker.end())
    {
        AddPendingItem(item);
        it = m_itemTracker.find(item);
        if (it == m_itemTracker.end())
        {
            return;
        }
    }

    it->second->SetSafetyResult(level, reason);

    // Force redraw
    if (GetItemCount() > 0)
    {
        RedrawItems(0, GetItemCount() - 1);
    }
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
    RemoveItemsInSubtree(item);
}

void CFileAISafetyControl::RemoveItemsInSubtree(CItem* item)
{
    if (item == nullptr || m_rootItem == nullptr || m_itemTracker.empty())
    {
        return;
    }

    std::vector<CItem*> itemsToRemove;
    itemsToRemove.reserve(m_itemTracker.size());
    for (const auto& [trackedItem, _] : m_itemTracker)
    {
        if (trackedItem == item || item->IsAncestorOf(trackedItem))
        {
            itemsToRemove.push_back(trackedItem);
        }
    }

    if (itemsToRemove.empty())
    {
        return;
    }

    SetRedraw(FALSE);
    for (auto* trackedItem : itemsToRemove)
    {
        const auto findItem = m_itemTracker.find(trackedItem);
        if (findItem == m_itemTracker.end())
        {
            continue;
        }

        m_rootItem->RemoveChild(findItem->second);
        m_itemTracker.erase(findItem);
    }
    SetRedraw(TRUE);
}

bool CFileAISafetyControl::ContainsTrackedItem(const CItem* item) const
{
    return item != nullptr && m_itemTracker.contains(const_cast<CItem*>(item));
}

bool CFileAISafetyControl::IsTrackedItemStale(const CItem* item) const
{
    return item == nullptr || !ContainsTrackedItem(item);
}

void CFileAISafetyControl::SortItems()
{
    __super::SortItems();
}

void CFileAISafetyControl::AfterDeleteAllItems()
{
    m_itemTracker.clear();

    delete m_rootItem;
    m_rootItem = new CItemAISafety();
    InsertItem(0, m_rootItem);
    m_rootItem->SetExpanded(true);
}
