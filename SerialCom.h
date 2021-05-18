#ifndef		__MD_SERIAL__
#define		__MD_SERIAL__

#define		KB				1024
#define		BUFFERSIZE		64*KB
#define		MAX_DIAL_TIME	50
#define		USE_CURR_PORT	0

class CSerialCom
{
protected:
	int			m_CurPort;
	DWORD		m_param;
	HANDLE		m_hCom;
	OVERLAPPED	m_Overlapped;						// structure needed for overlapped I/O
	BOOL		m_fSuccess;
	DWORD		m_dwError;
	DWORD		m_dwEvtMask;
	DWORD		m_dwModemStatus;
	char		m_ErrMsg[128];
public:
	BOOL		m_PortOpen(int PortNo);
	void		m_PortClose(void);
	BOOL		m_PortGetting(int PortNo,DWORD& param);
	BOOL		m_PortPinConf(WORD pin,bool state=false);
	BOOL		m_PortSetting(DWORD param);
	BOOL		m_PortReading(char* pData,DWORD& Counter);
	BOOL		m_PortWritting(char* pData,DWORD& Counter);
	BOOL		m_ModemSetup(char* atcommads,int PortNo=USE_CURR_PORT);
	DWORD		m_ModemGetStatus(void);
	BOOL		m_ModemWaitEvent(DWORD Events);
	BOOL		m_ModemWritting(char* atcommand);
public:
	CSerialCom();
	~CSerialCom();
};

#define	TRACE00	printf

#endif		//__MD_SERIAL__