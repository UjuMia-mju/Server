#pragma once
#include "Protocol.pb.h"

using PacketHandleFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandleFunc GPacketHandler[UINT16_MAX];

enum : uint16 
{
	PKT_C_LOGIN = 1000,
	PKT_S_LOGIN = 1001,
	PKT_C_GACHA = 1002,
	PKT_S_GACHA = 1003,
	PKT_C_GACHA_POOL_LIST = 1004,
	PKT_S_GACHA_POOL_LIST = 1005,
	PKT_C_MY_SKINS = 1006,
	PKT_S_MY_SKINS = 1007,
	PKT_C_CREATE_ROOM = 1008,
	PKT_S_CREATE_ROOM = 1009,
	PKT_C_ROOM_LIST = 1010,
	PKT_S_ROOM_LIST = 1011,
	PKT_C_ENTER_ROOM = 1012,
	PKT_S_ENTER_ROOM = 1013,
	PKT_C_LEAVE_ROOM = 1014,
	PKT_S_LEAVE_ROOM = 1015,
	PKT_C_INVITE_PLAYER = 1016,
	PKT_S_INVITE_PLAYER = 1017,
	PKT_S_INVITE_NOTIFICATION = 1018,
	PKT_C_INVITE_RESPONSE = 1019,
	PKT_S_INVITE_RESPONSE = 1020,
	PKT_S_ROOM_MEMBER_ENTER = 1021,
	PKT_S_ROOM_MEMBER_LEAVE = 1022,
	PKT_C_READY = 1023,
	PKT_S_READY = 1024,
	PKT_C_START_ROOM = 1025,
	PKT_S_START_ROOM = 1026,
	PKT_C_CHAT = 1027,
	PKT_S_CHAT = 1028,
	PKT_C_ENTER_GAME = 1029,
	PKT_S_ENTER_GAME = 1030,
	PKT_C_TEST_ENTER_GAME = 1031,
	PKT_C_SHOW_STAGE = 1032,
	PKT_S_SHOW_STAGE = 1033,
	PKT_C_START_STAGE = 1034,
	PKT_S_START_STAGE = 1035,
	PKT_C_GET_CLEAR_INFO = 1036,
	PKT_S_GET_CLEAR_INFO = 1037,
	PKT_C_MOVE = 1038,
	PKT_S_MOVE = 1039,
	PKT_S_PLAYER_LIST = 1040,
	PKT_S_PLAYER_ENTER = 1041,
	PKT_S_PLAYER_LEAVE = 1042,
	PKT_C_PLAYER_ANIMATION = 1043,
	PKT_S_PLAYER_ANIMATION = 1044,
	PKT_C_PLAYER_STAT_EVENT = 1045,
	PKT_S_PLAYER_STAT = 1046,
	PKT_C_OBJECT_PICKUP = 1047,
	PKT_S_OBJECT_PICKUP = 1048,
	PKT_C_OBJECT_DROP = 1049,
	PKT_S_OBJECT_DROP = 1050,
	PKT_C_OBJECT_MOVE = 1051,
	PKT_S_OBJECT_MOVE = 1052,
//  EXAMPLE:
//	PKT_S_TEST = 1,
//	PKT_S_LOGIN = 2,
};


// Custom Handlers
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_C_LOGIN(PacketSessionRef& session, Protocol::C_LOGIN& pkt);
bool Handle_C_GACHA(PacketSessionRef& session, Protocol::C_GACHA& pkt);
bool Handle_C_GACHA_POOL_LIST(PacketSessionRef& session, Protocol::C_GACHA_POOL_LIST& pkt);
bool Handle_C_MY_SKINS(PacketSessionRef& session, Protocol::C_MY_SKINS& pkt);
bool Handle_C_CREATE_ROOM(PacketSessionRef& session, Protocol::C_CREATE_ROOM& pkt);
bool Handle_C_ROOM_LIST(PacketSessionRef& session, Protocol::C_ROOM_LIST& pkt);
bool Handle_C_ENTER_ROOM(PacketSessionRef& session, Protocol::C_ENTER_ROOM& pkt);
bool Handle_C_LEAVE_ROOM(PacketSessionRef& session, Protocol::C_LEAVE_ROOM& pkt);
bool Handle_C_INVITE_PLAYER(PacketSessionRef& session, Protocol::C_INVITE_PLAYER& pkt);
bool Handle_C_INVITE_RESPONSE(PacketSessionRef& session, Protocol::C_INVITE_RESPONSE& pkt);
bool Handle_C_READY(PacketSessionRef& session, Protocol::C_READY& pkt);
bool Handle_C_START_ROOM(PacketSessionRef& session, Protocol::C_START_ROOM& pkt);
bool Handle_C_CHAT(PacketSessionRef& session, Protocol::C_CHAT& pkt);
bool Handle_C_ENTER_GAME(PacketSessionRef& session, Protocol::C_ENTER_GAME& pkt);
bool Handle_C_TEST_ENTER_GAME(PacketSessionRef& session, Protocol::C_TEST_ENTER_GAME& pkt);
bool Handle_C_SHOW_STAGE(PacketSessionRef& session, Protocol::C_SHOW_STAGE& pkt);
bool Handle_C_START_STAGE(PacketSessionRef& session, Protocol::C_START_STAGE& pkt);
bool Handle_C_GET_CLEAR_INFO(PacketSessionRef& session, Protocol::C_GET_CLEAR_INFO& pkt);
bool Handle_C_MOVE(PacketSessionRef& session, Protocol::C_MOVE& pkt);
bool Handle_C_PLAYER_ANIMATION(PacketSessionRef& session, Protocol::C_PLAYER_ANIMATION& pkt);
bool Handle_C_PLAYER_STAT_EVENT(PacketSessionRef& session, Protocol::C_PLAYER_STAT_EVENT& pkt);
bool Handle_C_OBJECT_PICKUP(PacketSessionRef& session, Protocol::C_OBJECT_PICKUP& pkt);
bool Handle_C_OBJECT_DROP(PacketSessionRef& session, Protocol::C_OBJECT_DROP& pkt);
bool Handle_C_OBJECT_MOVE(PacketSessionRef& session, Protocol::C_OBJECT_MOVE& pkt);


class ClientPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; i++)
		{
			GPacketHandler[i] = Handle_INVALID;
		}
		GPacketHandler[PKT_C_LOGIN] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_LOGIN>(Handle_C_LOGIN, session, buffer, len);
		};
		GPacketHandler[PKT_C_GACHA] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_GACHA>(Handle_C_GACHA, session, buffer, len);
		};
		GPacketHandler[PKT_C_GACHA_POOL_LIST] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_GACHA_POOL_LIST>(Handle_C_GACHA_POOL_LIST, session, buffer, len);
		};
		GPacketHandler[PKT_C_MY_SKINS] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_MY_SKINS>(Handle_C_MY_SKINS, session, buffer, len);
		};
		GPacketHandler[PKT_C_CREATE_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_CREATE_ROOM>(Handle_C_CREATE_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_C_ROOM_LIST] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_ROOM_LIST>(Handle_C_ROOM_LIST, session, buffer, len);
		};
		GPacketHandler[PKT_C_ENTER_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_ENTER_ROOM>(Handle_C_ENTER_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_C_LEAVE_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_LEAVE_ROOM>(Handle_C_LEAVE_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_C_INVITE_PLAYER] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_INVITE_PLAYER>(Handle_C_INVITE_PLAYER, session, buffer, len);
		};
		GPacketHandler[PKT_C_INVITE_RESPONSE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_INVITE_RESPONSE>(Handle_C_INVITE_RESPONSE, session, buffer, len);
		};
		GPacketHandler[PKT_C_READY] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_READY>(Handle_C_READY, session, buffer, len);
		};
		GPacketHandler[PKT_C_START_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_START_ROOM>(Handle_C_START_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_C_CHAT] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_CHAT>(Handle_C_CHAT, session, buffer, len);
		};
		GPacketHandler[PKT_C_ENTER_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_ENTER_GAME>(Handle_C_ENTER_GAME, session, buffer, len);
		};
		GPacketHandler[PKT_C_TEST_ENTER_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_TEST_ENTER_GAME>(Handle_C_TEST_ENTER_GAME, session, buffer, len);
		};
		GPacketHandler[PKT_C_SHOW_STAGE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_SHOW_STAGE>(Handle_C_SHOW_STAGE, session, buffer, len);
		};
		GPacketHandler[PKT_C_START_STAGE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_START_STAGE>(Handle_C_START_STAGE, session, buffer, len);
		};
		GPacketHandler[PKT_C_GET_CLEAR_INFO] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_GET_CLEAR_INFO>(Handle_C_GET_CLEAR_INFO, session, buffer, len);
		};
		GPacketHandler[PKT_C_MOVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_MOVE>(Handle_C_MOVE, session, buffer, len);
		};
		GPacketHandler[PKT_C_PLAYER_ANIMATION] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_PLAYER_ANIMATION>(Handle_C_PLAYER_ANIMATION, session, buffer, len);
		};
		GPacketHandler[PKT_C_PLAYER_STAT_EVENT] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_PLAYER_STAT_EVENT>(Handle_C_PLAYER_STAT_EVENT, session, buffer, len);
		};
		GPacketHandler[PKT_C_OBJECT_PICKUP] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_OBJECT_PICKUP>(Handle_C_OBJECT_PICKUP, session, buffer, len);
		};
		GPacketHandler[PKT_C_OBJECT_DROP] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_OBJECT_DROP>(Handle_C_OBJECT_DROP, session, buffer, len);
		};
		GPacketHandler[PKT_C_OBJECT_MOVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::C_OBJECT_MOVE>(Handle_C_OBJECT_MOVE, session, buffer, len);
		};
	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_LOGIN& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_LOGIN);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_GACHA& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_GACHA);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_GACHA_POOL_LIST& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_GACHA_POOL_LIST);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_MY_SKINS& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_MY_SKINS);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_CREATE_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_CREATE_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_ROOM_LIST& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_ROOM_LIST);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_ENTER_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_ENTER_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_LEAVE_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_LEAVE_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_INVITE_PLAYER& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_INVITE_PLAYER);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_INVITE_NOTIFICATION& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_INVITE_NOTIFICATION);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_INVITE_RESPONSE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_INVITE_RESPONSE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_ROOM_MEMBER_ENTER& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_ROOM_MEMBER_ENTER);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_ROOM_MEMBER_LEAVE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_ROOM_MEMBER_LEAVE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_READY& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_READY);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_START_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_START_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_CHAT& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_CHAT);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_ENTER_GAME& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_ENTER_GAME);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_SHOW_STAGE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_SHOW_STAGE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_START_STAGE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_START_STAGE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_GET_CLEAR_INFO& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_GET_CLEAR_INFO);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_MOVE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_MOVE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_PLAYER_LIST& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_PLAYER_LIST);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_PLAYER_ENTER& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_PLAYER_ENTER);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_PLAYER_LEAVE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_PLAYER_LEAVE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_PLAYER_ANIMATION& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_PLAYER_ANIMATION);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_PLAYER_STAT& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_PLAYER_STAT);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_OBJECT_PICKUP& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_OBJECT_PICKUP);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_OBJECT_DROP& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_OBJECT_DROP);
	}
	static SendBufferRef MakeSendBuffer(Protocol::S_OBJECT_MOVE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_S_OBJECT_MOVE);
	}
	
private:
	template<typename PacketType, typename ProcessFunc>
	static bool HandlePacket(ProcessFunc func, PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketType pkt;
		if (pkt.ParseFromArray(buffer + sizeof(PacketHeader), len - sizeof(PacketHeader)) == false)
		{
			return false;
		}
		return func(session, pkt);
	}

	template<typename T>
	static SendBufferRef MakeSendBuffer(T& pkt, uint16 pktId)
	{
		const uint16 dataSize = static_cast<uint16>(pkt.ByteSizeLong());
		const uint16 packetSize = dataSize + sizeof(PacketHeader);

		SendBufferRef sendBuffer = GSendBufferManager->Open(packetSize);

		PacketHeader* header = reinterpret_cast<PacketHeader*>(sendBuffer->Buffer());
		header->size = packetSize;
		header->id = pktId;

		ASSERT_CRASH(pkt.SerializeToArray(&header[1], dataSize));

		sendBuffer->Close(packetSize);

		return sendBuffer;
	}
};