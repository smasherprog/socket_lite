#include "Connector.h"

SL::NET::Connector::Connector(HANDLE *iocphandle, std::string host, PortNumber port, NetworkProtocol protocol)
    : IOCPHandle(iocphandle), Protocol(protocol), Host(host), Port(port)
{
}

SL::NET::Connector::~Connector() {}

void SL::NET::Connector::async_connect()
{
    bSuccess =
        WSAConnectByName(ConnSocket, NodeName, PortName, &dwLocalAddr, (SOCKADDR *)&LocalAddr, &dwRemoteAddr, (SOCKADDR *)&RemoteAddr, NULL, NULL);
    if (!bSuccess) {
        wprintf(L"WsaConnectByName failed with error: %d\n", WSAGetLastError());
        closesocket(ConnSocket);
        return INVALID_SOCKET;
    }
