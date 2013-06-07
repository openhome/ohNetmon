#include "CpNetworkMonitorList1.h"

#include <OpenHome/Private/Debug.h>
#include "Debug.h"

using namespace OpenHome;
using namespace OpenHome::Net;

// CpNetworkMonitorList1Job

CpNetworkMonitorList1Job::CpNetworkMonitorList1Job(ICpNetworkMonitorList1Handler& aHandler)
{
	iHandler = &aHandler;
	iDevice = 0;
}
	
void CpNetworkMonitorList1Job::Set(CpDevice& aDevice, ICpNetworkMonitorList1HandlerFunction aFunction)
{
	iDevice = &aDevice;
	iFunction = aFunction;
	iDevice->AddRef();
}

void CpNetworkMonitorList1Job::Execute()
{
	if (iDevice) {
		(iHandler->*iFunction)(*iDevice);
		iDevice->RemoveRef();
		iDevice = 0;
	}
	else {
		THROW(ThreadKill);
	}
}

// CpNetworkMonitorList1

CpNetworkMonitorList1::CpNetworkMonitorList1(CpStack& aCpStack, ICpNetworkMonitorList1Handler& aHandler)
	: iFree(kMaxJobCount)
	, iReady(kMaxJobCount)
{
	for (TUint i = 0; i < kMaxJobCount; i++) {
		iFree.Write(new CpNetworkMonitorList1Job(aHandler));
	}
	
    FunctorCpDevice NetworkMonitorAdded = MakeFunctorCpDevice(*this, &CpNetworkMonitorList1::NetworkMonitorAdded);
    FunctorCpDevice NetworkMonitorRemoved = MakeFunctorCpDevice(*this, &CpNetworkMonitorList1::NetworkMonitorRemoved);
    
    iDeviceListNetworkMonitor = new CpDeviceListUpnpServiceType(aCpStack, Brn("av.openhome.org"), Brn("NetworkMonitor"), 1, NetworkMonitorAdded, NetworkMonitorRemoved);

	iThread = new ThreadFunctor("NML1", MakeFunctor(*this, &CpNetworkMonitorList1::Run));

	iThread->Start();
}

CpNetworkMonitorList1::~CpNetworkMonitorList1()
{
	delete (iDeviceListNetworkMonitor);
	
	iReady.Write(iFree.Read());

	delete (iThread);
	
	for (TUint i = 0; i < kMaxJobCount; i++) {
		delete (iFree.Read());
	}
}
    
void CpNetworkMonitorList1::Refresh()
{
	iDeviceListNetworkMonitor->Refresh();
}
    
void CpNetworkMonitorList1::NetworkMonitorAdded(CpDevice& aDevice)
{
    LOG(kTopology, "CpNetworkMonitorList1::NetworkMonitorAdded ");
    LOG(kTopology, aDevice.Udn());
    LOG(kTopology, "\n");

	CpNetworkMonitorList1Job* job = iFree.Read();
	job->Set(aDevice, &ICpNetworkMonitorList1Handler::NetworkMonitorAdded);
	iReady.Write(job);
}

void CpNetworkMonitorList1::NetworkMonitorRemoved(CpDevice& aDevice)
{
    LOG(kTopology, "CpNetworkMonitorList1::NetworkMonitorRemoved ");
    LOG(kTopology, aDevice.Udn());
    LOG(kTopology, "\n");
    
	CpNetworkMonitorList1Job* job = iFree.Read();
	job->Set(aDevice, &ICpNetworkMonitorList1Handler::NetworkMonitorRemoved);
	iReady.Write(job);
}

void CpNetworkMonitorList1::Run()
{
    for (;;)
    {
    	CpNetworkMonitorList1Job* job = iReady.Read();
    	
    	try {
	    	job->Execute();
	    	iFree.Write(job);
	    }
	    catch (ThreadKill)
	    {
	    	iFree.Write(job);
	    	break;
	    }
    }
}
