#pragma once
#include "Protocol.pb.h"
using namespace Protocol;

class Item
{
public:
    Item(uint64 itemId, const string& itemName);
    ~Item();

    // Getters
    uint64 GetItemId() const { return _itemId; }
    string GetItemName() const { return _itemName; }
    Protocol::ItemState GetState() const { return _state; }
    PosInfo GetPosition() const { return _position; }
    RotInfo GetRotation() const { return _rotation; }
    uint64 GetOwnerId() const { return _ownerId; }

    // Setters (클라이언트 정보 그대로 저장)
    void SetState(Protocol::ItemState state) { _state = state; }
    void SetPosition(const PosInfo& pos) { _position = pos; }
    void SetRotation(const RotInfo& rot) { _rotation = rot; }
    void SetOwnerId(uint64 ownerId) { _ownerId = ownerId; }

private:
    uint64 _itemId = 0;
    string _itemName;
    Protocol::ItemState _state = Protocol::ON_GROUND;

    PosInfo _position;
    RotInfo _rotation;

    uint64 _ownerId = 0; // 0이면 월드 아이템
};

using ItemRef = shared_ptr<Item>;