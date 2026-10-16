#include "pch.h"
#include "ServerPacketHandler.h"
#include "BufferReader.h"
#include "ServerSession.h"
#include "RelayStats.h"

PacketHandleFunc GPacketHandler[UINT16_MAX];
static int room_id = 0;


bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO Log
	return true;
}

bool Handle_S_RELAY_PACKET(PacketSessionRef& session, Protocol::S_RELAY_PACKET& pkt)
{
	RelayStats::OnReceived(pkt.payload());
	return true;
}

bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt)
{	
	if (pkt.success() == false)
	{
		return true;
	}

	cout << "[Client] Login success - Player: " << pkt.player().name()
		<< " (ID: " << pkt.player().id() << ")" << endl;

	auto serverSession = static_pointer_cast<ServerSession>(session);

	// 플레이어 정보 저장
	serverSession->SetPlayerInfo(
		pkt.player().name(),
		pkt.player().tag(),
		pkt.player().id()
	);

	cout << "ss" << endl;
	// 로그인 성공 후 유저 정보 요청
	//Protocol::C_GET_USER_INFO getUserInfoPkt;


	// 로그인 성공 후 클리어 정보 요청
	//Protocol::C_GET_CLEAR_INFO getClearInfoPkt;
	//auto sendBuffer = ServerPacketHandler::MakeSendBuffer(getClearInfoPkt);
	//session->Send(sendBuffer);

	//cout << "[Client] Requested clear info" << endl;

	return true;
}

bool Handle_S_GET_CURRENCY(PacketSessionRef& session, Protocol::S_GET_CURRENCY& pkt)
{
	return false;
}

bool Handle_S_GACHA(PacketSessionRef& session, Protocol::S_GACHA& pkt)
{
	if (pkt.success())
	{
		cout << "\n============================================\n";
		cout << "[Gacha Success] Check the results!\n";

		const auto& result = pkt.result();
		cout << "Gems spent: " << result.gems_spent() << " / Gems left: " << result.remaining_gems() << "\n\n";

		// 얻은 스킨 리스트 출력
		for (int i = 0; i < result.obtained_skins_size(); ++i)
		{
			const auto& skin = result.obtained_skins(i);
			cout << "  [Obtained] Skin ID: " << skin.skin_id() << "\n";
			cout << "      Name: " << skin.skin_name() << "\n";
			cout << "      Desc: " << skin.skin_des() << "\n";
			cout << "      Rarity: " << skin.rarity() << "\n";
		}
		cout << "============================================\n";
	}
	else
	{
		cout << "\n[Gacha Failed] " << pkt.error_msg() << "\n";
	}

	return true;
}

bool Handle_S_GACHA_POOL_LIST(PacketSessionRef& session, Protocol::S_GACHA_POOL_LIST& pkt)
{
	cout << "\n========== Gacha Pools ==========" << endl;
	
	if (pkt.pools_size() == 0)
	{
		cout << "No gacha pools available." << endl;
	}
	else
	{
		for (const auto& pool : pkt.pools())
		{
			cout << "Pool ID: " << pool.pool_id() << endl;
			cout << "Name: " << pool.pool_name() << endl;
			cout << "SKINS: " << endl;

			for (const auto& skin : pool.skins())
			{
				cout << "  - Skin ID: " << skin.skin_id() << endl;
				cout << "    Name: " << skin.skin_name() << endl;
				cout << "    Description: " << skin.skin_des() << endl;
				cout << "    Rarity: " << skin.rarity() << endl;
			}

			cout << "-----------------------------" << endl;
		}
	}

	return true;
}

bool Handle_S_SKIN_LIST(PacketSessionRef& session, Protocol::S_SKIN_LIST& pkt)
{
	cout << "\n========== All Skins ==========" << endl;
	if (pkt.skins_size() == 0)
	{
		cout << "No skins available." << endl;
	}
	else
	{
		for (const auto& skin : pkt.skins())
		{
			cout << "Skin ID: " << skin.skin_id() << endl;
			cout << "Name: " << skin.skin_name() << endl;
			cout << "Description: " << skin.skin_des() << endl;
			cout << "Rarity: " << skin.rarity() << endl;
			cout << "-----------------------------" << endl;
		}
	}

	return true;
}

bool Handle_S_MY_SKINS(PacketSessionRef& session, Protocol::S_MY_SKINS& pkt)
{
	const auto& skins = pkt.skins();
	cout << "\n========== My Skins ==========" << endl;
	if (skins.size() == 0)
	{
		cout << "No skins obtained yet." << endl;
	}
	else
	{
		for (const auto& skin : skins)
		{
			cout << "Skin ID: " << skin.skin_id() << endl;
			cout << "Name: " << skin.skin_name() << endl;
			cout << "Description: " << skin.skin_des() << endl;
			cout << "Rarity: " << skin.rarity() << endl;
			cout << "-----------------------------" << endl;
		}
	}

	return true;
}

bool Handle_S_CREATE_ROOM(PacketSessionRef& session, Protocol::S_CREATE_ROOM& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Create room failed: " << pkt.error_msg() << endl;
		return false;
	}

	cout << "[Client] Room created - ID: " << pkt.room().room_id() << endl;
	static_pointer_cast<ServerSession>(session)->SetRoomId(pkt.room().room_id());
	return true;
}

bool Handle_S_ROOM_LIST(PacketSessionRef& session, Protocol::S_ROOM_LIST& pkt)
{
	return false;
}

bool Handle_S_ENTER_ROOM(PacketSessionRef& session, Protocol::S_ENTER_ROOM& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Enter room failed: " << pkt.error_msg() << endl;
		return false;
	}

	cout << "\n========== ENTERED ROOM ==========" << endl;
	cout << "Room ID: " << pkt.room().room_id() << endl;
	cout << "Room Name: " << pkt.room().room_name() << endl;
	cout << "Players: " << pkt.room().current_count() << "/" << pkt.room().max_count() << endl;
	cout << "Room Owner: " << pkt.room().host().name() << "#" << pkt.room().host().tag() << endl;

	// 기존 멤버 리스트 출력
	if (pkt.members_size() > 0)
	{
		cout << "\nCurrent Members:" << endl;
		for (int i = 0; i < pkt.members_size(); i++)
		{
			const auto& member = pkt.members(i);
			cout << "  - " << member.player().name() << "#" << member.player().tag()
				<< (member.is_ready() ? " [READY]" : " [NOT READY]") << endl;
		}
	}

	cout << "==================================\n" << endl;

	// room_id 저장 (게임 시작 시 사용)
	room_id = pkt.room().room_id();
	static_pointer_cast<ServerSession>(session)->SetRoomId(pkt.room().room_id());

	return true;
}

bool Handle_S_LEAVE_ROOM(PacketSessionRef& session, Protocol::S_LEAVE_ROOM& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Leave room failed: " << endl;
		return false;
	}

	cout << "[Client] Successfully left the room" << endl;
	return true;
}

bool Handle_S_INVITE_PLAYER(PacketSessionRef& session, Protocol::S_INVITE_PLAYER& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Invite failed: " << pkt.error_msg() << endl;
		return false;
	}

	cout << "[Client] Invite sent to " << pkt.player_name() << "#" << pkt.player_tag() << endl;
	return true;
}

bool Handle_S_INVITE_NOTIFICATION(PacketSessionRef& session, Protocol::S_INVITE_NOTIFICATION& pkt)
{
	cout << "\n========== INVITATION RECEIVED ==========" << endl;
	cout << "From: " << pkt.inviter_name() << endl;
	cout << "Room: " << pkt.room_name() << " (ID: " << pkt.room_id() << ")" << endl;
	cout << "Invite ID: " << pkt.invite_id() << endl;
	cout << "========================================\n" << endl;

	// 자동 수락
	if (GInviteAcceptDelayMs > 0)
		this_thread::sleep_for(chrono::milliseconds(GInviteAcceptDelayMs.load()));

	Protocol::C_INVITE_RESPONSE responsePkt;
	responsePkt.set_invite_id(pkt.invite_id());
	responsePkt.set_accept(true);
	room_id = pkt.room_id(); // 나중에 게임 입장할 때 사용

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(responsePkt);
	static_pointer_cast<ServerSession>(session)->Send(sendBuffer);

	cout << "[Client] Accepted invitation" << endl;
	return true;
}

bool Handle_S_INVITE_RESPONSE(PacketSessionRef& session, Protocol::S_INVITE_RESPONSE& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Join failed: " << pkt.error_msg() << endl;
		return false;
	}

	cout << "[Client] Successfully joined the room!" << endl;
	return true;
}

bool Handle_S_ROOM_MEMBER_ENTER(PacketSessionRef& session, Protocol::S_ROOM_MEMBER_ENTER& pkt)
{
	cout << "\n[Room] " << pkt.member().player().name() << "#"
		<< pkt.member().player().tag() << " joined the room" << endl;
	return true;
}

bool Handle_S_ROOM_MEMBER_LEAVE(PacketSessionRef& session, Protocol::S_ROOM_MEMBER_LEAVE& pkt)
{
	return false;
}

bool Handle_S_READY(PacketSessionRef& session, Protocol::S_READY& pkt)
{
	static_pointer_cast<ServerSession>(session)->OnReadyChanged(pkt.player_id(), pkt.is_ready());

	cout << "[Room] Player " << pkt.player_id()
		<< (pkt.is_ready() ? " is ready" : " cancelled ready") << endl;
	return true;
}

bool Handle_S_START_ROOM(PacketSessionRef& session, Protocol::S_START_ROOM& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Start room failed123123: " << pkt.error_msg() << endl;
		return false;
	}

	// 호스트의 클리어 스테이지 정보 출력
	cout << "\n========Host Clear Stages===========:" << endl;
	if (pkt.host_clear_stages_size() > 0)
	{
		for (int i = 0; i < pkt.host_clear_stages_size(); i++)
		{
			const auto& clearInfo = pkt.host_clear_stages(i);
			cout << "  - Stage " << clearInfo.map_id()
				<< " | Stars: " << clearInfo.star()
				<< " | Clear Time: " << clearInfo.clear_time() << endl;
		}
	}
	else
	{
		cout << "  No stages cleared yet." << endl;
	}

	// 스테이지 선택 화면으로 진입. C_ENTER_GAME은 S_START_STAGE를 받은 뒤에 보냄
	static_pointer_cast<ServerSession>(session)->SetRoomStarted();
	cout << "[Client] ========== ROOM STARTED (stage select) ==========" << endl;
	return true;
}


bool Handle_S_SHOW_STAGE(PacketSessionRef& session, Protocol::S_SHOW_STAGE& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Failed to show stage: " << endl;
		return false;
	}
	cout << "\n========== Stage Info ==========" << endl;
	auto stageInfo = pkt.stage();
	cout << "Map ID: " << stageInfo.map_id() << "-" << stageInfo.stage_name() << "-" << stageInfo.description() << endl;

	static_pointer_cast<ServerSession>(session)->OnShowStageReceived();
	return true;
}

bool Handle_S_HOST_SHOW_STAGE(PacketSessionRef& session, Protocol::S_HOST_SHOW_STAGE& pkt)
{
	cout << "\n========== Host Stage Info ==========" << endl;
	auto stageInfo = pkt.stage();
	cout << "Map ID: " << stageInfo.map_id() << "-" << stageInfo.stage_name() << "-" << stageInfo.description() << endl;

	static_pointer_cast<ServerSession>(session)->SetHostStage(stageInfo.map_id());
	return true;
}

bool Handle_S_START_STAGE(PacketSessionRef& session, Protocol::S_START_STAGE& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Failed to start stage: " << endl;
		return false;
	}

	cout << "[Client] Stage started successfully!" << endl;

	// 스테이지 시작 -> 게임 씬 로드 -> 게임 입장
	static_pointer_cast<ServerSession>(session)->SendStageEnter();
	return true;
}

bool Handle_S_GET_CLEAR_INFO(PacketSessionRef& session, Protocol::S_GET_CLEAR_INFO& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Failed to get clear info" << endl;
		return false;
	}

	cout << "\n========== Player Clear Info ==========" << endl;
	cout << "Total cleared stages: " << pkt.stage_clears_size() << endl;

	for (int i = 0; i < pkt.stage_clears_size(); i++)
	{
		const auto& clearInfo = pkt.stage_clears(i);
		cout << "Stage " << clearInfo.map_id() << "-"
			<< " | Stars: " << clearInfo.star()
			<< " | Clear Time: " << clearInfo.clear_time() << endl;
	}

	cout << "======================================\n" << endl;

	return true;
}

bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Enter game failed!" << endl;
		return false;
	}

	cout << "\n========== [ENTER GAME] ==========" << endl;
	cout << "Current Players in Game: " << pkt.players_size() << endl;

	for (int i = 0; i < pkt.players_size(); ++i)
	{
		const auto& p = pkt.players(i);
		cout << "  - [" << p.player_id() << "] " << p.name()
			<< " | Pos: (" << p.pos().x() << ", " << p.pos().y() << ", " << p.pos().z() << ")" << endl;
	}
	cout << "==================================" << endl;

	return true;
}

bool Handle_S_GAME_READY_TO_START(PacketSessionRef& session, Protocol::S_GAME_READY_TO_START& pkt)
{
	static_pointer_cast<ServerSession>(session)->SetGameReady();

	int time = pkt.start_delay_seconds();	
	cout << "[Client] Game is ready to start! time : " << time << " seconds" << endl;

	return true;
}

bool Handle_S_GAME_CLEAR(PacketSessionRef& session, Protocol::S_GAME_CLEAR& pkt)
{
	int mapId = pkt.map_id();
	int star = pkt.star();
	int clearTime = pkt.clear_time_seconds();
	cout << "\n========== GAME CLEAR! ==========" << endl;
	cout << "Congratulations! You cleared the stage " << mapId << " with " << star << " stars!" << endl;
	cout << "Clear Time: " << clearTime << " seconds" << endl;

	return true;
}

bool Handle_S_ROOM_DESTROY(PacketSessionRef& session, Protocol::S_ROOM_DESTROY& pkt)
{
	static_pointer_cast<ServerSession>(session)->SetRoomId(0);
	return true;
}

bool Handle_S_STAGE_INFO(PacketSessionRef& session, Protocol::S_STAGE_INFO& pkt)
{
	cout << "\n========== Stage Info ==========" << endl;
	auto stageInfo = pkt.stages();
	vector<StageKey> stages;
	for (int i = 0; i < stageInfo.size(); i++)
	{
		const auto& stage = stageInfo[i];
		cout << "Stage " << stage.map_id() << "-"
			<< stage.stage_name() << ": " << stage.description() << endl;

		stages.push_back(StageKey{ stage.map_id(), stage.chapter(), stage.stage() });
	}

	static_pointer_cast<ServerSession>(session)->SetStageList(move(stages));
	return true;
}

bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt)
{
	return true;
}

bool Handle_S_PLAYER_LIST(PacketSessionRef& session, Protocol::S_PLAYER_LIST& pkt)
{
	return true;
}

bool Handle_S_PLAYER_ENTER(PacketSessionRef& session, Protocol::S_PLAYER_ENTER& pkt)
{
	return true;
}

bool Handle_S_PLAYER_LEAVE(PacketSessionRef& session, Protocol::S_PLAYER_LEAVE& pkt)
{
	return true;
}

bool Handle_S_PLAYER_ANIMATION(PacketSessionRef& session, Protocol::S_PLAYER_ANIMATION& pkt)
{
	return true;
}

bool Handle_S_PLAYER_STAT(PacketSessionRef& session, Protocol::S_PLAYER_STAT& pkt)
{
	return false;
}

bool Handle_S_OBJECT_PICKUP(PacketSessionRef& session, Protocol::S_OBJECT_PICKUP& pkt)
{
	return false;
}

bool Handle_S_OBJECT_DROP(PacketSessionRef& session, Protocol::S_OBJECT_DROP& pkt)
{
	return false;
}

bool Handle_S_OBJECT_MOVE(PacketSessionRef& session, Protocol::S_OBJECT_MOVE& pkt)
{
	return false;
}

bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt)
{
	cout << pkt.msg() << endl;
	return true;
}
