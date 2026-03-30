#include "pch.h"
#include "ClientPacketHandler.h"
#include "Player.h"
#include "Room.h"
#include "RoomManager.h"
#include "GameSession.h"
#include "AccountDB.h"
#include "AuthValidator.h"
#include "InviteManager.h"
#include "GachaManager.h"
#include "StageManager.h"

PacketHandleFunc GPacketHandler[UINT16_MAX];

namespace
{
	void SendErrorResponse(PacketSessionRef& session, const PacketResult& result)
	{
		// 패킷 타입별 에러 응답 (함수 오버로딩으로 처리)
	}

	void SendCreateRoomError(PacketSessionRef& session, const std::string& errorMsg)
	{
		Protocol::S_CREATE_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg(errorMsg);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
	}

	void SendStartRoomError(PacketSessionRef& session, const std::string& errorMsg)
	{
		Protocol::S_START_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg(errorMsg);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
	}

	void SendEnterGameError(PacketSessionRef& session, const std::string& errorMsg)
	{
		Protocol::S_ENTER_GAME errorPkt;
		errorPkt.set_success(false);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
	}
}

#define CHECK_AUTH_LOGIN(session, ErrorSender) \
    GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session); \
    PacketResult authResult = AuthValidator::ValidateAuth(gameSession, AuthLevel::LOGGED_IN); \
    if (!authResult.IsSuccess()) { \
        ErrorSender(session, authResult.GetMessage()); \
        return false; \
    }

#define CHECK_AUTH_ROOM(session, room, ErrorSender) \
    GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session); \
    PacketResult authResult; \
    auto room = AuthValidator::GetRoomIfValid(gameSession, &authResult); \
    if (!room) { \
        ErrorSender(session, authResult.GetMessage()); \
        return false; \
    }

// 컨텐츠는 직접 개발
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO : Log
	return false;
}

bool Handle_C_GET_DB_DATA(PacketSessionRef& session, Protocol::C_GET_DB_DATA& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	// DB에서 정보를 보낸다. (가차 리스트, 스킨 리스트, 스테이지 리스트)

	// 스테이지 리스트
	Protocol::S_STAGE_INFO sStageInfoPkt;
	GStageManager.FillStageListPacket(sStageInfoPkt);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(sStageInfoPkt);
	session->Send(sendBuffer);

	// 스킨 폴 리스트
	Protocol::S_GACHA_POOL_LIST skinPoolList;
	GGACHA.FillGachaPoolListPacket(skinPoolList);
	auto skinPoolSendBuffer = ClientPacketHandler::MakeSendBuffer(skinPoolList);
	session->Send(skinPoolSendBuffer);

	// 스킨 리스트
	Protocol::S_SKIN_LIST skinListPkt;
	GGACHA.FillSkinListPacket(skinListPkt);
	auto skinListSendBuffer = ClientPacketHandler::MakeSendBuffer(skinListPkt);
	session->Send(skinListSendBuffer);

	return true;
}

bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	string email = pkt.userid();
	string password = pkt.psw();

	cout << "Received Login - Email: " << email << ", Password: " << password << endl;

	Protocol::S_LOGIN sLoginPkt;

	// DB에서 플레이어 정보를 긁어온다.
	if (AccountDB::ValidateAccount(email, password))
	{
		int32 dbPlayerId = 0;
		string dbPlayerName;
		int32 dbPlayerTag;
		if (AccountDB::GetPlayerInfo(email, password, dbPlayerId, dbPlayerName, dbPlayerTag))
		{
			sLoginPkt.set_success(true);
			
			// DB에서 플레이어 정보를 세팅한다.
			auto player = sLoginPkt.mutable_player();
			player->set_id(dbPlayerId);
			player->set_name(dbPlayerName);
			player->set_tag(dbPlayerTag);

			// Player 연결
			PlayerRef playerRef = MakeShared<Player>();
			playerRef->name = player->name();
			playerRef->playerId = player->id();
			playerRef->tag = player->tag();
			playerRef->ownerSession = gameSession;

			// PlayerInfo연결
			PlayerInfoRef playerInfo = MakeShared<PlayerInfo>();

			int32 dbCoin = 0;
			int32 dbGem = 0;
			std::vector<OwnedSkinInfo> loadedSkins;
			if (AccountDB::GetUserProfileInfo(dbPlayerId, dbCoin, dbGem, loadedSkins))
			{
				auto infoPkt = sLoginPkt.mutable_player_info();
				infoPkt->set_player_id(player->id());
				infoPkt->set_coin(dbCoin);
				infoPkt->set_gem(dbGem);

				playerInfo->SetCoin(dbCoin);
				playerInfo->SetGem(dbGem);
				playerInfo->SetDbUserId(dbPlayerId);

				for (const auto& skin : loadedSkins)
				{
					playerInfo->AddSkin(skin.skinId, skin.getAt, skin.isEquipped);
					infoPkt->add_owned_skins(skin.skinId);
				}
			}
			else
			{
				cout << "Failed to retrieve user profile info for player ID: " << dbPlayerId << endl;
			}
			
			playerInfo->SetDbUserId(player->id());
			playerInfo->ownerSession = gameSession; //순환 참조 방지

			gameSession->SetPlayer(playerRef);
			gameSession->SetPlayerInfo(playerInfo);

			cout << "gem: " << playerInfo->GetGem() << ", coin: " << playerInfo->GetCoin() << endl;
		}
		else
		{
			sLoginPkt.set_success(false);
			std::cout << "Failed to retrieve player info for email: " << email << endl;
		}
	}
	else
	{
		// Fail to Login
		sLoginPkt.set_success(false);
		std::cout << "Failed to validate account for email: " << email << endl;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(sLoginPkt);
	std::cout << "Sending S_LOGIN packet - Size: " << sendBuffer->WriteSize()
		<< ", Success: " << sLoginPkt.success() << endl;
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_GET_CURRENCY(PacketSessionRef& session, Protocol::C_GET_CURRENCY& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	int32 dbPlayerId = gameSession->GetPlayerInfo()->GetDbUserId();
	int32 dbCoin = 0;
	int32 dbGem = 0;
	Protocol::S_GET_CURRENCY resPkt;

	if (AccountDB::GetUserGoods(dbPlayerId, dbCoin, dbGem))
	{
		resPkt.set_success(true);
		resPkt.set_coin(dbCoin);
		resPkt.set_gem(dbGem);
		
		cout << "Sent currency info - Player ID: " << dbPlayerId << ", Coin: " << dbCoin << ", Gem: " << dbGem << endl;
	}
	else
	{
		resPkt.set_success(false);
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_GACHA(PacketSessionRef& session, Protocol::C_GACHA& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);
	
	PlayerInfoRef playerInfo = gameSession->GetPlayerInfo();

	cout << "[GACHA LOG] start gacha - Player ID: " << playerInfo->GetDbUserId() << ", Pool ID: " << pkt.pool_id() << std::endl;
	if (playerInfo == nullptr)
	{
		// 인증은 되었지만 플레이어 정보가 없는 경우 (예: DB 오류)
		cout << "[GACHA LOG] 플레이어 정보가 없습니다. 가챠를 진행할 수 없습니다." << std::endl;
		return false;
	}
	int32 obtainedSkinId = 0;
	
	// 가챠 전(Before)의 재화 상태를 기억.
	int32 gemBefore = playerInfo->GetGem();
	int32 coinBefore = playerInfo->GetCoin();

	std::cout << "[GACHA LOG] player owned goods -> Gem: " << gemBefore << ", Coin: " << coinBefore << std::endl;

	bool isSuccess = GGACHA.ExecuteGacha(playerInfo, pkt.pool_id(), obtainedSkinId);

	// 5. 결과를 클라이언트에게 알려주기 위한 패킷 조립
	Protocol::S_GACHA resPkt;
	resPkt.set_success(isSuccess);

	if (isSuccess)
	{
		std::cout << "[GACHA LOG] 가챠 성공! 획득한 스킨 ID = " << obtainedSkinId << std::endl;
		Protocol::GachaResult* result = resPkt.mutable_result();

		int32 gemAfter = playerInfo->GetGem();
		result->set_gems_spent(gemBefore - gemAfter);
		result->set_remaining_gems(gemAfter);

		// 2. SkinInfo 리스트에 새 스킨 추가
		playerInfo->AddSkin(obtainedSkinId, std::time(nullptr), false); // 획득한 스킨을 플레이어 정보에 추가 (DB 저장은 별도 처리 필요)

		Protocol::SkinInfo* skinInfo = result->add_obtained_skins();
		skinInfo->set_skin_id(obtainedSkinId);

		// 3. 스킨 메타데이터 조회 (캐시)
		const SkinMetaData* skinMeta = GGACHA.GetSkinMetaData(obtainedSkinId);

		if (skinMeta != nullptr)
		{
			// DB에서 읽어온 실제 다국어 이름/설명/레어도를 세팅합니다.
			skinInfo->set_skin_name(skinMeta->name);
			skinInfo->set_skin_des(skinMeta->description);
			skinInfo->set_rarity(skinMeta->rarity);
		}
		else
		{
			skinInfo->set_skin_name("Unknown Skin");
			skinInfo->set_skin_des("정보를 찾을 수 없습니다.");
			skinInfo->set_rarity(1);
		}
	}
	else
	{
		// (선택) 에러 코드를 보내고 싶다면 추가적으로 패킷에 세팅
		cout << "[GACHA LOG] Fail" << endl;
		resPkt.set_success(false);
		resPkt.set_error_msg("fail to send");
	}

	// 6. 패킷 전송
	SendBufferRef sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);
	session->Send(sendBuffer);

	return false;
}

bool Handle_C_GACHA_POOL_LIST(PacketSessionRef& session, Protocol::C_GACHA_POOL_LIST& pkt)
{
	GameSessionRef gameSession = static_pointer_cast<GameSession>(session);

	if (gameSession->GetPlayerInfo() == nullptr)
	{
		cout << "PlayerInfo is null. Cannot retrieve gacha pool list." << endl;
		return false;
	}

	
	Protocol::S_GACHA_POOL_LIST resPkt;
	GGACHA.FillGachaPoolListPacket(resPkt);
	
	SendBufferRef sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_MY_SKINS(PacketSessionRef& session, Protocol::C_MY_SKINS& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);

	PlayerInfoRef playerInfo = gameSession->GetPlayerInfo();

	Protocol::S_MY_SKINS resPkt;
	for (const auto& skin : playerInfo->GetOwnedSkins())
	{
		Protocol::SkinInfo* skinInfo = resPkt.add_skins();
		skinInfo->set_skin_id(skin.first);
		const SkinMetaData* skinMeta = GGACHA.GetSkinMetaData(skin.first);

		if (skinMeta != nullptr)
		{
			skinInfo->set_skin_name(skinMeta->name);
			skinInfo->set_skin_des(skinMeta->description);
			skinInfo->set_rarity(skinMeta->rarity);

			cout << "Owned Skin - ID: " << skin.first << ", Name: " << skinMeta->name << ", Rarity: " << skinMeta->rarity << endl;
		}
		else
		{
			skinInfo->set_skin_name("Unknown Skin");
			skinInfo->set_skin_des("정보를 찾을 수 없습니다.");
			skinInfo->set_rarity(1);
		}
	}
	// 6. 패킷 전송
	SendBufferRef sendBuffer = ClientPacketHandler::MakeSendBuffer(resPkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_CREATE_ROOM(PacketSessionRef& session, Protocol::C_CREATE_ROOM& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);

	// 이미 방에 있는지 체크
	if (auto currentRoom = gameSession->GetRoom().lock())
	{
		std::cout << "Create room failed: Already in a room" << endl;
		SendCreateRoomError(session, "Already in a room");
		return false;
	}

	// 방 생성
	int64 hostId = gameSession->GetPlayer()->playerId;
	string hostName = gameSession->GetPlayer()->name;
	int32 hostTag = gameSession->GetPlayer()->tag;
	RoomRef newRoom = RoomManager::Instance().CreateRoom(hostId, hostName, hostTag);

	gameSession->SetRoom(newRoom);
	newRoom->DoAsync(&Room::EnterLobby, gameSession->GetPlayer());

	// 패킷 만들기
	Protocol::S_CREATE_ROOM createRoomPkt;
	createRoomPkt.set_success(true);

	Protocol::RoomInfo* roomInfo = createRoomPkt.mutable_room();
	roomInfo->set_room_id(newRoom->GetRoomId());
	roomInfo->set_current_count(1);
	roomInfo->set_max_count(4);
	roomInfo->set_is_playing(false);


	// host 정보 세팅
	Protocol::Player* host = roomInfo->mutable_host();
	host->set_id(gameSession->GetPlayer()->playerId);
	host->set_name(gameSession->GetPlayer()->name);
	host->set_tag(gameSession->GetPlayer()->tag);

	// 방 생성 결과 패킷 전송
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(createRoomPkt);
	newRoom->DoAsync([session, sendBuffer]() {
		session->Send(sendBuffer);
		});
	
	return true;
}

bool Handle_C_ROOM_LIST(PacketSessionRef& session, Protocol::C_ROOM_LIST& pkt)
{
	return false;
}

bool Handle_C_ENTER_ROOM(PacketSessionRef& session, Protocol::C_ENTER_ROOM& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);
	// 이미 방에 있는지 체크
	if (auto currentRoom = gameSession->GetRoom().lock())
	{
		std::cout << "Enter room failed: Already in a room" << endl;
		SendEnterGameError(session, "Already in a room");
		return false;
	}
	uint64 roomId = pkt.room_id();
	RoomRef targetRoom = RoomManager::Instance().FindRoom(roomId);

	if (!targetRoom)
	{
		std::cout << "Enter room failed: Room not found - ID: " << roomId << endl;

		Protocol::S_ENTER_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg("Room not found");
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
		return false;
	}
	// 방이 꽉 찼는지 체크
	if (targetRoom->GetCurrentCount() >= targetRoom->GetMaxCount())
	{
		std::cout << "Enter room failed: Room is full" << endl;

		Protocol::S_ENTER_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg("Room is full");
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
		return false;
	}

	// 게임이 이미 시작되었는지 체크
	if (targetRoom->IsPlaying())
	{
		std::cout << "Enter room failed: Game already started" << endl;

		Protocol::S_ENTER_ROOM errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_error_msg("Game already started");
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
		return false;
	}

	// 방에 입장
	gameSession->GetRoom() = targetRoom;
	targetRoom->DoAsync(&Room::EnterLobby, gameSession->GetPlayer());

	// 성공 응답 패킷 구성
	Protocol::S_ENTER_ROOM enterRoomPkt;
	targetRoom->MakeEnterRoomPacket(gameSession, enterRoomPkt);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterRoomPkt);
	targetRoom->DoAsync([session, sendBuffer]() {
		session->Send(sendBuffer);
		});

	std::cout << "Player " << gameSession->GetPlayer()->name
		<< " entered room " << roomId << endl;

	return true;
}

bool Handle_C_LEAVE_ROOM(PacketSessionRef& session, Protocol::C_LEAVE_ROOM& pkt)
{
	auto gameSession = static_pointer_cast<GameSession>(session);

	Protocol::S_LEAVE_ROOM responsePkt;
	// 방에 속해있는지 확인
	auto room = gameSession->GetRoom().lock();
	responsePkt.set_player_id(gameSession->GetPlayer()->playerId);

	if (!room)
	{
		responsePkt.set_success(false);
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(responsePkt);
		session->Send(sendBuffer);
		return false;
	}

	// 방에서 나가기 처리
	room->DoAsync(&Room::LeaveLobby, gameSession->GetPlayer());

	// 세션의 방 정보 초기화
	gameSession->SetRoom(weak_ptr<Room>());

	responsePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(responsePkt);
	session->Send(sendBuffer);

	cout << "[Room] Player " << gameSession->GetPlayer()->name << " left the room." << endl;
	cout << "[Room] Current player count: " << room->GetCurrentCount() << endl;
	return false;
}

bool Handle_C_INVITE_PLAYER(PacketSessionRef& session, Protocol::C_INVITE_PLAYER& pkt)
{
	// 여기 나중에 수정
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);

	// 방에 있는지 체크
	auto room = gameSession->GetRoom().lock();
	if (!room)
	{
		std::cout << "Invite failed: you are not in the room" << endl;
		Protocol::S_INVITE_PLAYER errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_player_name(pkt.player_name());
		errorPkt.set_player_tag(pkt.player_tag());
		errorPkt.set_error_msg("Not in a room");
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
		return false;
	}

	// 방장인지 확인
	std::cout << "room->GetOwnerId() : " << room->GetOwnerId() << "playerId : " << gameSession->GetPlayer()->playerId << endl;
	if (room->GetOwnerId() != gameSession->GetPlayer()->playerId)
	{
		std::cout << "Invite failed: Only room owner can invite" << endl;
		Protocol::S_INVITE_PLAYER errorPkt;
		errorPkt.set_success(false);
		errorPkt.set_player_name(pkt.player_name());
		errorPkt.set_player_tag(pkt.player_tag());
		errorPkt.set_error_msg("Only room owner can invite");
		auto sendBuffer = ClientPacketHandler::MakeSendBuffer(errorPkt);
		session->Send(sendBuffer);
		return false;
	}

	string targetName = pkt.player_name();
	int32 targetTag = pkt.player_tag();

	std::cout << "Invite request from " << gameSession->GetPlayer()->name
		<< " to " << targetName << "#" << targetTag << endl;

	// InviteManager를 통해 초대 생성 및 전송
	uint64 inviteId = InviteManager::Instance().CreateInvite(
		room->GetRoomId(),
		room->GetRoomName(),
		gameSession->GetPlayer()->playerId,
		gameSession->GetPlayer()->name,
		targetName,
		targetTag
	);

	Protocol::S_INVITE_PLAYER responsePkt;

	if (inviteId == 0)
	{
		// 초대 실패
		responsePkt.set_success(false);
		responsePkt.set_player_name(targetName);
		responsePkt.set_player_tag(targetTag);
		responsePkt.set_error_msg("Player not found or already in room");
		std::cout << "Invite failed: Target not found or busy" << endl;
	}
	else
	{
		// 초대 성공
		responsePkt.set_success(true);
		responsePkt.set_player_name(targetName);
		responsePkt.set_player_tag(targetTag);
		std::cout << "Invite sent successfully - Invite ID: " << inviteId << endl;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(responsePkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_INVITE_RESPONSE(PacketSessionRef& session, Protocol::C_INVITE_RESPONSE& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);

	uint64 inviteId = pkt.invite_id();
	bool accept = pkt.accept();

	std::cout << "Invite response from " << gameSession->GetPlayer()->name
		<< " - Invite ID: " << inviteId
		<< ", Accept: " << (accept ? "Yes" : "No") << endl;

	Protocol::S_INVITE_RESPONSE responsePkt;

	if (accept)
	{
		// 초대 수락
		bool success = InviteManager::Instance().AcceptInvite(inviteId, gameSession);
		responsePkt.set_success(success);

		if (!success)
		{
			responsePkt.set_error_msg("Failed to join room (invite expired or room full)");
		}
		else
		{
			// 추가: 방 입장 성공 시 방 정보 전송
			auto room = gameSession->GetRoom().lock();
			if (room)
			{
				Protocol::S_ENTER_ROOM enterRoomPkt;
				room->MakeEnterRoomPacket(gameSession, enterRoomPkt);
				auto enterRoomBuffer = ClientPacketHandler::MakeSendBuffer(enterRoomPkt);
				session->Send(enterRoomBuffer);
			}
		}
	}
	else
	{
		// 초대 거절
		InviteManager::Instance().DeclineInvite(inviteId);
		responsePkt.set_success(true);
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(responsePkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_READY(PacketSessionRef& session, Protocol::C_READY& pkt)
{
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	// 준비 상태 변경
	bool isReady = pkt.is_ready();
	room->DoAsync(&Room::SetReady, gameSession->GetPlayer()->playerId, isReady);

	std::cout << "Player " << gameSession->GetPlayer()->name
		<< (isReady ? " is ready" : " cancelled ready") << endl;

	return true;
}

bool Handle_C_START_ROOM(PacketSessionRef& session, Protocol::C_START_ROOM& pkt)
{
	// 방에 있는지 체크
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	// 방장인지 확인
	if (room->GetOwnerId() != gameSession->GetPlayer()->playerId)
	{
		std::cout << "Start room failed: Only room owner" << endl;
		SendStartRoomError(session, "Only room owner can start game");
		return false;
	}

	// 모두 준비되었는지 확인
	if (!room->CanStartGame())
	{
		std::cout << "Start room failed: Not all players are ready" << endl;
		SendStartRoomError(session, "Not all players are ready");
		return false;
	}

	// 게임 시작
	room->DoAsync(&Room::StartGame);

	std::cout << "Room " << room->GetRoomId() << " game started by "
		<< gameSession->GetPlayer()->name << endl;

	return true;
}

bool Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	auto room = gameSession->GetRoom().lock();

	if (!room)
	{
		std::cout << "Enter game failed: Room not available" << endl;
		SendEnterGameError(session, "Room not available");
		return false;
	}

	room->DoAsync(&Room::EnterGame, gameSession->GetPlayer());

	Protocol::S_ENTER_GAME enterGamePkt;
	enterGamePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_TEST_ENTER_GAME(PacketSessionRef& session, Protocol::C_TEST_ENTER_GAME& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	// 테스트 코드 부분
	auto room = GetGlobalTestRoom();
	gameSession->GetRoom() = room;

	//
	if (!room)
	{
		std::cout << "Enter game failed: Room not available" << endl;
		SendEnterGameError(session, "Room not available");
		return false;
	}

	room->DoAsync(&Room::EnterGame, gameSession->GetPlayer());

	Protocol::S_ENTER_GAME enterGamePkt;
	enterGamePkt.set_success(true);
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_SHOW_STAGE(PacketSessionRef& session, Protocol::C_SHOW_STAGE& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	Protocol::S_SHOW_STAGE showStagePkt;
	showStagePkt.set_success(true);
	auto stagePkt = showStagePkt.mutable_stage();
	
	// map_id를 가져오기
	int32 map_id = pkt.map_id();
	int32 chapter = pkt.chapter();
	int32 stage = pkt.stageindex();

	// stage cache에서 정보 가져오기
	auto stageData = StageManager::GetInstance().GetStageInfo(map_id, chapter, stage);
	if (stageData)
	{
		stagePkt->set_map_id(map_id);
		stagePkt->set_chapter(chapter);
		stagePkt->set_stage(stage);
		stagePkt->set_stage_name(stageData->mapName);
		stagePkt->set_description(stageData->mapDescription);
		stagePkt->set_difficulty(stageData->difficulty);
		stagePkt->set_isbossstage(stageData->isBoss);
		stagePkt->set_estimated_clear_time(stageData->estimated_clearTime);
	}
	else
	{
		showStagePkt.set_success(false);
		cout << "error: stage cache miss" << endl;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(showStagePkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_START_STAGE(PacketSessionRef& session, Protocol::C_START_STAGE& pkt)
{
	return false;
}

bool Handle_C_GET_CLEAR_INFO(PacketSessionRef& session, Protocol::C_GET_CLEAR_INFO& pkt)
{
	CHECK_AUTH_LOGIN(session, SendCreateRoomError);

	int32 playerId = gameSession->GetPlayer()->playerId;

	// DB에서 클리어 정보 조회
	xvector<StageClearInfo> clearDataList;
	bool success = StageManager::GetInstance().GetMyStageClearInfo(playerId, clearDataList);

	Protocol::S_GET_CLEAR_INFO clearInfoPkt;
	clearInfoPkt.set_success(success);

	if (success)
	{
		// 클리어 정보를 패킷에 담기
		for (const auto& clearData : clearDataList)
		{
			Protocol::StageClearInfo* clearInfo = clearInfoPkt.add_stage_clears();
			clearInfo->set_map_id(clearData.stageId);
			clearInfo->set_star(clearData.star);
			clearInfo->set_clear_time(clearData.clearTime);
		}

		std::cout << "Player " << gameSession->GetPlayer()->name
			<< " clear info loaded: " << clearDataList.size() << " records" << endl;
	}
	else
	{
		std::cout << "Failed to load clear info for player: " << playerId << endl;
	}

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(clearInfoPkt);
	session->Send(sendBuffer);

	return true;
}

bool Handle_C_CHAT(PacketSessionRef& session, Protocol::C_CHAT& pkt)
{
	// 방에 있는지 체크
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	std::cout << pkt.msg() << endl;

	Protocol::S_CHAT chatPkt;
	chatPkt.set_msg(pkt.msg());
	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(chatPkt);

	room->DoAsync(&Room::Broadcast, sendBuffer);

	return true;
}

bool Handle_C_MOVE(PacketSessionRef& session, Protocol::C_MOVE& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	// 플레이어가 로그인되어 있는지 확인
	if (gameSession->GetPlayer() == nullptr)
	{
		std::cout << "Move packet from non-logged in player!" << endl;
		return false;
	}

	// 플레이어 위치 업데이트 (메모리)
	gameSession->GetPlayer()->posX = pkt.pos().x();
	gameSession->GetPlayer()->posY = pkt.pos().y();
	gameSession->GetPlayer()->posZ = pkt.pos().z();

	gameSession->GetPlayer()->rotX = pkt.rot().x();
	gameSession->GetPlayer()->rotY = pkt.rot().y();
	gameSession->GetPlayer()->rotZ = pkt.rot().z();
	gameSession->GetPlayer()->rotW = pkt.rot().w();

	// 같은 방에 있는 플레이어들에게 브로드캐스트
	Protocol::S_MOVE movePkt;
	movePkt.set_playerid(gameSession->GetPlayer()->playerId);

	auto pos = movePkt.mutable_pos();
	pos->set_x(pkt.pos().x());
	pos->set_y(pkt.pos().y());
	pos->set_z(pkt.pos().z());

	auto rot = movePkt.mutable_rot();
	rot->set_x(pkt.rot().x());
	rot->set_y(pkt.rot().y());
	rot->set_z(pkt.rot().z());
	rot->set_w(pkt.rot().w());

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(movePkt);

	// 같은 방의 모든 플레이어에게 전송
	auto roomPtr = gameSession->GetRoom().lock();
	if (roomPtr != nullptr)
	{
		roomPtr->DoAsync(&Room::Broadcast, sendBuffer);
	}
	else
	{
		std::cout << "Room not found for player: " << gameSession->GetPlayer()->name << endl;
	}

	return true;
}

bool Handle_C_PLAYER_ANIMATION(PacketSessionRef& session, Protocol::C_PLAYER_ANIMATION& pkt)
{
	GameSessionRef gameSession = std::static_pointer_cast<GameSession>(session);

	gameSession->GetPlayer()->animState = pkt.state();
	Protocol::S_PLAYER_ANIMATION animationPkt;
	animationPkt.set_playerid(gameSession->GetPlayer()->playerId);
	animationPkt.set_state(pkt.state());

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(animationPkt);
	auto roomPtr = gameSession->GetRoom().lock();

	if (roomPtr != nullptr)
	{
		roomPtr->DoAsync(&Room::Broadcast, sendBuffer);
	}

	return true;
}

bool Handle_C_PLAYER_STAT_EVENT(PacketSessionRef& session, Protocol::C_PLAYER_STAT_EVENT& pkt)
{
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);
}

bool Handle_C_OBJECT_PICKUP(PacketSessionRef& session, Protocol::C_OBJECT_PICKUP& pkt)
{
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	// 픽업 브로드캐스트
	Protocol::S_OBJECT_PICKUP pickupPkt;
	pickupPkt.set_success(true);

	// ObjectId 복사
	Protocol::ObjectId* objId = pickupPkt.mutable_object_id();
	objId->CopyFrom(pkt.object_id());

	pickupPkt.set_player_id(gameSession->GetPlayer()->playerId);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(pickupPkt);
	room->DoAsync(&Room::Broadcast, sendBuffer);

	std::cout << "Player " << gameSession->GetPlayer()->name
		<< " picked up object" << endl;

	return true;
}

bool Handle_C_OBJECT_DROP(PacketSessionRef& session, Protocol::C_OBJECT_DROP& pkt)
{
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);
	// 드롭 브로드캐스트
	Protocol::S_OBJECT_DROP dropPkt;

	// ObjectId 복사
	Protocol::ObjectId* objId = dropPkt.mutable_object_id();
	objId->CopyFrom(pkt.object_id());

	dropPkt.set_player_id(gameSession->GetPlayer()->playerId);

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(dropPkt);
	room->DoAsync(&Room::Broadcast, sendBuffer);

	std::cout << "Player " << gameSession->GetPlayer()->name
		<< " dropped object" << endl;

	return true;
}

bool Handle_C_OBJECT_MOVE(PacketSessionRef& session, Protocol::C_OBJECT_MOVE& pkt)
{
	CHECK_AUTH_ROOM(session, OUT room, SendStartRoomError);

	// 그대로 브로드캐스트 (플레이어 이동과 동일)
	Protocol::S_OBJECT_MOVE movePkt;
	Protocol::ObjectId* objId = movePkt.mutable_object_id();
	objId->CopyFrom(pkt.object_id());  // 받은 그대로 복사

	*movePkt.mutable_pos() = pkt.pos();
	*movePkt.mutable_rot() = pkt.rot();

	auto sendBuffer = ClientPacketHandler::MakeSendBuffer(movePkt);
	room->DoAsync(&Room::Broadcast, sendBuffer);

	return true;
}
