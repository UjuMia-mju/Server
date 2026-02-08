#pragma once
#include <functional>

/*--------------
	Job
---------------*/

using CallbackType = std::function<void()>;

class Job
{
public:
	Job(CallbackType&& callback) : _callback(std::move(callback))
	{
	}

	template<typename T, typename Ret, typename... Args>
	Job(shared_ptr<T>owner, Ret(T::* func)(Args...), Args&&... args)
	{
		_callback = [owner, func, args...]() 
		{
			(owner.get()->*func)(args...);
		};
	}

	void Execute()
	{
		_callback();
	}
private:
	CallbackType _callback;
};

