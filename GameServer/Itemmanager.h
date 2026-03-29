#pragma once
#include "Item.h"

class ItemManager
{
private:
    ItemManager() = default;

public:
    static ItemManager& Instance()
    {
        static ItemManager instance;
        return instance;
    }

    // 아이템 생성/삭제
    ItemRef CreateItem(const string& itemName, const PosInfo& pos);
    bool RemoveItem(uint64 itemId);
    ItemRef FindItem(uint64 itemId);

    // 아이템 상태 관리 (호스트가 보낸 정보 그대로 저장)
    bool UpdateItemState(uint64 itemId, Protocol::ItemState state,
        const PosInfo& pos, const RotInfo& rot, uint64 ownerId);

    // 모든 아이템 정보 가져오기 (새 플레이어 입장 시)
    vector<ItemRef> GetAllItems();

private:
    USE_LOCK;
    unordered_map<uint64, ItemRef> _items;
    uint64 _nextItemId = 1;
};
