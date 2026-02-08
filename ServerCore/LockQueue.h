#pragma once

template<typename T>
class LockQueue
{
public:
	void Push(T item)
	{
		WRITE_LOCK;
		_items.push(item);
	}

	T Pop()
	{
		WRITE_LOCK;
		if (_items.empty())
		{
			return T();
		}

		T ret = _items.front();
		_items.pop();
		return ret;
	}

	void PopAll(OUT xvector<T>& items)
	{
		WRITE_LOCK;
		while (T item = Pop())
		{
			items.push_back(item);
		}
	}

	void Clear()
	{
		WRITE_LOCK;
		_items = xqueue<T>();
	}
private:
	USE_LOCK;
	xqueue<T> _items;
};

