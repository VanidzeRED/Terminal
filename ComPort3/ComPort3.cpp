#include <iostream>
#include <windows.h>
#include <winsock.h>
#include <string>
#include <sstream>
#include "TerminalHeader.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

SOCKET s;
SOCKET clientSock;

int ReadingFlag = 1;
DWORD iSize;

#define ERRCODE_UNKNOWNCOMMAND ((DWORD)22)
#define ERRCODE_NOMEMORY ((DWORD)39)
#define ERRCODE_READWRITEINTERUUPT ((DWORD)995)
#define ERRCODE_NONSOCKETOBJECT ((DWORD)10038)
#define ERRCODE_HOSTCONNECTIONLOST ((DWORD)10053)
#define HOSTIP ((char*)"192.168.0.106")
#define DATAFILENAME ((LPCTSTR)L"info.txt")
#define COMPORTNAME ((LPCTSTR)L"COM3")
#define SOCKETPORT ((int)2121)


//TODO: параметры переместить в config-файл (*.h)

class RingBuffer
{
private:
	// память под буфер
	char _data[128];
	// количество чтений
	volatile uint16_t _readCount;
	// количество записей
	volatile uint16_t _writeCount;
	// маска для индексов
	static const uint16_t _mask = 127;
public:
	// запись в буфер, возвращает true если значение записано
	inline bool Write(char value)
	{
		if (IsFull())
			return false;
		_data[_writeCount++ & _mask] = value;
		return true;
	}
	// чтение из буфера, возвращает true если значение прочитано
	inline bool Read(char value)
	{
		if (IsEmpty())
			return false;
		value = _data[_readCount++ & _mask];
		return true;
	}
	// возвращает первый элемент из буфера, не удаляя его
	inline char First()const
	{
		return operator[](0);
	}
	
	// возвращает элемент по индексу
	inline char& operator[] (uint16_t i)
	{
		return _data[(_readCount + i) & _mask];
	}

	inline const char operator[] (uint16_t i)const
	{
		return _data[(_readCount + i) & _mask];
	}
	// пуст ли буфер
	inline bool IsEmpty()const
	{
		return _writeCount == _readCount;
	}
	// полон ли буфер
	inline bool IsFull()const
	{
		return ((uint16_t)(_writeCount - _readCount) & (uint16_t)~(_mask)) != 0;
	}
	// количество элементов в буфере
	uint16_t Count()
	{
		return (_writeCount - _readCount) & _mask;
	}
	// очистить буфер
	inline void Clear()
	{
		_readCount = 0;
		_writeCount = 0;
	}
};

	RingBuffer buffer;

HANDLE ComPortOpen()
{
	HANDLE serialPort = ::CreateFile(COMPORTNAME, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
	cout << "Port opened\n";
	return serialPort;
}

void DCBConfigure(DCB* dcbSerialParams, HANDLE serialPort)
{
	if (!GetCommState(serialPort, dcbSerialParams))
	{
		throw runtime_error("error getting state of com-port\n");
	}
	dcbSerialParams->DCBlength = sizeof(dcbSerialParams);
	dcbSerialParams->BaudRate = CBR_9600;
	dcbSerialParams->ByteSize = 8;
	dcbSerialParams->StopBits = ONESTOPBIT;
	dcbSerialParams->Parity = NOPARITY;
	dcbSerialParams->fAbortOnError = TRUE;
	if (!SetCommState(serialPort, dcbSerialParams))
	{
		throw runtime_error("error setting state of com-port\n");
	}
	cout << "DCB params set\n";
}

void Ending(HANDLE serialPort, HANDLE dataFile)
{
	ReadingFlag = 0;
	CloseHandle(serialPort);
	CloseHandle(dataFile);
	cout << "Programm finished\n";
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

void ReadCom(char* dataBuffer, HANDLE serialPort, HANDLE dataFile)
{
	OFSTRUCT Buff = { 0 };
    int strSize;
	LPDWORD wrSize=0;

	if ((GetLastError()) && (GetLastError() != ERRCODE_NONSOCKETOBJECT))
	{
		if (GetLastError() == ERRCODE_READWRITEINTERUUPT || GetLastError() == ERRCODE_UNKNOWNCOMMAND)
		{
			cout << "\nCom-port connection lost.\n" << GetLastError() << "\n";
			throw (runtime_error("Exception throwed\n"));
		}
		if (GetLastError() == ERRCODE_HOSTCONNECTIONLOST)
		{
			cout << "\n\nClient connection lost, waiting for next connection\n\n" << GetLastError() << "\n";
			ReadFile(serialPort, dataBuffer, 1, &iSize, NULL);
			if (iSize > 0) {
				cout << iSize << " bytes accept\n";
				for (int i = 0; i < iSize; i++) {
					cout << dataBuffer[i];
					buffer.Write(dataBuffer[i]);
				}
				cout << "\n";
			}
			BOOL iRet = WriteFile(dataFile, dataBuffer, iSize, wrSize, NULL);
			CloseConnection();
			CreateServer(SOCKETPORT, HOSTIP);
			clientSock = accept(s, NULL, NULL);
				for (uint16_t i = 0; i < buffer.Count(); i++) {
				buffer.Read(dataBuffer[i]);
			}
		} 
		else 
		{
			if (GetLastError() == ERRCODE_NOMEMORY)
			{
				cout << "No memory\n";
				Ending(serialPort, dataFile);
			}
			else
			{
				cout << "Some other error on reading.\n" << GetLastError() << "\n";
			}
		}
	}
	else
	{
		char c;
		int i = 0;
		do {
			ReadFile(serialPort, &c, 1, &iSize, NULL);
			dataBuffer[i++] = c;
		} while (iSize > 0);
		
		BOOL iRet = WriteFile(dataFile, dataBuffer, iSize, wrSize, NULL);
	}
	
}

int main(int argc, TCHAR* argv[])
{
	HANDLE hSerial;
	HANDLE file;
	file = ::CreateFile(DATAFILENAME, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	hSerial = ComPortOpen();
	const int strSize = 128;
	
	while (hSerial == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			cout << "Serial port does not exist.\n";
		}
		else {
			cout << "Some other error on opening.\n" << GetLastError() << "\n";
		}
		Sleep(1000);
		hSerial = ComPortOpen();
	}

	DCB  dcbSerialParams;
	DCBConfigure(&dcbSerialParams, hSerial);			// Получение параметров порта и их переназначение

	//TODO: try-catch
	
	while (!CreateServer(SOCKETPORT, HOSTIP)) {               //  Создание сервера. На вход порт и IP
		Sleep(1000);
		cout << "You'll newer see me ;)" << endl;
	}

	clientSock = accept(s, NULL, NULL);

	while (clientSock == AF_UNSPEC) {
		clientSock = accept(s, NULL, NULL);
		LPDWORD wrSize = 0;
		char recivedData[strSize];
		
		try {
			ReadCom(recivedData, hSerial, file);
			for (int i = 0; i < iSize; i++) {
				char a = recivedData[i];
				cout << a;
				buffer.Write(recivedData[i]);
			}
		}
		catch (runtime_error e) {
			cout << e.what();
			CloseHandle(hSerial);
			while (GetLastError())
			{
				if (GetLastError() == ERROR_FILE_NOT_FOUND)
				{
					cout << "Serial port does not exist.\n";
				}
				else {
					cout << "Some other error on opening.\n" << GetLastError() << "\n";
				}
				Sleep(1000);
				hSerial = ComPortOpen();
			}

			DCB  dcbSerialParams;
			DCBConfigure(&dcbSerialParams, hSerial);
		}
		BOOL iRet = WriteFile(file, recivedData, iSize, wrSize, NULL);
		for (uint16_t i = 0; i < buffer.Count(); i++) {
			buffer.Read(recivedData[i]);
		}

	}

	cout << "Client accepted\n";

	int sendedBytes;

	char recivedData[strSize];

	while (ReadingFlag)
	{
		if (send(clientSock, "", 1, 0) != -1) {
			try {
				ReadCom(recivedData, hSerial, file);
				sendedBytes = send(clientSock, recivedData, iSize, 0);
				if (sendedBytes != 0) {
					cout << sendedBytes << " bytes sended to socket\n";
				}
			}
			catch (runtime_error e) {
				cout << e.what();
				CloseHandle(hSerial);
				while (GetLastError())
				{
					if (GetLastError() == ERROR_FILE_NOT_FOUND)
					{
						cout << "Serial port does not exist.\n";
					}
					else {
						cout << "Some other error on opening.\n" << GetLastError() << "\n";
					}
					Sleep(1000);
					hSerial = ComPortOpen();
				}

				DCB  dcbSerialParams;
				DCBConfigure(&dcbSerialParams, hSerial);
			}
		} else {
			cout << "Connection lost";
		try {
			ReadCom(recivedData, hSerial, file);
			for (int i = 0; i < iSize; i++) {
				char a = recivedData[i];
				cout << a;
				buffer.Write(recivedData[i]);
			}
		}
		catch (runtime_error e){
			cout << e.what();
			CloseHandle(hSerial);
			while (GetLastError())
			{
				if (GetLastError() == ERROR_FILE_NOT_FOUND)
				{
					cout << "Serial port does not exist.\n";
				}
				else {
					cout << "Some other error on opening.\n" << GetLastError() << "\n";
				}
				Sleep(1000);
				hSerial = ComPortOpen();
			}

			DCB  dcbSerialParams;
			DCBConfigure(&dcbSerialParams, hSerial);
		}
			
		}
		Sleep(100);
	}
	
	getchar();
}
