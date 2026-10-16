#include "pch.h"
#include "PacketStats.h"
#include "ServerPacketHandler.h"
#include <algorithm>
#include <iomanip>
#include <sstream>

using Category = PacketStats::Category;

struct PacketDesc
{
	const char* name;
	Category category;
};

// 클라이언트 -> 서버 요청 패킷 분류 (서버 ClientPacketHandler.cpp 동작 기준)
//   새 C_ 패킷을 추가하면 여기에도 넣어야 함 (안 넣으면 Unknown으로 집계)
static const map<uint16, PacketDesc>& PacketTable()
{
	static const map<uint16, PacketDesc> table =
	{
		// Write: 서버 상태를 바꿈
		{ PKT_C_LOGIN,				{ "C_LOGIN",				Category::Write } },	// 세션 인증, PlayerInfo 로드
		{ PKT_C_CREATE_ROOM,		{ "C_CREATE_ROOM",			Category::Write } },	// RoomManager에 방 추가
		{ PKT_C_ENTER_ROOM,			{ "C_ENTER_ROOM",			Category::Write } },	// Room 멤버 추가
		{ PKT_C_LEAVE_ROOM,			{ "C_LEAVE_ROOM",			Category::Write } },	// Room 멤버 제거
		{ PKT_C_INVITE_PLAYER,		{ "C_INVITE_PLAYER",		Category::Write } },	// InviteManager에 초대 생성
		{ PKT_C_INVITE_RESPONSE,	{ "C_INVITE_RESPONSE",		Category::Write } },	// 초대 제거 + Room 멤버 추가
		{ PKT_C_READY,				{ "C_READY",				Category::Write } },	// Room 레디 상태
		{ PKT_C_START_ROOM,			{ "C_START_ROOM",			Category::Write } },	// Room playing 상태
		{ PKT_C_ENTER_GAME,			{ "C_ENTER_GAME",			Category::Write } },	// Room 게임 입장 상태
		{ PKT_C_TEST_ENTER_ROOM,	{ "C_TEST_ENTER_ROOM",		Category::Write } },
		{ PKT_C_TEST_ENTER_GAME,	{ "C_TEST_ENTER_GAME",		Category::Write } },
		{ PKT_C_MOVE,				{ "C_MOVE",					Category::Write } },	// Player 위치
		{ PKT_C_PLAYER_ANIMATION,	{ "C_PLAYER_ANIMATION",		Category::Write } },	// Player 애니메이션 상태
		{ PKT_C_PLAYER_STAT_EVENT,	{ "C_PLAYER_STAT_EVENT",	Category::Write } },	// (서버 핸들러 아직 비어 있음)
		{ PKT_C_GACHA,				{ "C_GACHA(DB)",			Category::Write } },	// 재화 차감 + 스킨 추가
		{ PKT_C_GAME_CLEAR,			{ "C_GAME_CLEAR(DB)",		Category::Write } },	// 클리어 기록 갱신

		// Read: 조회만 함
		{ PKT_C_GET_DB_DATA,		{ "C_GET_DB_DATA",			Category::Read } },
		{ PKT_C_GET_CURRENCY,		{ "C_GET_CURRENCY(DB)",		Category::Read } },
		{ PKT_C_GET_CLEAR_INFO,		{ "C_GET_CLEAR_INFO(DB)",	Category::Read } },
		{ PKT_C_GACHA_POOL_LIST,	{ "C_GACHA_POOL_LIST",		Category::Read } },
		{ PKT_C_MY_SKINS,			{ "C_MY_SKINS",				Category::Read } },
		{ PKT_C_ROOM_LIST,			{ "C_ROOM_LIST",			Category::Read } },
		{ PKT_C_SHOW_STAGE,			{ "C_SHOW_STAGE",			Category::Read } },
		{ PKT_C_HOST_SHOW_STAGE,	{ "C_HOST_SHOW_STAGE",		Category::Read } },	// 스테이지 캐시 조회 후 방에 브로드캐스트
		{ PKT_C_START_STAGE,		{ "C_START_STAGE",			Category::Read } },	// 상태 변경 없이 스테이지 캐시 조회 후 브로드캐스트

		// Relay: 상태 변경 없이 같은 방에 그대로 전달
		{ PKT_C_RELAY_PACKET,		{ "C_RELAY_PACKET",			Category::Relay } },
		{ PKT_C_CHAT,				{ "C_CHAT",					Category::Relay } },
		{ PKT_C_OBJECT_PICKUP,		{ "C_OBJECT_PICKUP",		Category::Relay } },
		{ PKT_C_OBJECT_DROP,		{ "C_OBJECT_DROP",			Category::Relay } },
		{ PKT_C_OBJECT_MOVE,		{ "C_OBJECT_MOVE",			Category::Relay } },
	};
	return table;
}

static const char* CategoryName(Category category)
{
	switch (category)
	{
	case Category::Read:	return "Read";
	case Category::Write:	return "Write";
	case Category::Relay:	return "Relay";
	default:				return "Unknown";
	}
}

// 패킷 ID별 보낸 횟수 (IOCP 워커 스레드 + 메인 스레드에서 동시에 증가)
static atomic<int64> GSendCount[UINT16_MAX + 1];

void PacketStats::OnSend(const SendBufferRef& sendBuffer)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
	GSendCount[header->id].fetch_add(1, memory_order_relaxed);
}

static Category CategoryOf(uint16 id)
{
	auto it = PacketTable().find(id);
	return (it != PacketTable().end()) ? it->second.category : Category::Unknown;
}

PacketStats::Totals PacketStats::GetTotals()
{
	Totals totals;
	for (int32 id = 0; id <= UINT16_MAX; id++)
	{
		int64 count = GSendCount[id].load(memory_order_relaxed);
		if (count == 0)
			continue;

		switch (CategoryOf(static_cast<uint16>(id)))
		{
		case Category::Read:	totals.read += count;		break;
		case Category::Write:	totals.write += count;		break;
		case Category::Relay:	totals.relay += count;		break;
		default:				totals.unknown += count;	break;
		}
	}
	return totals;
}

// TakeRate 기준점 (마지막 TakeRate 호출 시점의 누적 개수)
static mutex GRateLock;
static PacketStats::Totals GRateBase;

string PacketStats::TakeRate()
{
	Totals cur = GetTotals();
	Totals prev;
	{
		lock_guard<mutex> guard(GRateLock);
		prev = GRateBase;
		GRateBase = cur;
	}

	ostringstream ss;
	ss << "[Packet] R " << (cur.read - prev.read) << "/s, W " << (cur.write - prev.write) << "/s, Relay " << (cur.relay - prev.relay) << "/s";
	return ss.str();
}

void PacketStats::Reset()
{
	for (auto& count : GSendCount)
		count.store(0);

	lock_guard<mutex> guard(GRateLock);
	GRateBase = Totals{};
}

PacketStats::Snapshot PacketStats::TakeSnapshot()
{
	Snapshot snapshot(UINT16_MAX + 1);
	for (int32 id = 0; id <= UINT16_MAX; id++)
		snapshot[id] = GSendCount[id].load();

	return snapshot;
}

void PacketStats::Report(ostream& out, const string& title, const Snapshot* since)
{
	struct Row
	{
		string name;
		Category category;
		int64 count;
	};

	vector<Row> rows;
	int64 total = 0;
	int64 byCategory[4] = {};

	for (int32 id = 0; id <= UINT16_MAX; id++)
	{
		int64 count = GSendCount[id].load() - (since != nullptr ? (*since)[id] : 0);
		if (count == 0)
			continue;

		auto it = PacketTable().find(static_cast<uint16>(id));
		Row row;
		row.name = (it != PacketTable().end()) ? it->second.name : "ID " + to_string(id);
		row.category = (it != PacketTable().end()) ? it->second.category : Category::Unknown;
		row.count = count;

		rows.push_back(row);
		total += count;
		byCategory[static_cast<int32>(row.category)] += count;
	}

	auto percent = [](int64 part, int64 whole) { return whole > 0 ? part * 100.0 / whole : 0.0; };

	out << "\n========== [Packet] " << title << " ==========" << endl;
	if (total == 0)
	{
		out << "  No packets sent." << endl;
		return;
	}

	out << fixed << setprecision(1);
	out << "  Total sent: " << total << endl;
	for (Category category : { Category::Read, Category::Write, Category::Relay, Category::Unknown })
	{
		int64 count = byCategory[static_cast<int32>(category)];
		if (count == 0 && category == Category::Unknown)
			continue;

		out << "  " << left << setw(8) << CategoryName(category) << right << setw(10) << count
			<< "  (" << setw(5) << percent(count, total) << "%)" << endl;
	}

	// Relay는 빼고 Read + Write만 기준으로 본 비율
	int64 readCount = byCategory[static_cast<int32>(Category::Read)];
	int64 writeCount = byCategory[static_cast<int32>(Category::Write)];
	int64 readWrite = readCount + writeCount;
	out << "  Read : Write = " << percent(readCount, readWrite) << " : " << percent(writeCount, readWrite)
		<< "  (excluding Relay)" << endl;

	// 패킷별 (분류 -> 개수 많은 순)
	sort(rows.begin(), rows.end(), [](const Row& a, const Row& b)
		{
			if (a.category != b.category)
				return a.category < b.category;
			return a.count > b.count;
		});

	out << "\n  " << left << setw(8) << "Type" << setw(26) << "Packet" << right << setw(10) << "Count"
		<< setw(10) << "Total%" << setw(10) << "InType%" << endl;
	for (auto& row : rows)
	{
		out << "  " << left << setw(8) << CategoryName(row.category) << setw(26) << row.name << right
			<< setw(10) << row.count
			<< setw(9) << percent(row.count, total) << "%"
			<< setw(9) << percent(row.count, byCategory[static_cast<int32>(row.category)]) << "%" << endl;
	}

	out << defaultfloat;
	out.flush();
}
