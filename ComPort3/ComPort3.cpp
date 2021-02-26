#include <iostream>
#include <windows.h>
#include <winsock.h>
#include <string>
#include <sstream>
#include "TerminalHeader.h"

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

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

void Ending(HANDLE serialPort, HANDLE dataFile, int* readingFlag)
{
	readingFlag = 0;
	CloseHandle(serialPort);
	CloseHandle(dataFile);
	cout << "Programm finished\n";
}

bool ConnectToHost(int PortNo, char* IPAddress, SOCKET s)
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

bool CreateServer(int PortNo, char* IPAddress, SOCKET *s)
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

	*s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (*s == INVALID_SOCKET)
	{
		return false;
	}

	bind(*s, (SOCKADDR*)&target, sizeof(target));

	if (listen(*s, 10) == SOCKET_ERROR)
	{
		return false;
	}
	else {
		cout << "Server set\n";
		return true;
	}
}

void CloseConnection(SOCKET s)
{
	if (s)
		closesocket(s);

	WSACleanup();
}

void ReadCom(char** recivedChar, HANDLE serialPort, HANDLE dataFile, DWORD *iSize)
{
	OFSTRUCT Buff = { 0 };
    int strSize;
	LPDWORD wrSize=0;
	char dataBuffer[STR_BUFFER_SIZE];

	if ((GetLastError()) && (GetLastError() != ERRCODE_NONSOCKETOBJECT))
	{
		if (GetLastError() == ERRCODE_READWRITEINTERUUPT || GetLastError() == ERRCODE_UNKNOWNCOMMAND || GetLastError() == ERRCODE_NOMEMORY)
		{
			throw (runtime_error("Exception throwed\n"));
		}
		if (GetLastError() == ERRCODE_HOSTCONNECTIONLOST)
		{
			cout << "\n\nClient connection lost, waiting for next connection\n\n" << GetLastError() << "\n";
			ReadFile(serialPort, recivedChar, 1, iSize, NULL);
			if (iSize > 0) {
				cout << iSize << " bytes accept\n";
				for (int i = 0; i < *iSize; i++) {
					cout << recivedChar[i];
					buffer.Write(*recivedChar[i]);
				}
				cout << "\n";
			}
			BOOL iRet = WriteFile(dataFile, recivedChar, *iSize, wrSize, NULL);
		} 
		else 
		{
			cout << "Some unknown error on reading.\n" << GetLastError() << "\n";
		}
	}
	else
	{
		ReadFile(serialPort, dataBuffer, STR_BUFFER_SIZE, iSize, NULL);
		BOOL iRet = WriteFile(dataFile, dataBuffer, *iSize, wrSize, NULL);
		*recivedChar = dataBuffer;
		for (int i = 0; i < *iSize; i++) {
			cout << dataBuffer[i];
		}
	}
}

int main(int argc, TCHAR* argv[])
{
	SOCKET s;
	SOCKET clientSock;
	HANDLE hSerial;
	HANDLE file;
	DWORD iSize;
	int readingFlag = 1;
	file = ::CreateFile(DATAFILENAME, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	hSerial = ComPortOpen();
	
	while (hSerial == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_NOT_FOUND)
		{
			cout << "Serial port does not exist.\n";
		}
		else {
			cout << "Some unknown error on opening.\n" << GetLastError() << "\n";
		}
		Sleep(1000);
		hSerial = ComPortOpen();
	}

	DCB  dcbSerialParams;
	DCBConfigure(&dcbSerialParams, hSerial);			// Получение параметров порта и их переназначение

	//TODO: try-catch
	
	while (!CreateServer(SOCKETPORT, HOSTIP, &s)) {               //  Создание сервера. На вход порт и IP
		Sleep(1000);
		cout << "You'll newer see me ;)" << endl;
	}

	clientSock = accept(s, NULL, NULL);
	char* recivedData;
	char recivedSTR[STR_BUFFER_SIZE];

	while (clientSock == AF_UNSPEC) {
		clientSock = accept(s, NULL, NULL);
		LPDWORD wrSize = 0;
		try {
			ReadCom(&recivedData, hSerial, file, &iSize);
			for (int i = 0; i < iSize; i++) {
				buffer.Write(recivedData[i]);
			}
		}
		catch (runtime_error e) {
			if (GetLastError() == ERRCODE_NOMEMORY)
			{
				cout << "No memory\n";
				Ending(hSerial, file, &readingFlag);
				break;
			}
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

	while (readingFlag)
	{
		if (send(clientSock, "", 1, 0) != -1) {
			try {
				ReadCom(&recivedData, hSerial, file, &iSize);
				sendedBytes = send(clientSock, recivedData, iSize, 0);
				if (sendedBytes != 0) {
					cout << "\n" << sendedBytes << " bytes sended to socket\n";
				}
			}
			catch (runtime_error e) {
				if (GetLastError() == ERRCODE_NOMEMORY)
				{
					cout << "No memory\n";
					Ending(hSerial, file, &readingFlag);
					break;
				}
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
			ReadCom(&recivedData, hSerial, file, &iSize);
			CloseConnection(s);
			CreateServer(SOCKETPORT, HOSTIP, &s);
			clientSock = accept(s, NULL, NULL);
			for (uint16_t i = 0; i < buffer.Count(); i++) {
				buffer.Read(recivedData[i]);
			}
		}
		catch (runtime_error e){
			if (GetLastError() == ERRCODE_NOMEMORY)
			{
				cout << "No memory\n";
				Ending(hSerial, file, &readingFlag);
				break;
			}
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
