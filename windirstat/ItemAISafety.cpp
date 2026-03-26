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
#include "ItemAISafety.h"
#include "FileAISafetyControl.h"

CItemAISafety::CItemAISafety(CItem* item) : m_item(item) {}

CItemAISafety::~CItemAISafety()
{
    for (const auto& child : m_children)
    {
        delete child;
    }
}

const std::unordered_map<uint8_t, uint8_t> CItemAISafety::s_columnMap =
{
    { COL_AISAFETY_NAME, COL_NAME },
    { COL_AISAFETY_SIZE, COL_SIZE_PHYSICAL },
};

bool CItemAISafety::DrawSubItem(const int subitem, CDC* pdc, const CRect rc, const UINT state, int* width, int* focusLeft)
{
    if (subitem != COL_AISAFETY_NAME) return false;
    return CTreeListItem::DrawSubItem(s_columnMap.at(static_cast<uint8_t>(subitem)), pdc, rc, state, width, focusLeft);
}

std::wstring CItemAISafety::GetText(const int subitem) const
{
    // Root node
    if (GetParent() == nullptr)
    {
        if (subitem != COL_AISAFETY_NAME) return {};

        static const std::wstring title = L"AI Safety Analysis";
        return std::format(L"{} ({})", title, m_children.size());
    }

    if (m_item == nullptr) return {};

    switch (subitem)
    {
    case COL_AISAFETY_NAME:
        return m_item->GetPath();

    case COL_AISAFETY_LEVEL:
        switch (m_safetyLevel)
        {
        case SL_SAFE: return L"SAFE";
        case SL_LOW: return L"LOW";
        case SL_MEDIUM: return L"MED";
        case SL_HIGH: return L"HIGH";
        case SL_DANGER: return L"DANGER";
        case SL_PENDING: return L"...";
        case SL_ERROR: return L"ERROR";
        default: return L"?";
        }

    case COL_AISAFETY_SIZE:
        if (m_item != nullptr)
            return m_item->GetText(COL_SIZE_PHYSICAL);
        return {};

    case COL_AISAFETY_REASON:
        return m_reason;

    default:
        return {};
    }
}

int CItemAISafety::CompareSibling(const CTreeListItem* tlib, const int subitem) const
{
    if (GetParent() == nullptr) return 0;
    if (m_item == nullptr) return 0;

    const auto* other = reinterpret_cast<const CItemAISafety*>(tlib);
    if (other->m_item == nullptr) return 0;

    switch (subitem)
    {
    case COL_AISAFETY_NAME:
        return signum(_wcsicmp(m_item->GetPath().c_str(), other->m_item->GetPath().c_str()));

    case COL_AISAFETY_LEVEL:
        return signum(static_cast<int>(m_safetyLevel) - static_cast<int>(other->m_safetyLevel));

    case COL_AISAFETY_SIZE:
        return signum(static_cast<LONGLONG>(m_item->GetSizePhysical()) -
            static_cast<LONGLONG>(other->m_item->GetSizePhysical()));

    case COL_AISAFETY_REASON:
        return signum(_wcsicmp(m_reason.c_str(), other->m_reason.c_str()));

    default:
        return 0;
    }
}

int CItemAISafety::GetTreeListChildCount() const
{
    return static_cast<int>(m_children.size());
}

CTreeListItem* CItemAISafety::GetTreeListChild(const int i) const
{
    return m_children[i];
}

HICON CItemAISafety::GetIcon()
{
    if (m_visualInfo == nullptr)
    {
        return nullptr;
    }

    if (m_visualInfo->icon != nullptr)
    {
        return m_visualInfo->icon;
    }

    if (m_item == nullptr)
    {
        m_visualInfo->icon = GetIconHandler()->GetSearchImage();
        return m_visualInfo->icon;
    }

    m_visualInfo->icon = m_item->IsTypeOrFlag(IT_DIRECTORY, IT_DRIVE)
        ? GetIconHandler()->m_defaultFolderImage
        : GetIconHandler()->m_defaultFileImage;
    return m_visualInfo->icon;
}

COLORREF CItemAISafety::GetItemTextColor() const
{
    switch (m_safetyLevel)
    {
    case SL_SAFE: return RGB(0, 160, 0);       // Green
    case SL_LOW: return RGB(0, 100, 200);       // Blue
    case SL_MEDIUM: return RGB(200, 180, 0);    // Yellow/Gold
    case SL_HIGH: return RGB(220, 120, 0);      // Orange
    case SL_DANGER: return RGB(220, 0, 0);      // Red
    case SL_ERROR: return RGB(180, 0, 0);       // Dark Red
    case SL_PENDING: return RGB(128, 128, 128); // Gray
    default: return CLR_NONE;
    }
}

void CItemAISafety::SetSafetyResult(SAFETY_LEVEL level, const std::wstring& reason)
{
    m_safetyLevel = level;
    m_reason = reason;
}

void CItemAISafety::AddChild(CItemAISafety* child)
{
    child->SetParent(this);

    std::scoped_lock guard(m_protect);
    m_children.push_back(child);

    if (IsVisible() && IsExpanded())
    {
        CFileAISafetyControl::Get()->OnChildAdded(this, child);
    }
}

void CItemAISafety::RemoveChild(CItemAISafety* child)
{
    if (IsVisible())
    {
        CFileAISafetyControl::Get()->OnChildRemoved(this, child);
    }

    std::scoped_lock guard(m_protect);
    if (const auto it = std::ranges::find(m_children, child); it != m_children.end())
    {
        m_children.erase(it);
    }

    delete child;
}
