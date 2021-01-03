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
const int strSize = 128;
DWORD iSize;
LPCTSTR sPortName = L"COM3";
LPCTSTR FileName = L"info.txt";
char adress[] = "192.168.0.104";
int port = 2121;

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

char* ReadCom()
{
	OFSTRUCT Buff = { 0 };
	iSize = 0;
	LPDWORD wrSize=0;
	DWORD Code39 = 39;
	DWORD Code10038 = 10038;
	DWORD Code10053 = 10053;
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
			if (GetLastError() == Code10053) {
				cout << "\n\nClient connection lost, waiting for next connection\n\n" << GetLastError() << "\n";
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
				cout << "2.Client connection lost, waiting for next connection\n";
				CloseConnection();
				CreateServer(port, adress);
				clientSock = accept(s, NULL, NULL);
				for (uint16_t i = 0; i < buffer.Count(); i++) {
					buffer.Read(RecivedChar[i]);
				}
			}
			else
			{
				cout << "Some other error on reading.\n" << GetLastError() << "\n";
				if (GetLastError() == Code39)
				{
					cout << "No memory\n";
					Ending();
				}
			}
		}
	}
	else
	{
		ReadFile(hSerial, RecivedChar, strSize, &iSize, NULL);
		BOOL iRet = WriteFile(File, RecivedChar, iSize, wrSize, NULL);
	}
	return RecivedChar;
	
}

int main(int argc, TCHAR* argv[])
{
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
	char* recivedData;

	while (ReadingFlag)
	{
		if (send(clientSock, ".", 1, 0) != -1) {
			recivedData = ReadCom();
			cout << iSize << " bytes accept\n";
			if (iSize > 0) {
				for (int i = 0; i < iSize; i++) {
					cout << recivedData[i];
				}
				cout << "\n";
			}
			cout << send(clientSock, recivedData, iSize, 0) << " bytes sended to socket\n";
		}
		else {
			recivedData = ReadCom();
			for (int i = 0; i < iSize; i++) {
				cout << recivedData[i];
				buffer.Write(recivedData[i]);
			}
		}
		Sleep(1000);
	}
	getchar();
}
