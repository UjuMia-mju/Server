#include "pch.h"
#include "ServerPacketHandler.h"
#include "BufferReader.h"
#include "ServerSession.h"

PacketHandleFunc GPacketHandler[UINT16_MAX];
static int room_id = 0;


bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len)
{
	PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
	// TODO Log
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
		cout << u8"[가챠 대성공] 결과를 확인하세요!\n";

		const auto& result = pkt.result();
		cout << u8"소모한 젬: " << result.gems_spent() << u8" / 남은 젬: " << result.remaining_gems() << "\n\n";

		// 얻은 스킨 리스트 출력
		for (int i = 0; i < result.obtained_skins_size(); ++i)
		{
			const auto& skin = result.obtained_skins(i);
			cout << u8"  [당첨] 스킨 ID: " << skin.skin_id() << "\n";
			cout << u8"      이름: " << skin.skin_name() << "\n";
			cout << u8"      설명: " << skin.skin_des() << "\n";
			cout << u8"      등급: " << skin.rarity() << "\n";
		}
		cout << "============================================\n";
	}
	else
	{
		cout << "\nu8[가챠 실패] " << pkt.error_msg() << "\n";
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
	this_thread::sleep_for(1s);

	Protocol::C_INVITE_RESPONSE responsePkt;
	responsePkt.set_invite_id(pkt.invite_id());
	responsePkt.set_accept(true);
	room_id = pkt.room_id(); // 나중에 게임 입장할 때 사용

	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(responsePkt);
	session->Send(sendBuffer);

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
	cout << "[Room] Player " << pkt.player_id()
		<< (pkt.is_ready() ? " is ready" : " cancelled ready") << endl;
	return true;
}

bool Handle_S_START_ROOM(PacketSessionRef& session, Protocol::S_START_ROOM& pkt)
{
	if (!pkt.success())
	{
		cout << "[Client] Start room failed: " << pkt.error_msg() << endl;
		return false;
	}
	Protocol::C_ENTER_GAME enterGamePkt;
	enterGamePkt.set_playerindex(0);
	auto sendBuffer = ServerPacketHandler::MakeSendBuffer(enterGamePkt);
	session->Send(sendBuffer);
	cout << "[Client] ========== GAME STARTED! ==========" << endl;
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

	return true;
}

bool Handle_S_START_STAGE(PacketSessionRef& session, Protocol::S_START_STAGE& pkt)
{
	return false;
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
	return true;
}

bool Handle_S_STAGE_INFO(PacketSessionRef& session, Protocol::S_STAGE_INFO& pkt)
{
	cout << "\n========== Stage Info ==========" << endl;
	auto stageInfo = pkt.stages();
	for (int i = 0; i < stageInfo.size(); i++)
	{
		const auto& stage = stageInfo[i];
		cout << "Stage " << stage.map_id() << "-"
			<< stage.stage_name() << ": " << stage.description() << endl;
	}

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
