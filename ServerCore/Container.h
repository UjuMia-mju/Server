#pragma once
#include "Types.h"
#include "Allocator.h"
#include "pch.h"

#include <vector>
#include <array>
#include <list>
#include <queue>
#include <stack>
#include <map>
#include <set>
#include <deque>
#include <string>
#include <unordered_map>
#include <unordered_set>

using namespace std;

template<typename T>
using xvector = vector<T, StlAllocator<T>>;

template<typename T>
using xlist = list<T, StlAllocator<T>>;

template<typename T, uint32 Size>
using xarray = array<T, Size>;

template<typename Key, typename Type, typename Pred = less<Key>>
using xmap = map<Key, Type, Pred, StlAllocator<pair<const Key, Type>>>;

template<typename Key, typename Pred = less<Key>>
using xset = set<Key, Pred, StlAllocator<Key>>;

template<typename T>
using xdeque = deque<T, StlAllocator<T>>;

template<typename T, typename Container = xdeque<T>>
using xqueue = queue<T, Container>;

template<typename T, typename Container = xdeque<T>>
using xstack = stack<T, Container>;

template<typename T, typename Container = xvector<T>, typename Pred = less<typename Container::value_type>>
using xpriority_queue = priority_queue<T, Container, Pred>;

using xstring = basic_string<char, char_traits<char>, StlAllocator<char>>;

using xwstring = basic_string<wchar_t, char_traits<wchar_t>, StlAllocator<wchar_t>>;

template<typename Key, typename Type, typename Hash = hash<Key>, typename Pred = equal_to<Key>>
using xunordered_map = unordered_map<Key, Type, Hash, Pred, StlAllocator<pair<const Key, Type>>>;

template<typename Key, typename Hash = hash<Key>, typename Pred = equal_to<Key>>
using xhashSet = unordered_set < Key, Hash, Pred, StlAllocator<Key>>;