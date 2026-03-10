#pragma once
#include "Protocol.pb.h"

using PacketHandleFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandleFunc GPacketHandler[UINT16_MAX];

enum : uint16 
{
	PKT_C_LOGIN = 1000,
	PKT_S_LOGIN = 1001,
	PKT_C_CREATE_ROOM = 1002,
	PKT_S_CREATE_ROOM = 1003,
	PKT_C_ROOM_LIST = 1004,
	PKT_S_ROOM_LIST = 1005,
	PKT_C_ENTER_ROOM = 1006,
	PKT_S_ENTER_ROOM = 1007,
	PKT_C_LEAVE_ROOM = 1008,
	PKT_S_LEAVE_ROOM = 1009,
	PKT_C_INVITE_PLAYER = 1010,
	PKT_S_INVITE_PLAYER = 1011,
	PKT_S_INVITE_NOTIFICATION = 1012,
	PKT_C_INVITE_RESPONSE = 1013,
	PKT_S_INVITE_RESPONSE = 1014,
	PKT_S_ROOM_MEMBER_ENTER = 1015,
	PKT_S_ROOM_MEMBER_LEAVE = 1016,
	PKT_C_READY = 1017,
	PKT_S_READY = 1018,
	PKT_C_START_ROOM = 1019,
	PKT_S_START_ROOM = 1020,
	PKT_C_CHAT = 1021,
	PKT_S_CHAT = 1022,
	PKT_C_ENTER_GAME = 1023,
	PKT_S_ENTER_GAME = 1024,
	PKT_C_SHOW_STAGE = 1025,
	PKT_S_SHOW_STAGE = 1026,
	PKT_C_START_STAGE = 1027,
	PKT_S_START_STAGE = 1028,
	PKT_C_GET_CLEAR_INFO = 1029,
	PKT_S_GET_CLEAR_INFO = 1030,
	PKT_C_MOVE = 1031,
	PKT_S_MOVE = 1032,
	PKT_S_PLAYER_LIST = 1033,
	PKT_S_PLAYER_ENTER = 1034,
	PKT_S_PLAYER_LEAVE = 1035,
	PKT_C_ANIMATION = 1036,
	PKT_S_ANIMATION = 1037,
//  EXAMPLE:
//	PKT_S_TEST = 1,
//	PKT_S_LOGIN = 2,
};


// Custom Handlers
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt);
bool Handle_S_CREATE_ROOM(PacketSessionRef& session, Protocol::S_CREATE_ROOM& pkt);
bool Handle_S_ROOM_LIST(PacketSessionRef& session, Protocol::S_ROOM_LIST& pkt);
bool Handle_S_ENTER_ROOM(PacketSessionRef& session, Protocol::S_ENTER_ROOM& pkt);
bool Handle_S_LEAVE_ROOM(PacketSessionRef& session, Protocol::S_LEAVE_ROOM& pkt);
bool Handle_S_INVITE_PLAYER(PacketSessionRef& session, Protocol::S_INVITE_PLAYER& pkt);
bool Handle_S_INVITE_NOTIFICATION(PacketSessionRef& session, Protocol::S_INVITE_NOTIFICATION& pkt);
bool Handle_S_INVITE_RESPONSE(PacketSessionRef& session, Protocol::S_INVITE_RESPONSE& pkt);
bool Handle_S_ROOM_MEMBER_ENTER(PacketSessionRef& session, Protocol::S_ROOM_MEMBER_ENTER& pkt);
bool Handle_S_ROOM_MEMBER_LEAVE(PacketSessionRef& session, Protocol::S_ROOM_MEMBER_LEAVE& pkt);
bool Handle_S_READY(PacketSessionRef& session, Protocol::S_READY& pkt);
bool Handle_S_START_ROOM(PacketSessionRef& session, Protocol::S_START_ROOM& pkt);
bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt);
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt);
bool Handle_S_SHOW_STAGE(PacketSessionRef& session, Protocol::S_SHOW_STAGE& pkt);
bool Handle_S_START_STAGE(PacketSessionRef& session, Protocol::S_START_STAGE& pkt);
bool Handle_S_GET_CLEAR_INFO(PacketSessionRef& session, Protocol::S_GET_CLEAR_INFO& pkt);
bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt);
bool Handle_S_PLAYER_LIST(PacketSessionRef& session, Protocol::S_PLAYER_LIST& pkt);
bool Handle_S_PLAYER_ENTER(PacketSessionRef& session, Protocol::S_PLAYER_ENTER& pkt);
bool Handle_S_PLAYER_LEAVE(PacketSessionRef& session, Protocol::S_PLAYER_LEAVE& pkt);
bool Handle_S_ANIMATION(PacketSessionRef& session, Protocol::S_ANIMATION& pkt);


class ServerPacketHandler
{
public:
	static void Init()
	{
		for (int32 i = 0; i < UINT16_MAX; i++)
		{
			GPacketHandler[i] = Handle_INVALID;
		}
		GPacketHandler[PKT_S_LOGIN] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_LOGIN>(Handle_S_LOGIN, session, buffer, len);
		};
		GPacketHandler[PKT_S_CREATE_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_CREATE_ROOM>(Handle_S_CREATE_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_S_ROOM_LIST] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ROOM_LIST>(Handle_S_ROOM_LIST, session, buffer, len);
		};
		GPacketHandler[PKT_S_ENTER_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ENTER_ROOM>(Handle_S_ENTER_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_S_LEAVE_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_LEAVE_ROOM>(Handle_S_LEAVE_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_S_INVITE_PLAYER] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_INVITE_PLAYER>(Handle_S_INVITE_PLAYER, session, buffer, len);
		};
		GPacketHandler[PKT_S_INVITE_NOTIFICATION] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_INVITE_NOTIFICATION>(Handle_S_INVITE_NOTIFICATION, session, buffer, len);
		};
		GPacketHandler[PKT_S_INVITE_RESPONSE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_INVITE_RESPONSE>(Handle_S_INVITE_RESPONSE, session, buffer, len);
		};
		GPacketHandler[PKT_S_ROOM_MEMBER_ENTER] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ROOM_MEMBER_ENTER>(Handle_S_ROOM_MEMBER_ENTER, session, buffer, len);
		};
		GPacketHandler[PKT_S_ROOM_MEMBER_LEAVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ROOM_MEMBER_LEAVE>(Handle_S_ROOM_MEMBER_LEAVE, session, buffer, len);
		};
		GPacketHandler[PKT_S_READY] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_READY>(Handle_S_READY, session, buffer, len);
		};
		GPacketHandler[PKT_S_START_ROOM] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_START_ROOM>(Handle_S_START_ROOM, session, buffer, len);
		};
		GPacketHandler[PKT_S_CHAT] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_CHAT>(Handle_S_CHAT, session, buffer, len);
		};
		GPacketHandler[PKT_S_ENTER_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ENTER_GAME>(Handle_S_ENTER_GAME, session, buffer, len);
		};
		GPacketHandler[PKT_S_SHOW_STAGE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_SHOW_STAGE>(Handle_S_SHOW_STAGE, session, buffer, len);
		};
		GPacketHandler[PKT_S_START_STAGE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_START_STAGE>(Handle_S_START_STAGE, session, buffer, len);
		};
		GPacketHandler[PKT_S_GET_CLEAR_INFO] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_GET_CLEAR_INFO>(Handle_S_GET_CLEAR_INFO, session, buffer, len);
		};
		GPacketHandler[PKT_S_MOVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_MOVE>(Handle_S_MOVE, session, buffer, len);
		};
		GPacketHandler[PKT_S_PLAYER_LIST] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_PLAYER_LIST>(Handle_S_PLAYER_LIST, session, buffer, len);
		};
		GPacketHandler[PKT_S_PLAYER_ENTER] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_PLAYER_ENTER>(Handle_S_PLAYER_ENTER, session, buffer, len);
		};
		GPacketHandler[PKT_S_PLAYER_LEAVE] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_PLAYER_LEAVE>(Handle_S_PLAYER_LEAVE, session, buffer, len);
		};
		GPacketHandler[PKT_S_ANIMATION] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ANIMATION>(Handle_S_ANIMATION, session, buffer, len);
		};
	}

	static bool HandlePacket(PacketSessionRef& session, BYTE* buffer, int32 len)
	{
		PacketHeader* header = reinterpret_cast<PacketHeader*>(buffer);
		return GPacketHandler[header->id](session, buffer, len);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_LOGIN& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_LOGIN);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_CREATE_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_CREATE_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_ROOM_LIST& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_ROOM_LIST);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_ENTER_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_LEAVE_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_LEAVE_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_INVITE_PLAYER& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_INVITE_PLAYER);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_INVITE_RESPONSE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_INVITE_RESPONSE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_READY& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_READY);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_START_ROOM& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_START_ROOM);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_CHAT& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_CHAT);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_GAME& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_ENTER_GAME);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_SHOW_STAGE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_SHOW_STAGE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_START_STAGE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_START_STAGE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_GET_CLEAR_INFO& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_GET_CLEAR_INFO);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_MOVE& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_MOVE);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_ANIMATION& pkt)
	{
		return MakeSendBuffer(pkt, PKT_C_ANIMATION);
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