#include "pch.h"
#include "NetAdress.h"

/*------------------
	 NetAdress
--------------------*/

NetAddress::NetAddress(SOCKADDR_IN sockAddr) : _sockAddr(sockAddr)
{
}

NetAddress::NetAddress(wstring ip, uint16 port)
{
	::memset(&_sockAddr, 0, sizeof(SOCKADDR_IN));
	_sockAddr.sin_family = AF_INET;
	_sockAddr.sin_addr = Ip2Address(ip.c_str());
	_sockAddr.sin_port = ::htons(port);
}

wstring NetAddress::GetIpAddress()
{
	WCHAR _ipBuffer[100];
	// InetNtopW : 네트워크 주소를 문자열로 변환
	::InetNtopW(AF_INET, &_sockAddr.sin_addr, _ipBuffer, len32(_ipBuffer));
	return wstring(_ipBuffer);
}

IN_ADDR NetAddress::Ip2Address(const WCHAR* ip)
{
	IN_ADDR address;
	// InetPtonW : 문자열로 된 IP 주소를 네트워크 주소 구조체로 변환
	::InetPtonW(AF_INET, ip, &address);
	return address;
}
