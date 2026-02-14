#pragma once
#include "Protocol.pb.h"

using PacketHandleFunc = std::function<bool(PacketSessionRef&, BYTE*, int32)>;
extern PacketHandleFunc GPacketHandler[UINT16_MAX];

enum : uint16 
{
	PKT_C_LOGIN = 1000,
	PKT_S_LOGIN = 1001,
	PKT_C_ENTER_GAME = 1002,
	PKT_S_ENTER_GAME = 1003,
	PKT_C_CHAT = 1004,
	PKT_S_CHAT = 1005,
	PKT_C_MOVE = 1006,
	PKT_S_MOVE = 1007,
	PKT_S_PLAYER_LIST = 1008,
	PKT_S_PLAYER_ENTER = 1009,
	PKT_S_PLAYER_LEAVE = 1010,
//  EXAMPLE:
//	PKT_S_TEST = 1,
//	PKT_S_LOGIN = 2,
};


// Custom Handlers
bool Handle_INVALID(PacketSessionRef& session, BYTE* buffer, int32 len);
bool Handle_S_LOGIN(PacketSessionRef& session, Protocol::S_LOGIN& pkt);
bool Handle_S_ENTER_GAME(PacketSessionRef& session, Protocol::S_ENTER_GAME& pkt);
bool Handle_S_CHAT(PacketSessionRef& session, Protocol::S_CHAT& pkt);
bool Handle_S_MOVE(PacketSessionRef& session, Protocol::S_MOVE& pkt);
bool Handle_S_PLAYER_LIST(PacketSessionRef& session, Protocol::S_PLAYER_LIST& pkt);
bool Handle_S_PLAYER_ENTER(PacketSessionRef& session, Protocol::S_PLAYER_ENTER& pkt);
bool Handle_S_PLAYER_LEAVE(PacketSessionRef& session, Protocol::S_PLAYER_LEAVE& pkt);


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
		GPacketHandler[PKT_S_ENTER_GAME] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_ENTER_GAME>(Handle_S_ENTER_GAME, session, buffer, len);
		};
		GPacketHandler[PKT_S_CHAT] = [](PacketSessionRef& session, BYTE* buffer, int32 len)
		{
			return HandlePacket<Protocol::S_CHAT>(Handle_S_CHAT, session, buffer, len);
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
	static SendBufferRef MakeSendBuffer(Protocol::C_ENTER_GAME& pkt)
	{

		return MakeSendBuffer(pkt, PKT_C_ENTER_GAME);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_CHAT& pkt)
	{

		return MakeSendBuffer(pkt, PKT_C_CHAT);
	}
	static SendBufferRef MakeSendBuffer(Protocol::C_MOVE& pkt)
	{

		return MakeSendBuffer(pkt, PKT_C_MOVE);
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