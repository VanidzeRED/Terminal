﻿#include <iostream>
#include <windows.h>
#include <winsock.h>
#include <string>
#include <sstream>

#pragma comment(lib, "Ws2_32.lib")

using namespace std;

SOCKET s;
SOCKET clientSock;

int ReadingFlag = 1;
const int strSize = 128;
DWORD iSize;
LPCTSTR sPortName = L"COM6";
LPCTSTR FileName = L"info.txt";
char adress[] = "192.168.0.106";
int port = 2121;

//TODO: параметры переместить в config-файл (*.h)

class RingBuffer
{
private:
	// память под буфер
	char _data[strSize];
	// количество чтений
	volatile uint16_t _readCount;
	// количество записей
	volatile uint16_t _writeCount;
	// маска для индексов
	static const uint16_t _mask = strSize - 1;
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
	HANDLE serialPort = ::CreateFile(sPortName, GENERIC_READ, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
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

#define ERRCODE_NOMEMORY ((DWORD)39)

void ReadCom(char* dataBuffer, HANDLE serialPort, HANDLE dataFile)
{
	OFSTRUCT Buff = { 0 };
	iSize = 0;
	LPDWORD wrSize=0;
	DWORD Code22 = 22;
	DWORD Code39 = 39;
	DWORD Code995 = 995;
	DWORD Code10038 = 10038;
	DWORD Code10053 = 10053;

	if ((GetLastError()) && (GetLastError() != Code10038))
	{
		if (GetLastError() == Code995 || GetLastError() == Code22)
		{
			cout << "\nCom-port connection lost.\n" << GetLastError() << "\n";
			throw (runtime_error("Exception throwed\n"));
		}
		if (GetLastError() == Code10053) 
		{
			cout << "\n\nClient connection lost, waiting for next connection\n\n" << GetLastError() << "\n";
			ReadFile(serialPort, dataBuffer, strSize, &iSize, NULL);
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
			CreateServer(port, adress);
			clientSock = accept(s, NULL, NULL);
				for (uint16_t i = 0; i < buffer.Count(); i++) {
				buffer.Read(dataBuffer[i]);
			}
		} 
		else 
		{
			if (GetLastError() == Code39)
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
		int i = 0;
		ReadFile(serialPort, dataBuffer, 1, &iSize, NULL);
		BOOL iRet = WriteFile(dataFile, dataBuffer, iSize, wrSize, NULL);
	}
	
}

int main(int argc, TCHAR* argv[])
{
	HANDLE hSerial;
	HANDLE file;
	file = ::CreateFile(FileName, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
	hSerial = ComPortOpen();
	
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
	/*
	while (!CreateServer(port, adress)) {               //  Создание сервера. На вход порт и IP
		Sleep(1000);
		cout << "You'll newer see me ;)" << endl;
	}
	
	char* recivedData;
	while (clientSock == AF_UNSPEC) {
		LPDWORD wrSize = 0;
		char RecivedChar[strSize];
		
		ReadFile(hSerial, RecivedChar, strSize, &iSize, NULL);
		if (iSize > 0) {
			cout << iSize << " bytes accept\n";
			for (int i = 0; i < iSize; i++) {
				cout << RecivedChar[i];
				buffer.Write(RecivedChar[i]);
			}
			cout << "\n";
		}
		BOOL iRet = WriteFile(File, RecivedChar, iSize, wrSize, NULL);
		clientSock = accept(s, NULL, NULL);
		for (uint16_t i = 0; i < buffer.Count(); i++) {
			buffer.Read(RecivedChar[i]);
		}

	}

	cout << "Client accepted\n";

	int sendedBytes;
	*/

	char recivedData[strSize];

	while (ReadingFlag)
	{
		/*if (send(clientSock, "", 1, 0) != -1) {
			sendedBytes = send(clientSock, ReadCom(), iSize, 0);
			if (sendedBytes != 0) {
				cout << sendedBytes << " bytes sended to socket\n";
			}
			Sleep(100);
		} else {*/
			//cout << "Connection lost";
		try {
			ReadCom(recivedData, hSerial, file);
			for (int i = 0; i < iSize; i++) {
				char a = recivedData[i];
				cout << a;
				//buffer.Write(recivedData[i]);
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
			
		//}
		Sleep(100);
	}
	
	getchar();
}
