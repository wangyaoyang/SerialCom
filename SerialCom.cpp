#include "stdafx.h"
#include <stdio.h>

#include "SerialCom.h"
///////////////////////////////////////////////////////////////
#define	MAX_PORT_NUMBER	40
static	LPTSTR	staticPortName[MAX_PORT_NUMBER] =
	{	_T("COM1:"),_T("COM2:"),_T("COM3:"),_T("COM4:"),
		_T("COM5:"),_T("COM6:"),_T("COM7:"),_T("COM8:"),
		_T("COM9:"),_T("COM10:"),_T("COM11:"),_T("COM12:"),
		_T("COM13:"),_T("COM14:"),_T("COM15:"),_T("COM16:"),
		_T("COM17:"),_T("COM18:"),_T("COM19:"),_T("COM20:"),
		_T("COM21:"),_T("COM22:"),_T("COM23:"),_T("COM24:"),
		_T("COM25:"),_T("COM26:"),_T("COM27:"),_T("COM28:"),
		_T("COM29:"),_T("COM30:"),_T("COM31:"),_T("COM32:"),
		_T("COM33:"),_T("COM34:"),_T("COM35:"),_T("COM36:"),
		_T("COM37:"),_T("COM38:"),_T("COM39:"),_T("COM40:"),
	};
///////////////////////////////////////////////////////////////
CSerialCom::CSerialCom()
{
	m_CurPort					= 0;
	m_hCom 						= INVALID_HANDLE_VALUE;
	m_Overlapped.Internal		= 0;
	m_Overlapped.InternalHigh	= 0;
	m_Overlapped.Offset			= 0;
	m_Overlapped.OffsetHigh		= 0;
	m_Overlapped.hEvent			= NULL;
	m_fSuccess					= FALSE;
	m_dwError					= 0;
	m_dwEvtMask					= 0;
	m_dwModemStatus				= 0;
	memset(m_ErrMsg,0,128);
}

CSerialCom::~CSerialCom()
{
	m_PortClose();
}

BOOL CSerialCom::m_PortGetting(int PortNo,DWORD& param)
{
	DCB			Dcb;
	int			ByteSize	= 0;
	int			StopBits	= 0;
	int			Parity		= 0;
	int			BaudRate	= 0;
	
	if( !m_PortOpen(PortNo) ) return FALSE;
	if( !GetCommState(m_hCom,&Dcb) )
	{
		TRACE00(" MODEMCOM.DLL--Error Can not get port status");
		return FALSE;
	}
	ByteSize	= Dcb.ByteSize;
	StopBits	= Dcb.StopBits+1;
	Parity		= Dcb.Parity;
	BaudRate	= Dcb.BaudRate;

	param =	(PortNo	 & 0x0000003f) << 26	|
			(ByteSize & 0x0000000f) << 22	|
			(StopBits & 0x00000003) << 20	|
			(Parity	 & 0x00000003) << 18	|
			(BaudRate & 0x0003ffff);
	m_PortClose();
	return TRUE;
}

#define	PORT_PIN_SET_CTS	0x0001
#define	PORT_PIN_SET_RTS	0x0002
#define	PORT_PIN_SET_DSR	0x0003
#define	PORT_PIN_SET_DTR	0x0004
#define	PORT_PIN_GET_CTS	0x0100
#define	PORT_PIN_GET_RTS	0x0200
#define	PORT_PIN_GET_DSR	0x0300
#define	PORT_PIN_GET_DTR	0x0400

BOOL CSerialCom::m_PortPinConf(WORD pin,bool state)
{
	DCB			Dcb;

	if( !GetCommState(m_hCom,&Dcb) )
	{
		TRACE00(" MODEMCOM.DLL--Error Can not get port status");
		return FALSE;
	}
	/**********************************************************/
	switch( pin )
	{
	case PORT_PIN_SET_DSR:	break;
	case PORT_PIN_SET_RTS:
		Dcb.fRtsControl = state ? RTS_CONTROL_ENABLE : RTS_CONTROL_DISABLE;	break;
	case PORT_PIN_SET_CTS:	break;
	case PORT_PIN_SET_DTR:
		Dcb.fDtrControl = state ? DTR_CONTROL_ENABLE : DTR_CONTROL_DISABLE;	break;
	case PORT_PIN_GET_CTS:	return Dcb.fOutxCtsFlow;
	case PORT_PIN_GET_RTS:
		switch( Dcb.fRtsControl )
		{
		case RTS_CONTROL_DISABLE:	return FALSE;
		case RTS_CONTROL_ENABLE:	return TRUE;
		default:					return FALSE;
		}
		break;
	case PORT_PIN_GET_DSR:	return Dcb.fOutxDsrFlow;
	case PORT_PIN_GET_DTR:
		switch( Dcb.fDtrControl )
		{
		case DTR_CONTROL_DISABLE:	return FALSE;
		case DTR_CONTROL_ENABLE:	return TRUE;
		case DTR_CONTROL_HANDSHAKE:	return FALSE;
		default:					return FALSE;
		}
		break;
	default:;
	}
	if(!SetCommState(m_hCom,&Dcb))
	{
		m_dwError = GetLastError();
		sprintf_s(m_ErrMsg,"\nCOM%d Error code:%d",m_CurPort+1,m_dwError);
		TRACE00(m_ErrMsg);
	}
	return TRUE;
}

BOOL CSerialCom::m_PortSetting(DWORD param)
{
	DCB			Dcb;
	int			PortNo	= ( 0xfc000000 & param ) >> 26;
	int			ByteSize	= ( 0x03c00000 & param ) >> 22;
	int			StopBits	= ( 0x00300000 & param ) >> 20;
	int			Parity	= ( 0x000c0000 & param ) >> 18;
	int			BaudRate	= ( 0x0003ffff & param );

	if( !m_PortOpen(PortNo) ) return FALSE;
	if( !GetCommState(m_hCom,&Dcb) )
	{
		TRACE00(" MODEMCOM.DLL--Error Can not get port status");
		return FALSE;
	}
	/**********************************************************/
	Dcb.ByteSize	= ByteSize;
	Dcb.StopBits	= StopBits-1;
	Dcb.Parity		= Parity;
	Dcb.BaudRate	= BaudRate;

	if(!SetCommState(m_hCom,&Dcb))
	{
		m_dwError = GetLastError();
		sprintf_s(m_ErrMsg,sizeof(m_ErrMsg),"\nCOM%d Error code:%d",m_CurPort+1,m_dwError);
		TRACE00(m_ErrMsg);
	}
	m_PortClose();
	return TRUE;
}

BOOL CSerialCom::m_PortOpen(int PortNo)
{	//PortNo is 1 base index ( 1..8 )
	//m_CurPort is 0 base index ( 0..7 )
	//If PortNo equal zero that's mean
	//	the current value of m_CurPort will be used.
	if(m_hCom != INVALID_HANDLE_VALUE) { CloseHandle(m_hCom); m_hCom = INVALID_HANDLE_VALUE; }
	if(PortNo<0 || PortNo>MAX_PORT_NUMBER) return FALSE;
	else if( PortNo ) m_CurPort = PortNo-1;
	m_hCom = CreateFile(
		staticPortName[m_CurPort],
		GENERIC_READ | GENERIC_WRITE,
		0,
		NULL,
		OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL,// |FILE_FLAG_OVERLAPPED,
		NULL
	); 
	if(m_hCom == INVALID_HANDLE_VALUE) {
		m_dwError = GetLastError();
		sprintf_s(m_ErrMsg,"\nCOM%d Error code:%d",PortNo,m_dwError);
		TRACE00(m_ErrMsg);
		return FALSE;
	}
	//在这里处理超时设置问题
	COMMTIMEOUTS	timeout;

	GetCommTimeouts( m_hCom,&timeout );
	timeout.ReadIntervalTimeout = MAXDWORD;
	timeout.ReadTotalTimeoutMultiplier = MAXDWORD;
	timeout.ReadTotalTimeoutConstant = 100;
	timeout.WriteTotalTimeoutMultiplier = MAXDWORD;
	timeout.WriteTotalTimeoutConstant = 100;
	if( !SetCommTimeouts( m_hCom,&timeout ) ) { TRACE00("\nFailed Setting time out"); return FALSE; }
	//在这里处理超时设置问题
	if(!SetupComm(m_hCom,BUFFERSIZE,BUFFERSIZE)) {
		m_dwError = GetLastError();
		{
			char	msg[256];
			memset( msg,0,256 );
			sprintf_s( msg,"\nError: code %d-COM%d ,Can't setup port",m_dwError,m_CurPort+1 );
			TRACE00( msg );
		}
		return FALSE;
	}
	//wangyaoyang20040216m_PortPinConf(PORT_PIN_SET_DTR,false);
	if(!m_PortPinConf(PORT_PIN_SET_DTR,true)) return FALSE;
	return TRUE;
}

BOOL CSerialCom::m_PortWritting(char* pData,DWORD& NumToWrite)
{
    DWORD		NumBeWritten=0;					// address of number of bytes written 

	if(m_hCom == INVALID_HANDLE_VALUE) return FALSE;
	if(!PurgeComm(m_hCom,PURGE_TXABORT|PURGE_TXCLEAR)) return FALSE;
	//wangyaoyang20040216
	//if(!m_PortPinConf(PORT_PIN_SET_DTR,true)) return FALSE;
	m_PortPinConf(PORT_PIN_SET_RTS,true);
	if(!WriteFile(	m_hCom, pData, NumToWrite, &NumBeWritten,NULL ))
	{
		m_dwError = GetLastError();
		if( m_dwError != ERROR_IO_PENDING )
		{
			char	msg[256];
			memset( msg,0,256 );
			sprintf_s( msg,"\nError: code %d _COM%d ,Can't write port",m_dwError,m_CurPort+1 );
			TRACE00( msg );
			return FALSE;
		}
	}
	//wangyaoyang20040216if(!m_PortPinConf(PORT_PIN_SET_DTR,false)) return FALSE;
	//m_PortPinConf(PORT_PIN_GET_RTS);
	//m_PortPinConf(PORT_PIN_SET_CTS);
	//Sleep(100);
	m_PortPinConf(PORT_PIN_SET_RTS);
	NumToWrite = NumBeWritten;
	return TRUE;
}

BOOL CSerialCom::m_PortReading(char* pData,DWORD& NumToRead)
{
	DWORD		NumBeRead=0;

	if(m_hCom == INVALID_HANDLE_VALUE) return FALSE;
	//wangyaoyang20040216if(m_PortPinConf(PORT_PIN_GET_DTR)==FALSE)
	{
		if(!ReadFile( m_hCom, (LPVOID)pData, NumToRead, &NumBeRead,	NULL ))
		{
			m_dwError = GetLastError();
			if( m_dwError != ERROR_IO_PENDING )
			{
				char	msg[256];
				memset( msg,0,256 );
				sprintf_s( msg,"\nError: code %d COM%d ,Can't read port",m_dwError,m_CurPort+1 );
				TRACE00( msg );
				return FALSE;
			}
		}
		NumToRead = NumBeRead;
		return TRUE;
	}
	return FALSE;
}

void CSerialCom::m_PortClose(void)
{
	if(m_hCom == INVALID_HANDLE_VALUE) return;
	EscapeCommFunction(	m_hCom,CLRRTS );
	EscapeCommFunction(	m_hCom,CLRDTR );
	CloseHandle(m_hCom);
	m_hCom = INVALID_HANDLE_VALUE;
}

BOOL CSerialCom::m_ModemSetup(char* atcommads,int PortNo)
{
	DWORD	length = 0;
	DWORD	modemStatus = 0;
	char	init_command[64];

	memset(init_command,0,64);
	if(!m_PortOpen(PortNo)) return FALSE;
	modemStatus = m_ModemGetStatus();
	if( (modemStatus & MS_DSR_ON) == 0 )
	{
		m_PortClose();
		return FALSE;
	}
	if(!SetCommMask(m_hCom,
		EV_BREAK|EV_CTS|EV_DSR|EV_ERR|EV_RING|EV_RLSD|EV_RXCHAR|EV_RXFLAG|EV_TXEMPTY))
	{
		sprintf_s(m_ErrMsg,"\nError %d :Can not create modem signal monitor.",m_dwError);
		TRACE00(m_ErrMsg);
		m_PortClose();
		return FALSE;
	}
	m_Overlapped.hEvent = CreateEvent(NULL,FALSE,FALSE,NULL);
	EscapeCommFunction(m_hCom,SETDTR);

	for( int i=0,j=0; atcommads[i]&&i<127; i++ )
	{
		if( atcommads[i] == 0x01 )
		{
			strcat_s( init_command,sizeof(init_command),"\r\n" );
			length = strlen(init_command);
			if( strcmp( init_command,"\r\n" ) != 0 )
				m_PortWritting(init_command,length);
		}
		else if( atcommads[i] == 0x02 )
		{
			j = 0;
			memset( init_command,0,64 );
			Sleep( 1000 );
		}
		else
		{
			init_command[j++] = atcommads[i];
		}
	}
	strcat_s( init_command,sizeof(init_command),"\r\n" );
	length = strlen(init_command);
	if( strcmp( init_command,"\r\n" ) != 0 )
		m_PortWritting(init_command,length);
	Sleep( 1000 );
	m_PortClose();

	return TRUE;
}

DWORD CSerialCom::m_ModemGetStatus(void)
{
	if(m_hCom == INVALID_HANDLE_VALUE) return FALSE;
	DWORD	oldModemStatus = m_dwModemStatus;
	if(!GetCommModemStatus(m_hCom,&m_dwModemStatus)) {
		m_dwError = GetLastError();
		sprintf_s(m_ErrMsg,"\nError code %d :Incorrect response from Modem",m_dwError);
		TRACE00(m_ErrMsg);
		return FALSE ;
	}
	if( oldModemStatus != m_dwModemStatus )
	{
		char buffer[64];

		sprintf_s( buffer,"\nCTS__DSR__CD___RING_\n" );
		TRACE00( buffer );
		if(MS_CTS_ON & m_dwModemStatus)	TRACE00("●   "); else TRACE00("○   ");
		if(MS_DSR_ON & m_dwModemStatus) TRACE00("●   "); else TRACE00("○   ");
		if(MS_RLSD_ON & m_dwModemStatus) TRACE00("●   "); else TRACE00("○   ");
		if(MS_RING_ON & m_dwModemStatus) TRACE00("●   "); else TRACE00("○   ");
		TRACE00("\n");
	}
	return m_dwModemStatus;
}

BOOL CSerialCom::m_ModemWaitEvent(DWORD Events)
{
/*	if(m_hCom == INVALID_HANDLE_VALUE) return FALSE;
	if(!WaitCommEvent(m_hCom,&m_dwEvtMask,&m_Overlapped)) {
		m_dwError = GetLastError();
		sprintf_s(m_ErrMsg,"\nModem Error code:%d",m_dwError);
		MessageBox(m_hParendwnd,m_ErrMsg,"Failed waiting event from Modem:",MB_ICONSTOP);
		return FALSE ;
	}
	if(Events & m_dwEvtMask) return TRUE; else return FALSE;
*/
	return TRUE;
}

BOOL CSerialCom::m_ModemWritting(char* atcommand)
{
	DWORD	length = 0;
	BOOL	modem_ok = FALSE;
	int		i = 0;

	if(m_hCom == INVALID_HANDLE_VALUE) return FALSE;
	for( i=0; i<10*MAX_DIAL_TIME; i++,TRACE00(".") )
		if( m_ModemGetStatus() & MS_RLSD_ON ) Sleep(100);
			else { modem_ok = TRUE;	break; }
	if(!modem_ok) return FALSE; else  Sleep( 3000 );
	if(!m_PortWritting(atcommand,length=strlen(atcommand))) return FALSE;
	for( i=0; i<10*MAX_DIAL_TIME; i++,Sleep(100),TRACE00(".") )
		if( m_ModemGetStatus() & MS_RLSD_ON ) return TRUE;
	return FALSE;
}
