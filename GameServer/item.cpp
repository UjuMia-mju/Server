#include "pch.h"
#include "item.h"

Item::Item(uint64 itemId, const string& itemName)
    : _itemId(itemId), _itemName(itemName)
{
    _state = Protocol::ON_GROUND;
}

Item::~Item()
{
}