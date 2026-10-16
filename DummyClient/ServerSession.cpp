#include "pch.h"
#include "ServerSession.h"
#include "ServerPacketHandler.h"
#include "PacketStats.h"

atomic<int64> GRecvPacketCount = 0;
atomic<int32> GInviteAcceptDelayMs = 1000;

ServerSession::ServerSession(const string& email, const string& password, bool autoLogin)
	: _email(email), _password(password), _autoLogin(autoLogin)
{
}

ServerSession::~ServerSession()
{
	cout << "~ServerSession()" << endl;
}

void ServerSession::OnConnected()
{
	if (_autoLogin == false)
		return;

	this_thread::sleep_for(200ms);

	SendLogin();
}

void ServerSession::OnRecvPacket(BYTE* buffer, int32 len)
{
	GRecvPacketCount.fetch_add(1);

	PacketSessionRef session = GetPacketSessionRef();
	ServerPacketHandler::HandlePacket(session, buffer, len);
}

void ServerSession::OnSend(int32 len)
{
	// Optional: Add logging if needed
}

void ServerSession::Send(SendBufferRef sendBuffer)
{
	PacketStats::OnSend(sendBuffer);
	PacketSession::Send(sendBuffer);
}

void ServerSession::OnDisconnected()
{
	// Optional: Add cleanup if needed
}

void ServerSession::SetPlayerInfo(const string& name, int32 tag, int32 playerId)
{
	_playerName = name;
	_playerTag = tag;
	_playerId = playerId;
	_isLoggedIn = true;
}

bool ServerSession::IsLoginCompleted() const
{
	return _isLoggedIn;
}

const string& ServerSession::GetPlayerName() const
{
	return _playerName;
}

int32 ServerSession::GetPlayerTag() const
{
	return _playerTag;
}

void ServerSession::SetRoomId(uint64 roomId)
{
	_roomId = roomId;
}

uint64 ServerSession::GetRoomId() const
{
	return _roomId;
}

bool ServerSession::IsInRoom() const
{
	return _roomId != 0;
}

void ServerSession::OnReadyChanged(uint64 playerId, bool isReady)
{
	lock_guard<mutex> guard(_readyLock);
	if (isReady)
		_readyPlayers.insert(playerId);
	else
		_readyPlayers.erase(playerId);
}

int32 ServerSession::GetReadyCount()
{
	lock_guard<mutex> guard(_readyLock);
	return static_cast<int32>(_readyPlayers.size());
}

void ServerSession::SetGameReady()
{
	_isGameReady = true;
}

bool ServerSession::IsGameReady() const
{
	return _isGameReady;
}

void ServerSession::SetRoomStarted()
{
	_isRoomStarted = true;
}

bool ServerSession::IsRoomStarted() const
{
	return _isRoomStarted;
}

void ServerSession::SetStageList(vector<StageKey> stages)
{
	{
		lock_guard<mutex> guard(_stageLock);
		_stageList = move(stages);
	}
	_hasStageList = true;
}

vector<StageKey> ServerSession::GetStageList()
{
	lock_guard<mutex> guard(_stageLock);
	return _stageList;
}

bool ServerSession::HasStageList() const
{
	return _hasStageList;
}

void ServerSession::OnShowStageReceived()
{
	_showStageCount.fetch_add(1);
}

int32 ServerSession::GetShowStageCount() const
{
	return _showStageCount;
}

void ServerSession::SetHostStage(int32 mapId)
{
	_hostStage = mapId;
}

int32 ServerSession::GetHostStage() const
{
	return _hostStage;
}

void ServerSession::SendGetDbDataPacket()
{
	Protocol::C_GET_DB_DATA pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendLoginPacket(string id, string psw)
{
	Protocol::C_LOGIN loginPkt;
	loginPkt.set_userid(id);
	loginPkt.set_psw(psw);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(loginPkt);
	Send(sendBuffer);

	cout << "[Client] Login packet sent - Email: " << id << endl;
}

void ServerSession::SendLogin()
{
	SendLoginPacket(_email, _password);
}

void ServerSession::SendCreateRoom()
{
	Protocol::C_CREATE_ROOM pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);

	cout << "[Client] Create room packet sent" << endl;
}

void ServerSession::SendInvitePacket(string name, int32 tag)
{
	Protocol::C_INVITE_PLAYER invitePkt;
	invitePkt.set_player_name(name);
	invitePkt.set_player_tag(tag);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(invitePkt);
	Send(sendBuffer);

	cout << "[Client] Invite packet sent - Target: " << name << "#" << tag << endl;
}

void ServerSession::SendLeaveRoom()
{
	Protocol::C_LEAVE_ROOM pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendEnterRoom()
{
	Protocol::C_ENTER_ROOM pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
	cout << "[Client] Show members packet sent" << endl;
}

void ServerSession::SendStageData(int32 map_id, int32 chapter, int32 stageIndex)
{
	Protocol::C_SHOW_STAGE pkt;

	pkt.set_chapter(chapter);
	pkt.set_map_id(map_id);
	pkt.set_stageindex(stageIndex);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendMyClearStageData()
{
	Protocol::C_GET_CLEAR_INFO pkt;

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendGachaPool()
{
	Protocol::C_GACHA_POOL_LIST pkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendGacha()
{
	Protocol::C_GACHA pkt;
	pkt.set_pool_id(1); // Example pool ID, adjust as needed
	pkt.set_pull_count(1); // Example pull count, adjust as needed
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendHostStageSelect(int32 map_id)
{
	Protocol::C_HOST_SHOW_STAGE pkt;
	pkt.set_map_id(map_id);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendStartStage(int32 map_id, int32 chapter, int32 stageIndex)
{
	Protocol::C_START_STAGE pkt;

	// 서버가 map_id + chapter + stage가 모두 맞아야 캐시 hit로 처리하므로 셋 다 보냄
	pkt.set_map_id(map_id);
	pkt.set_chapter(chapter);
	pkt.set_stageindex(stageIndex);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendStageEnter()
{
	Protocol::C_ENTER_GAME pkt;

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendStageClear(int32 map_id, int32 star, int32 clearTime)
{
	Protocol::C_GAME_CLEAR pkt;
	pkt.set_map_id(map_id);
	pkt.set_star(star);
	pkt.set_clear_time_seconds(clearTime);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);
}

void ServerSession::SendReady(bool isReady)
{
	Protocol::C_READY readyPkt;
	readyPkt.set_is_ready(isReady);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(readyPkt);
	Send(sendBuffer);
	cout << "[Client] Ready packet sent - Ready: " << (isReady ? "Yes" : "No") << endl;
}

void ServerSession::SendStartRoom()
{
	Protocol::C_START_ROOM startPkt;
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(startPkt);
	Send(sendBuffer);
	cout << "[Client] Start room packet sent" << endl;
}


void ServerSession::SendShowStage(int32 chapter, int32 stage)
{
	Protocol::C_SHOW_STAGE pkt;
	pkt.set_chapter(chapter);
	pkt.set_stageindex(stage);

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(pkt);
	Send(sendBuffer);

	cout << "[Client] Show stage packet sent - Stage " << chapter << "-" << stage << endl;
}

