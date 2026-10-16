#include "pch.h"
#include "LockStats.h"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <unordered_map>

namespace
{
	const int32 MAX_LOCK_NAMES = 64;	// 넘치면 마지막 칸("Others")에 합침

	struct ThreadCounters
	{
		atomic<int64> read[MAX_LOCK_NAMES];
		atomic<int64> write[MAX_LOCK_NAMES];

		ThreadCounters()
		{
			for (int32 i = 0; i < MAX_LOCK_NAMES; i++)
			{
				read[i].store(0);
				write[i].store(0);
			}
		}
	};

	struct Registry
	{
		mutex lock;
		vector<string> names;							// index -> 정리된 클래스 이름
		map<const char*, int32> nameIndex;				// typeid 이름 포인터 -> index
		vector<shared_ptr<ThreadCounters>> threads;		// 스레드가 끝나도 카운트는 남김
		vector<int64> baseRead;							// TakeRateLine 기준점
		vector<int64> baseWrite;
	};

	// 전역 객체 생성자/소멸자에서도 락을 잡을 수 있어서 처음 쓸 때 만들고 해제하지 않음
	Registry& GetRegistry()
	{
		static Registry* registry = new Registry();
		return *registry;
	}

	thread_local ThreadCounters* LCounters = nullptr;
	thread_local unordered_map<const char*, int32> LNameCache;

	// "class Room * __ptr64" -> "Room"
	string CleanName(const char* rawName)
	{
		string name = rawName;
		for (const char* token : { "class ", "struct ", "std::", " * __ptr64", " *" })
		{
			size_t pos;
			while ((pos = name.find(token)) != string::npos)
				name.erase(pos, strlen(token));
		}
		return name;
	}

	int32 LockNameIndex(const char* rawName)
	{
		auto cached = LNameCache.find(rawName);
		if (cached != LNameCache.end())
			return cached->second;

		Registry& registry = GetRegistry();
		lock_guard<mutex> guard(registry.lock);

		int32 index;
		auto it = registry.nameIndex.find(rawName);
		if (it != registry.nameIndex.end())
		{
			index = it->second;
		}
		else
		{
			// 같은 클래스라도 typeid 이름 포인터가 다를 수 있어서 정리된 이름으로 한 번 더 찾음
			string name = CleanName(rawName);
			auto found = find(registry.names.begin(), registry.names.end(), name);
			if (found != registry.names.end())
			{
				index = static_cast<int32>(found - registry.names.begin());
			}
			else if (registry.names.size() < MAX_LOCK_NAMES - 1)
			{
				index = static_cast<int32>(registry.names.size());
				registry.names.push_back(name);
			}
			else
			{
				index = MAX_LOCK_NAMES - 1;
				if (registry.names.size() < MAX_LOCK_NAMES)
					registry.names.push_back("Others");
			}
			registry.nameIndex[rawName] = index;
		}

		LNameCache[rawName] = index;
		return index;
	}

	ThreadCounters* GetCounters()
	{
		if (LCounters != nullptr)
			return LCounters;

		auto counters = make_shared<ThreadCounters>();
		Registry& registry = GetRegistry();
		lock_guard<mutex> guard(registry.lock);
		registry.threads.push_back(counters);
		LCounters = counters.get();
		return LCounters;
	}

	struct Row
	{
		string name;
		int64 read = 0;
		int64 write = 0;
	};

	// registry.lock 잡은 상태에서 호출. 스레드별 카운터를 클래스별로 합침
	void SumAll(Registry& registry, vector<int64>& outRead, vector<int64>& outWrite)
	{
		outRead.assign(registry.names.size(), 0);
		outWrite.assign(registry.names.size(), 0);
		for (auto& counters : registry.threads)
		{
			for (size_t i = 0; i < registry.names.size(); i++)
			{
				outRead[i] += counters->read[i].load(memory_order_relaxed);
				outWrite[i] += counters->write[i].load(memory_order_relaxed);
			}
		}
	}

	string FormatLine(const string& label, vector<Row> rows)
	{
		int64 allRead = 0;
		int64 allWrite = 0;
		for (auto& row : rows)
		{
			allRead += row.read;
			allWrite += row.write;
		}

		sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.read + a.write > b.read + b.write; });

		ostringstream ss;
		double readPercent = (allRead + allWrite) > 0 ? allRead * 100.0 / (allRead + allWrite) : 0.0;
		ss << fixed << setprecision(1)
			<< "[Lock] " << label << " | All R " << allRead << " / W " << allWrite << " (R " << readPercent << "%)";
		for (auto& row : rows)
		{
			if (row.read + row.write == 0)
				continue;
			ss << " | " << row.name << " R " << row.read << " / W " << row.write;
		}
		return ss.str();
	}

	// 워커 루프가 일이 없어도 매 틱 잡는 락 (이것만 있으면 1초 로그를 생략)
	bool IsIdleLock(const string& name)
	{
		return name == "JobTimer" || name.rfind("LockQueue", 0) == 0;
	}
}

void LockStats::OnLock(const char* name, bool isWrite)
{
	if (name == nullptr)
		return;

	int32 index = LockNameIndex(name);
	ThreadCounters* counters = GetCounters();
	if (isWrite)
		counters->write[index].fetch_add(1, memory_order_relaxed);
	else
		counters->read[index].fetch_add(1, memory_order_relaxed);
}

string LockStats::TakeRateLine(int32 intervalSec)
{
	Registry& registry = GetRegistry();
	vector<Row> rows;
	bool hasWork = false;
	{
		lock_guard<mutex> guard(registry.lock);

		vector<int64> read, write;
		SumAll(registry, read, write);
		registry.baseRead.resize(read.size(), 0);
		registry.baseWrite.resize(write.size(), 0);

		for (size_t i = 0; i < read.size(); i++)
		{
			Row row;
			row.name = registry.names[i];
			row.read = read[i] - registry.baseRead[i];
			row.write = write[i] - registry.baseWrite[i];
			if (row.read + row.write > 0 && IsIdleLock(row.name) == false)
				hasWork = true;
			rows.push_back(row);
		}

		registry.baseRead = read;
		registry.baseWrite = write;
	}

	if (hasWork == false)
		return "";

	return FormatLine(to_string(intervalSec) + "s", rows);
}

string LockStats::TotalLine()
{
	Registry& registry = GetRegistry();
	vector<Row> rows;
	{
		lock_guard<mutex> guard(registry.lock);

		vector<int64> read, write;
		SumAll(registry, read, write);
		for (size_t i = 0; i < read.size(); i++)
			rows.push_back(Row{ registry.names[i], read[i], write[i] });
	}

	return FormatLine("total", rows);
}
