#include "pch.h"
#include "ItemManager.h"

ItemRef ItemManager::CreateItem(const string& itemName, const PosInfo& pos)
{
    WRITE_LOCK;

    uint64 itemId = _nextItemId++;
    ItemRef item = MakeShared<Item>(itemId, itemName);
    item->SetPosition(pos);
    _items[itemId] = item;

    std::cout << "[ItemManager] Created item: " << itemName
        << " (ID: " << itemId << ")" << endl;

    return item;
}

bool ItemManager::RemoveItem(uint64 itemId)
{
    WRITE_LOCK;

    auto it = _items.find(itemId);
    if (it == _items.end())
        return false;

    _items.erase(it);
    std::cout << "[ItemManager] Removed item ID: " << itemId << endl;
    return true;
}

ItemRef ItemManager::FindItem(uint64 itemId)
{
    READ_LOCK;

    auto it = _items.find(itemId);
    if (it == _items.end())
        return nullptr;

    return it->second;
}

bool ItemManager::UpdateItemState(uint64 itemId, Protocol::ItemState state,
    const PosInfo& pos, const RotInfo& rot, uint64 ownerId)
{
    WRITE_LOCK;

    auto item = FindItem(itemId);
    if (!item)
        return false;

    // 호스트가 보낸 정보 그대로 저장
    item->SetState(state);
    item->SetPosition(pos);
    item->SetRotation(rot);
    item->SetOwnerId(ownerId);

    return true;
}

vector<ItemRef> ItemManager::GetAllItems()
{
    READ_LOCK;

    vector<ItemRef> result;
    result.reserve(_items.size());

    for (auto& [itemId, item] : _items)
    {
        result.push_back(item);
    }

    return result;
}