
// Created 2012 by Nikolai Smolyanskiy
// Defines the API to talk to the Bioloid Controller CM-5 to control dynamixels
//

#include "stdafx.h"
#include <stdio.h>
#include <conio.h>
#include <Windows.h>

class SerialPort
{
public:
	SerialPort();
	virtual ~SerialPort();
	
	HRESULT Open(const wchar_t* szPortName, DWORD baudRate);
	void Close();
	void Clear();

	HRESULT SendData(BYTE* pBuffer, unsigned long* pSize);
	HRESULT ReceiveData(BYTE* pBuffer, unsigned long* pSize);

private:
	HANDLE serialPortHandle;
};

SerialPort::SerialPort() : 
	serialPortHandle(INVALID_HANDLE_VALUE)
{
}

SerialPort::~SerialPort()
{    
	Close();
} 

HRESULT SerialPort::Open(const wchar_t* szPortName, DWORD baudRate)
{
	HRESULT hrResult = S_OK;
	DCB dcb;

	memset( &dcb, 0, sizeof(dcb) );
	
	dcb.DCBlength = sizeof(dcb);
	dcb.BaudRate = baudRate;
	dcb.Parity = NOPARITY;
	dcb.fParity = 0;
	dcb.StopBits = ONESTOPBIT;
	dcb.ByteSize = 8;

	serialPortHandle = CreateFile(szPortName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, NULL, NULL);
	if ( serialPortHandle!=INVALID_HANDLE_VALUE )
	{
		if( !SetCommState(serialPortHandle, &dcb) )
		{
			hrResult = E_INVALIDARG;
			Close();
		}
	}
	else
	{        
		hrResult = ERROR_OPEN_FAILED;
	}
	
	return hrResult;
}

void SerialPort::Close()
{ 
	if (serialPortHandle!=INVALID_HANDLE_VALUE || serialPortHandle!=NULL)
	{
		PurgeComm(serialPortHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
		CloseHandle(serialPortHandle);
	}
	serialPortHandle = INVALID_HANDLE_VALUE;
}

void SerialPort::Clear()
{ 
	if (serialPortHandle!=INVALID_HANDLE_VALUE || serialPortHandle!=NULL)
	{
		PurgeComm(serialPortHandle, PURGE_RXCLEAR | PURGE_TXCLEAR);
	}
}

HRESULT SerialPort::SendData(BYTE* pBuffer, unsigned long* pSize)
{   
	HRESULT hrResult = ERROR_WRITE_FAULT;

	if (serialPortHandle!=INVALID_HANDLE_VALUE && serialPortHandle!=NULL)
	{
		if( WriteFile(serialPortHandle, pBuffer, *pSize, pSize, NULL) && 
			FlushFileBuffers(serialPortHandle)
		)
		{
			hrResult = S_OK;
		}
	}
	
	return hrResult;
} 

HRESULT SerialPort::ReceiveData(BYTE* pBuffer, unsigned long* pSize)
{    
	HRESULT hrResult = ERROR_READ_FAULT;

	if (serialPortHandle!=INVALID_HANDLE_VALUE && serialPortHandle!=NULL)
	{
		if( ReadFile(serialPortHandle, pBuffer, *pSize, pSize, NULL) )
		{
			hrResult = S_OK;
		}
	}
	
	return hrResult;
} 


bool CreateAX12SetPositionCommand(BYTE id, short goal, BYTE* pBuffer, DWORD* pSize)
{   
	const unsigned int packetSize = 9;

	if(*pSize < packetSize)
	{
		return false;
	}

	// PACKET STRUCTURE: OXFF 0XFF ID LENGTH INSTRUCTION PARAMETER_1 �PARAMETER_N CHECKSUM
	*pSize = packetSize;
	pBuffer[0] = 0xFF;
	pBuffer[1] = 0xFF;
	pBuffer[2] = id;
	pBuffer[3] = 2 /* number of parameters */ + 3;	// packet body length
	pBuffer[4] = 3;						// instruction id = write data
	// Parameters
	pBuffer[5] = 30;					// start address of position goal setting
	pBuffer[6] = BYTE(goal);			// goal low byte (to address 30)
	pBuffer[7] = BYTE(goal>>8);			// goal high byte (to address 31)
	
	// Checksum
	DWORD packetSum = 0;
	for(size_t i=2; i<=7; i++)
	{
		packetSum += pBuffer[i];
	}
	pBuffer[8] = (BYTE)(~packetSum);

	return true;
}

bool SetDynamixelPosition(SerialPort* pSerialPort, unsigned int id, int position)
{
	BYTE buffer[256];
	DWORD size = sizeof(buffer);

	if( !CreateAX12SetPositionCommand(id, (short)position, buffer, &size) )
	{
		return false;
	}

	HRESULT hr = pSerialPort->SendData(buffer, &size);
	if(FAILED(hr))
	{
		printf("Failed to send set dynamixel position command\n");
		return false;
	}
	Sleep(10);

	memset(buffer, 0, sizeof(buffer));
	size = sizeof(buffer);
	pSerialPort->ReceiveData(buffer, &size);

	if (size>4 && buffer[4] == 0)
	{
		printf("id=%d set to position=%d\n", id, position);
	}
	else
	{
		printf("Error while setting id=%d position=%d, error:%d\n", id, position, buffer[4]);
		return false;
	}     
	
	return true;
}

bool SendTossModeCommand(SerialPort* pSerialPort)
{
	BYTE buffer[1024];
	buffer[0]='t';
	buffer[1]='\r';
	DWORD size = 2;

	HRESULT hr = pSerialPort->SendData(buffer, &size);
	if(FAILED(hr))
	{
		printf("Failed to send TOSS model command\n");
		return false;
	}
	Sleep(100);

	size = sizeof(buffer);
	pSerialPort->ReceiveData(buffer, &size);

	return true;
}

int _tmain(int argc, _TCHAR* argv[])
{
	DWORD baudRate = 57600;
	SerialPort comPort;
	
	HRESULT hr = comPort.Open(L"COM3", baudRate);
	if(FAILED(hr))
	{
		printf("Cannot open COM3 port\n");
		return 0;
	}

	SendTossModeCommand(&comPort);

	while(1)
	{
		printf( "Enter dynamixel ID and goal position:\n" );

		int id = 0;
		int position = 0;
		scanf("%d %d", &id, &position);

		SetDynamixelPosition(&comPort, id, position);

		printf("Press ESC to terminate, otherwise press any other key to continue\n");
		if(getch() == 0x1b)
		{
			break;
		}
	}

	comPort.Close();

	return 0;
}

