//====================================================================================
// Open Computer and Software Inventory Next Generation
// Copyright (C) 2010 OCS Inventory NG Team. All rights reserved.
// Web: http://www.ocsinventory-ng.org

// This code is open source and may be copied and modified as long as the source
// code is always made freely available.
// Please refer to the General Public Licence V2 http://www.gnu.org/ or Licence.txt
//====================================================================================

// InventoryRequest.cpp: implementation of the CInventoryRequest class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "InventoryRequest.h"
#include "FilePackageHistory.h"
#include "commonDownload.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CInventoryRequest::CInventoryRequest( BOOL bNotify)
{
	m_bNotify = bNotify;

	if (!initInventory())
		// Can't get Device hardware and os => stop !
		exit( -1);
}

CInventoryRequest::~CInventoryRequest()
{
	delete m_pTheDB;
	delete m_pState;
	delete m_pSysInfo;
}

void CInventoryRequest::setSuccess()
{
	writeLastInventoryState();
}

BOOL CInventoryRequest::initInventory()
{
	CString csStateFile;

	if (m_bNotify)
		// Notify IP information changes
		setQuery( _T( "NOTIFY"), _T( "IP"));
	else
		setQuery( _T( "INVENTORY"));

	/****	
	*
	* Other inventory sections
	*
	****/
	m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Loading Download history"));
	if (!loadDownloadHistory())
	{
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T("INVENTORY => Failed opening Download Package history file <%s>"), LookupError( GetLastError()));
	}

	/******************/

	if( m_pDeviceid->getOldDeviceID() != _T("") )
		m_cmXml.AddChildElem( _T("OLD_DEVICEID"),m_pDeviceid->getOldDeviceID());
			
	/****	
	*
	* Get the Device netbios Name
	*
	****/

	m_pTheDB	= new CXMLInteract(&m_cmXml);
	m_pState	= new COCSInventoryState;
	m_pSysInfo	= new CSysInfo( getAgentConfig()->isDebugRequired() >= OCS_DEBUG_MODE_TRACE, getDataFolder());

	m_Device.SetDeviceName( m_pDeviceid->getComputerName());
	// Get DeviceId from ocsinventoryFront
	m_Device.SetDeviceID( m_pDeviceid->getDeviceID() );

	/*****
	 *
	 *	Main inventory function
	 *
	 ****/
	// Get Device info
	if (!runInventory())
		return FALSE;

	// Read last inventory state from XML file
	m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Reading last inventory state"));
	csStateFile.Format( _T("%s\\%s"), getDataFolder(), OCS_LAST_STATE_FILE);
	if (!m_pState->ReadFromFile( csStateFile))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Error while reading state file <%s>"), csStateFile);
	return TRUE;
}

BOOL CInventoryRequest::final()
{
	BOOL bSuccess = FALSE;
	CString	csFilename;

	getXmlPointerContent();
	if (m_bNotify)
	{
		// Notify network informations changes
		bSuccess = m_pTheDB->NotifyNetworks( m_NetworkList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Notify %u Network Adapter(s)"), m_NetworkList.GetCount());
	}
	else
	{
		// Standard inventory => Check state to see if changed
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Checking last inventory state"));
		if (isStateChanged())
			m_pLogger->log( LOG_PRIORITY_NOTICE, _T( "INVENTORY => Inventory changed since last run"));
		else
			m_pLogger->log( LOG_PRIORITY_NOTICE, _T( "INVENTORY => No change since last inventory"));

		// Update the XML
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Generating XML document with Device properties"));
		// Update Memory slots
		bSuccess = m_pTheDB->UpdateMemorySlots( m_MemoryList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Logical Drive(s)"), m_MemoryList.GetCount());
		// Update Hardware 
		bSuccess = bSuccess && m_pTheDB->UpdateDeviceProperties( m_Device);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update common Device properties"));
		// Update BIOS file
		bSuccess = bSuccess && m_pTheDB->UpdateBIOS( m_BIOS);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update BIOS"));
		// Update Input Devices
		bSuccess = bSuccess && m_pTheDB->UpdateInputDevices( m_InputList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Input Device(s)"), m_InputList.GetCount());
		// Update System Ports
		bSuccess = bSuccess && m_pTheDB->UpdateSystemPorts( m_PortList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u System Port(s)"), m_PortList.GetCount());
		// Update System Controllers
		bSuccess = bSuccess && m_pTheDB->UpdateSystemControllers( m_SystemControllerList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u System Controler(s)"), m_SystemControllerList.GetCount());
		// Update System Slots
		bSuccess = bSuccess && m_pTheDB->UpdateSystemSlots( m_SlotList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u System Slot(s)"), m_SlotList.GetCount());
		// Update Sounds
		bSuccess = bSuccess && m_pTheDB->UpdateSounds( m_SoundList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Sound Device(s)"), m_SoundList.GetCount());
		// Update Storages
		bSuccess = bSuccess && m_pTheDB->UpdateStorages( m_StorageList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Storage Peripheral(s)"), m_StorageList.GetCount());
		// Update Logical Drives
		bSuccess = bSuccess && m_pTheDB->UpdateDrives( m_DriveList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Logical Drive(s)"), m_DriveList.GetCount());
		// Update Modems
		bSuccess = bSuccess && m_pTheDB->UpdateModems( m_ModemList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Modem(s)"), m_ModemList.GetCount());
		// Update Networks
		bSuccess = bSuccess && m_pTheDB->UpdateNetworks( m_NetworkList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Network Adapter(s)"), m_NetworkList.GetCount());
		// Update Videos
		bSuccess = bSuccess && m_pTheDB->UpdateVideos( m_VideoList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Video Adapter(s)"), m_VideoList.GetCount());
		// Update Monitors
		bSuccess = bSuccess && m_pTheDB->UpdateMonitors( m_MonitorList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Monitor(s)"), m_MonitorList.GetCount());
		// Update Printers
		bSuccess = bSuccess && m_pTheDB->UpdatePrinters( m_PrinterList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Printer(s)"), m_PrinterList.GetCount());
		// Update Softwares
		if (!getAgentConfig()->isNoSoftwareRequired())
		{
			bSuccess = bSuccess && m_pTheDB->UpdateSoftwares( m_SoftwareList);
			m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Software"), m_SoftwareList.GetCount());
		}
		// Update Registry values
		bSuccess = bSuccess && m_pTheDB->UpdateRegistryValues( m_RegistryList);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update %u Registry Value(s)"), m_RegistryList.GetCount());
		// Update Administrative Informations
		csFilename.Format( _T("%s\\%s"), getDataFolder(), OCS_ACCOUNTINFO_FILENAME);
		bSuccess = bSuccess && m_pTheDB->UpdateAccountInfo( csFilename);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => XML Update Administrative Information(s)"));
	}

	if (!bSuccess)
		m_pLogger->log( LOG_PRIORITY_ERROR, _T( "INVENTORY => XML Update Device properties failed"));
	bSuccess = bSuccess && CRequestAbstract::final();
	return bSuccess;
}

BOOL CInventoryRequest::writeLastInventoryState()
{
	if (m_bNotify)
		// Do not write inventory state changes in NOTIFY mode
		return TRUE;

	// Write inventory state
	m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Writing new inventory state"));
	CString csFileName;
	csFileName.Format( _T( "%s\\%s"), getDataFolder(), OCS_LAST_STATE_FILE);
	return m_pState->WriteToFile( csFileName);
}

void CInventoryRequest::setTag(CString csTag)
{
	m_csTag = csTag;
}

BOOL CInventoryRequest::isStateChanged()
{
	CString	csBuffer;
	ULONG	ulChecksum = 0;

	// Checking if hardware changes
	csBuffer = m_Device.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetHardware()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_HARDWARE;
		m_pState->SetHardware( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Hardware inventory state changed"));
	}
	// Checking if bios changes
	csBuffer = m_BIOS.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetBios()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_BIOS;
		m_pState->SetBios( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Bios inventory state changed"));
	}
	// Checking if memories changes
	csBuffer = m_MemoryList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetMemories()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_MEMORIES;
		m_pState->SetMemories( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Memory slots inventory state changed"));
	}
	// Checking if slots changes
	csBuffer = m_SlotList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetSlots()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_SLOTS;
		m_pState->SetSlots( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => System slots inventory state changed"));
	}
	// Checking if registry changes
	csBuffer = m_RegistryList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetRegistry()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_REGISTRY;
		m_pState->SetRegistry( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Registry inventory state changed"));
	}
	// Checking if controllers changes
	csBuffer = m_SystemControllerList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetControllers()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_CONTROLLERS;
		m_pState->SetControllers( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => System controllers inventory state changed"));
	}
	// Checking if monitors changes
	csBuffer = m_MonitorList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetMonitors()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_MONITORS;
		m_pState->SetMonitors( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Monitors inventory state changed"));
	}
	// Checking if ports changes
	csBuffer = m_PortList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetPorts()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_PORTS;
		m_pState->SetPorts( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => System ports inventory state changed"));
	}
	// Checking if storages changes
	csBuffer = m_StorageList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetStorages()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_STORAGES;
		m_pState->SetStorages( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Storage peripherals inventory state changed"));
	}
	// Checking if drives changes
	csBuffer = m_DriveList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetDrives()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_DRIVES;
		m_pState->SetDrives( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Logical drives inventory state changed"));
	}
	// Checking if inputs changes
	csBuffer = m_InputList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetInputs()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_INPUTS;
		m_pState->SetInputs( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Input peripherals inventory state changed"));
	}
	// Checking if modems changes
	csBuffer = m_ModemList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetModems()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_MODEMS;
		m_pState->SetModems( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Modems inventory state changed"));
	}
	// Checking if networks changes
	csBuffer = m_NetworkList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetNetworks()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_NETWORKS;
		m_pState->SetNetworks( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => etwork adapters inventory state changed"));
	}
	// Checking if printers changes
	csBuffer = m_PrinterList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetPrinters()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_PRINTERS;
		m_pState->SetPrinters( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Printers inventory state changed"));
	}
	// Checking if sounds changes
	csBuffer = m_SoundList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetSounds()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_SOUNDS;
		m_pState->SetSounds( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Sound adapters inventory state changed"));
	}
	// Checking if videos changes
	csBuffer = m_VideoList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetVideos()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_VIDEOS;
		m_pState->SetVideos( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Video adapters inventory state changed"));
	}
	// Checking if softwares changes
	csBuffer = m_SoftwareList.GetHash();
	if (csBuffer.CompareNoCase( m_pState->GetSoftwares()) != 0)
	{
		// Changed
		ulChecksum += OCS_CHECKSUM_SOFTWARES;
		m_pState->SetSoftwares( csBuffer);
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Software inventory state changed"));
	}
	m_Device.SetChecksum( ulChecksum);
	// if change
	if (ulChecksum)
		return TRUE;
	return FALSE;
}

/* Add ocs packages to a dedicated section */
BOOL CInventoryRequest::loadDownloadHistory()
{
	CFilePackageHistory  fileHistory;
	CString		csBuffer;
	short		errCnt			= 0;
	BOOL		historyOpened	= FALSE;

	while( errCnt<3 )
	{
		if (fileHistory.Open( getPackageHistoryFilename()))
		{
			historyOpened = TRUE;
			break;
		}
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Cannot open Download Package history file, retrying in 500ms"));
		Sleep(500);
		errCnt++;
	}

	getXmlPointerContent();
	m_cmXml.AddElem( _T("DOWNLOAD"));
	m_cmXml.AddElem( _T("HISTORY"));

	if( historyOpened )
	{
		while( fileHistory.ReadPackage( csBuffer))
		{
			// Chomp the string
			m_pLogger->log( LOG_PRIORITY_DEBUG, _T("INVENTORY => Adding Download Package <%s> to report"), csBuffer);
			m_cmXml.AddElem( _T("PACKAGE"));
			m_cmXml.AddAttrib( _T("ID"), csBuffer);
			m_cmXml.OutOfElem();
		}
		fileHistory.Close();
	}
	else
	{
		return FALSE;
	}
	return TRUE;
}

BOOL CInventoryRequest::runInventory()
{
	CString		cs1, cs2, cs3, cs4, cs5;
	DWORD		dwValue;
	ULONG		ulPhysicalMemory, ulSwapSize;
	BOOL		bRunBiosInfo = TRUE;
	CSoftware	cSoftOS;

	// Get logged on user
	if (!m_pSysInfo->getUserName( cs1))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve logged on user"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Logged on user ID is <%s>"), cs1);
	m_Device.SetLoggedOnUser( cs1);
	// Last logged on user
	m_pSysInfo->getLastLoggedUser( cs1);
	m_Device.SetLastLoggedUser( cs1);
	// Get OS informations and device type (windows station or windows server)
	if (!m_pSysInfo->getOS( cs1, cs2, cs3, cs4, cs5))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve Operating System"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Operating System is <%s %s %s>, description <%s>"), 
					cs1, cs2, cs3, cs4);
	// Fix for some Windows 2000 pro without OS name
	if ((cs1.GetLength() == 0) && (cs2== _T( "5.0.2195")))
		cs1 = _T( "Microsoft Windows 2000 Professional");
	m_Device.SetOS( cs1, cs2, cs3);
	m_Device.SetDescription (cs4);
	// Get OS Address width
	m_Device.SetAddressWidthOS( m_pSysInfo->getAddressWidthOS());
	m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Operating System uses %u bits memory address width"),  m_pSysInfo->getAddressWidthOS());
	// Prepare to also store OS information to software list
	cSoftOS.Set( MICROSOFT_CORP_STRING, cs1, cs2, NOT_AVAILABLE, cs3, NOT_AVAILABLE, 0, TRUE);
	cSoftOS.SetInstallDate( cs5, TRUE);
	cSoftOS.SetMemoryAddressWidth( m_pSysInfo->getAddressWidthOS());
	// Get NT Domain or Workgroup
	if (!m_pSysInfo->getDomainOrWorkgroup( cs1))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve computer domain or workgroup"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Computer domain or workgroup is <%s>"), 
					cs1);
	m_Device.SetDomainOrWorkgroup( cs1);
	// Get NT user Domain
	if (!m_pSysInfo->getUserDomain( cs1))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve user domain"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => User domain is <%s>"), 
					cs1);
	m_Device.SetUserDomain( cs1);
	// Get BIOS informations
	if (!m_pSysInfo->getBiosInfo( &m_BIOS))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve BIOS"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => System Manufacturer <%s>, System Model <%s>, System S/N <%s>, Bios Manufacturer <%s>, Bios Date <%s>, Bios Version <%s>"),
					  m_BIOS.GetSystemManufacturer(), m_BIOS.GetSystemModel(), m_BIOS.GetSystemSerialNumber(),
					  m_BIOS.GetBiosManufacturer(), m_BIOS.GetBiosDate(), m_BIOS.GetBiosVersion());
	// Get Processor infos (0 means error)
	if (!(dwValue = m_pSysInfo->getProcessors( cs1, cs2)))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve processor(s)"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %lu processor(s) %s at %s MHz"),
					  dwValue, cs1, cs2);
	m_Device.SetProcessor( cs1, cs2, dwValue);
	// Get memory informations
	if (!m_pSysInfo->getMemory( &ulPhysicalMemory, &ulSwapSize))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve memory informations from OS"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => OS Memory %lu MB, OS Swap size %lu MB"),
					  ulPhysicalMemory, ulSwapSize);
	m_Device.SetMemory( ulPhysicalMemory, ulSwapSize);
	if (!m_pSysInfo->getMemorySlots( &m_MemoryList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve memory slots"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d memory slot(s) found"),
					  m_MemoryList.GetCount());
	// Get Input Devices
	if (!m_pSysInfo->getInputDevices( &m_InputList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve input devices"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d input device(s) found"),
					  m_InputList.GetCount());
	// Get System ports
	if (!m_pSysInfo->getSystemPorts( &m_PortList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system ports"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d system port(s) found"),
					  m_PortList.GetCount());
	// Get System Slots
	if (!m_pSysInfo->getSystemSlots( &m_SlotList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system slots"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d system slot(s) found"),
					  m_SlotList.GetCount());
	// Get System controlers
	if (!m_pSysInfo->getSystemControllers( &m_SystemControllerList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system controlers"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d system controler(s) found"),
					  m_SystemControllerList.GetCount());
	// Get Physical storage devices
	if (!m_pSysInfo->getStoragePeripherals( &m_StorageList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve storage peripherals"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d storage peripheral(s) found"),
					  m_StorageList.GetCount());
	// Get Logical Drives
	if (!m_pSysInfo->getLogicalDrives( &m_DriveList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve logical drives"));
	// Get Sound Devices
	if (!m_pSysInfo->getSoundDevices( &m_SoundList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve sound devices"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d sound device(s) found"),
					  m_SoundList.GetCount());
	// Get Modems
	if (!m_pSysInfo->getModems( &m_ModemList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve modems"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d modem(s) found"),
					  m_ModemList.GetCount());
	// Get network adapter(s) hardware and IP informations
	if (!m_pSysInfo->getNetworkAdapters( &m_NetworkList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve network adapters"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d network adapter(s) found"),
					  m_NetworkList.GetCount());
	// Get Printer(s) informations
	if (!m_pSysInfo->getPrinters( &m_PrinterList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system printers"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d system printer(s) found"),
					  m_PrinterList.GetCount());
	// Get Video adapter(s) informations
	if (!m_pSysInfo->getVideoAdapters( &m_VideoList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve video adapters"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d video adapter(s) found"),
					  m_VideoList.GetCount());
	if (!m_pSysInfo->getMonitors( &m_MonitorList))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system monitors"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d system monitor(s) found"),
					  m_MonitorList.GetCount());
	// Get the primary local IP Address 
	m_Device.SetIPAddress( m_pSysInfo->getLocalIP());
	m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Default IPv4 address is <%s>"),
				  m_Device.GetIPAddress());
	// Get Windows registration infos
	if (!m_pSysInfo->getWindowsRegistration( cs1, cs2, cs3))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system registration"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Registered company <%s>, registered owner <%s>, Product ID <%.08s...>"),
					  cs1, cs2, cs3);
	m_Device.SetWindowsRegistration( cs1, cs2, cs3);
	// Get Windows product key
	if (!m_pSysInfo->getWindowsProductKey( cs1))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve system product key"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Product key <%.08s...>"),
					  cs1);
	m_Device.SetWindowsProductKey( cs1);
	// Get Software list
	if (!m_pSysInfo->getInstalledApplications( &m_SoftwareList, getAgentConfig()->isHkcuRequired()))
		m_pLogger->log( LOG_PRIORITY_WARNING, _T( "INVENTORY => Failed to retrieve software from Registry"));
	else
		m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => %d software found"),
						m_SoftwareList.GetCount());
	// Add OS to the list of detected software
	m_SoftwareList.AddTail( cSoftOS);
	// Verify total system memory
	ULONG ulMemTotal = m_MemoryList.GetTotalMemory();
	if (ulMemTotal > 0)
	{	
		ULONG ulMemStart = m_Device.GetPhysicalMemory();
		ULONG ulMemToSet = ulMemStart;
		ULONG ulDiff = abs( (long)(ulMemTotal - ulMemStart));

		if( ulDiff < 16 )
		{			
			m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => System Memory set to %lu (instead of %lu)"), ulMemTotal, ulMemStart);
			ulMemToSet = ulMemTotal;
		}
		else
			m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "INVENTORY => Bogus summed Memory Slots, %lu is too far from %lu (keeping the last one)"), ulMemTotal, ulMemStart);
		
		m_Device.SetMemory( ulMemToSet, m_Device.GetPageFileSize());
	}
	return TRUE;
}

LONG CInventoryRequest::SearchFilesInDirectory(LPCTSTR lpstrDir, LPCTSTR lpstrExtensionToSearch, LPCTSTR lpstrFoldersToExclude)
{
	CString		csDir = lpstrDir;
	CFileFind	cFinder;
	BOOL		bWorking;
	CSoftware	cApp;
	CString		csPublisher,
				csName,
				csVersion,
				csComment,
				csLanguage;
	LONG		lNumberOfFiles = 0;

	// Search for all files and directory
	csDir += "*.*";
	bWorking = cFinder.FindFile( csDir);
	while (bWorking)
	{
		bWorking = cFinder.FindNextFile();
		if (!cFinder.IsDots())
		{
			// Not "." or ".." directory
			if (cFinder.IsDirectory())
			{
				// This is a directory => recursive search if needed
				if (IsExcludeFolder( cFinder.GetFilePath(), lpstrFoldersToExclude))
				{
					// Folder to exclude from search => skip
					m_pLogger->log( LOG_PRIORITY_DEBUG, _T( "Skipping folder <%s>"), cFinder.GetFilePath());
				}
				else
				{
					csDir = cFinder.GetFilePath() + _T( "\\");
					SearchFilesInDirectory( csDir);
				}
			}
			else
			{
				// This a file => update total files number for this Device
				lNumberOfFiles ++;
				if (IsExtensionToSearch( cFinder.GetFileName(), lpstrExtensionToSearch))
				{
					// This is an apps to search
					if (!getFileVersion( cFinder.GetFilePath(), csPublisher, csName, csVersion, csComment, csLanguage))
						// Unable to get version infos => use filename
						csName = cFinder.GetFileName();
					StrForSQL( csName);
					if (csName.IsEmpty())
						// Version info do not contains app name => use filename
						csName = cFinder.GetFileName();
					cApp.Clear();
					cApp.Set( csPublisher, csName, csVersion, cFinder.GetRoot(), csComment, cFinder.GetFileName(), cFinder.GetLength());
					cApp.SetLanguage(csLanguage);
					m_SoftwareList.AddTail( cApp);
				}
			}
		}
	}
	return lNumberOfFiles;
}

BOOL CInventoryRequest::IsExtensionToSearch( LPCTSTR lpstrFilename, LPCTSTR lpstrExtension)
{
	CString	csFile = lpstrFilename,
			csExt,
			csData,
			csTemp,
			csBuffer = lpstrExtension;
	int		nPos;

	if ((lpstrExtension == NULL) || (_tcslen( lpstrExtension) == 0))
		return FALSE;
	csFile.MakeLower();
	if ((nPos = csFile.ReverseFind( '.')) == -1)
		return FALSE;
	csExt = csFile.Mid( nPos+1);
	while ((nPos = csBuffer.Find(_T( ","))) >= 0)
	{
		csData = csBuffer.Left( nPos);
		if (csExt.Compare( csData) == 0)
			return TRUE;
		csTemp = csBuffer.Mid( nPos + 1);
		csBuffer = csTemp;
	}
	if (csExt.Compare( csBuffer) == 0)
		return TRUE;
	return FALSE;
}

BOOL CInventoryRequest::IsExcludeFolder( LPCTSTR lpstrFolder, LPCTSTR lpstrFolderToSkip)
{
	CString	csFolder = lpstrFolder,
			csData,
			csTemp,
			csBuffer = lpstrFolderToSkip;
	int		nPos;


	if ((lpstrFolderToSkip == NULL) || (_tcslen( lpstrFolderToSkip) == 0))
		return FALSE;
	csFolder.MakeLower();
	while ((nPos = csBuffer.Find(_T( ","))) >= 0)
	{
		csData = csBuffer.Left( nPos);
		if (csFolder.Find( csData) >= 0)
			return TRUE;
		csTemp = csBuffer.Mid( nPos + 1);
		csBuffer = csTemp;
	}
	if (csFolder.Find( csBuffer) >= 0)
		return TRUE;
	return FALSE;
}