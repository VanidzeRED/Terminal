#include <iostream>
#include <windows.h>
#include <winsock.h>
#include <string>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

HANDLE hSerial;
HANDLE File;

SOCKET s;
SOCKET clientSock;

int ReadingFlag = 1;
const int strSize = 100;
DWORD iSize;
LPCTSTR sPortName = L"COM3";

void ComPortOpen()
{
	hSerial = ::CreateFile(sPortName, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	cout << "Port opened\n";
}

void DCBParams()
{
	DCB  dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
	dcbSerialParams.BaudRate = CBR_9600;
	dcbSerialParams.ByteSize = 8;
	dcbSerialParams.StopBits = ONESTOPBIT;
	dcbSerialParams.Parity = NOPARITY;
	dcbSerialParams.fAbortOnError = TRUE;
	cout << "DCB params set\n";
}

void Ending()
{
	ReadingFlag = 0;
	CloseHandle(hSerial);
	CloseHandle(File);
	cout << "Programm finished\n";
}

char* ReadCom()
{
	OFSTRUCT Buff = { 0 };
	iSize = 0;
	LPDWORD wrSize=0;
	DWORD Code39 = 39;
	DWORD Code10038 = 10038;
	char RecivedChar[strSize];

	if ((GetLastError()) && (GetLastError() != Code10038))
	{
		if (hSerial == INVALID_HANDLE_VALUE)
		{
			ComPortOpen();
			DCBParams();
		}
		else
		{
			cout << "Some other error on reading.\n" << GetLastError() << "\n";
			if (GetLastError() == Code39)
			{
				Ending();
			}
		}
	}
	else
	{
		ReadFile(hSerial, RecivedChar, strSize, &iSize, NULL);
		cout << iSize << " bytes accept\n";
		if (iSize > 0) {
			for (int i = 0; i < iSize; i++) {
				cout << RecivedChar[i];
			}
			cout << "\n";
		}
		BOOL iRet = WriteFile(File, RecivedChar, iSize, wrSize, NULL);
	}
	return RecivedChar;
	
}

bool ConnectToHost(int PortNo, char* IPAddress)
{
	WSADATA wsadata;

	int error = WSAStartup(0x0202, &wsadata);

	if (error)
		return false;

	if (wsadata.wVersion != 0x0202)
	{
		WSACleanup();
		return false;
	}

	SOCKADDR_IN target;

	target.sin_family = AF_INET;
	target.sin_port = htons(PortNo);
	target.sin_addr.s_addr = inet_addr(IPAddress);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET)
	{
		return false;
	}


	if (connect(s, (SOCKADDR*)&target, sizeof(target)) == SOCKET_ERROR)
	{
		return false;
	}
	else
		return true;
}

bool CreateServer(int PortNo, char* IPAddress)
{
	WSADATA wsadata;

	int error = WSAStartup(0x0202, &wsadata);

	if (error)
		return false;

	if (wsadata.wVersion != 0x0202)
	{
		WSACleanup();
		return false;
	}

	SOCKADDR_IN target;

	target.sin_family = AF_INET;
	target.sin_port = htons(PortNo);
	target.sin_addr.s_addr = inet_addr(IPAddress);

	s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (s == INVALID_SOCKET)
	{
		return false;
	}

	bind(s, (SOCKADDR*)&target, sizeof(target));

	if (listen(s, 10) == SOCKET_ERROR)
	{
		return false;
	}
	else {
		cout << "Server set\n";
		return true;
	}
}

void CloseConnection()
{
	if (s)
		closesocket(s);

	WSACleanup();
}

int main(int argc, TCHAR* argv[])
{
	LPCTSTR FileName = L"info.txt";
	char adress[] = "192.168.0.104";
	int port = 2121;
	File = ::CreateFile(FileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	ComPortOpen();
	
	while (hSerial == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			cout << "Serial port does not exist.\n";
		}
		cout << "Some other error on opening.\n" << GetLastError() << "\n";
		Sleep(1000);
		ComPortOpen();
	}
	DCB  dcbSerialParams = { 0 };
	dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

	while (!GetCommState(hSerial, &dcbSerialParams))
	{
		cout << "Getting state error\n";
		cout << GetLastError();
		Sleep(1000);
		ComPortOpen();
	}

	DCBParams();
	
	while (!CreateServer(port, adress)) {               //  Создание сервера. На вход порт и IP
		Sleep(1000);
		cout << "You'll newer see me ;)" << endl;
	}
	clientSock = accept(s, NULL, NULL);
	cout << "Client accepted\n";

	char RecivedChar[strSize];

	//while (!ConnectToHost(port, adress)) {              //  Подключение к серверу. На вход порт и IP
	//	Sleep(1000);
	//	cout << "I'm trying to connect, really :(" << endl;
	//}
	while (ReadingFlag)
	{
		
		
		cout << send(clientSock, ReadCom(), iSize, 0) << " bytes sended to socket\n";
		Sleep(1000);
	}
	getchar();
}
