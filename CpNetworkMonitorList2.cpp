#include "CpNetworkMonitorList2.h"

#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Uri.h>

using namespace OpenHome;
using namespace OpenHome::Net;

// CpNetworkMonitor

CpNetworkMonitor::CpNetworkMonitor(const Brx& aName, TIpAddress aAddress, TUint aSender, TUint aReceiver, TUint aResults)
	: iName(aName)
	, iAddress(aAddress)
	, iSender(aSender)
	, iReceiver(aReceiver)
	, iResults(aResults)
	, iRefCount(1)
{
}

const Brx& CpNetworkMonitor::Name() const
{
	return (iName);
}

TIpAddress CpNetworkMonitor::Address() const
{
	return (iAddress);
}

TUint CpNetworkMonitor::Sender() const
{
	return (iSender);
}

TUint CpNetworkMonitor::Receiver() const
{
	return (iReceiver);
}

TUint CpNetworkMonitor::Results() const
{
	return (iResults);
}

void CpNetworkMonitor::AddRef()
{
	iRefCount++;
}

void CpNetworkMonitor::RemoveRef()
{
	if (--iRefCount == 0) {
		delete (this);
	}
}



// CpNetworkMonitorList2Device

CpNetworkMonitorList2Device::CpNetworkMonitorList2Device(CpDevice& aDevice, ICpNetworkMonitorList2Handler& aHandler)
    : iDevice(aDevice)
	, iHandler(aHandler)
	, iNetworkMonitor(0)
{
    iDevice.AddRef();

	Brh location;

	iDevice.GetAttribute("Upnp.Location", location);

	Uri uri(location);

	Endpoint endpoint(0, uri.Host());

	iAddress = endpoint.Address();

	iService = new CpProxyAvOpenhomeOrgNetworkMonitor1(iDevice);

	Functor functorInitial = MakeFunctor(*this, &CpNetworkMonitorList2Device::EventInitialEvent);

    iService->SetPropertyInitialEvent(functorInitial);

	iService->Subscribe();
}
	
TBool CpNetworkMonitorList2Device::IsAttachedTo(CpDevice& aDevice)
{
	return (iDevice.Udn() == aDevice.Udn());
}

void CpNetworkMonitorList2Device::EventInitialEvent()
{
    Functor functorName = MakeFunctor(*this, &CpNetworkMonitorList2Device::EventNameChanged);

    iService->SetPropertyNameChanged(functorName);    

	Brhz name;
	
	iService->PropertyName(name);
	iService->PropertySender(iSender);
	iService->PropertyReceiver(iReceiver);
	iService->PropertyResults(iResults);
	
	iNetworkMonitor = new CpNetworkMonitor(name, iAddress, iSender, iReceiver, iResults);

	iHandler.NetworkMonitorAdded(*iNetworkMonitor);
}

void CpNetworkMonitorList2Device::EventNameChanged()
{
	Brhz name;

	iService->PropertyName(name);
	
	iHandler.NetworkMonitorRemoved(*iNetworkMonitor);
	iNetworkMonitor->RemoveRef();
	iNetworkMonitor = new CpNetworkMonitor(name, iAddress, iSender, iReceiver, iResults);
	iHandler.NetworkMonitorAdded(*iNetworkMonitor);
}

CpNetworkMonitorList2Device::~CpNetworkMonitorList2Device()
{
	delete (iService);

	if (iNetworkMonitor != 0) {
		iHandler.NetworkMonitorRemoved(*iNetworkMonitor);
		iNetworkMonitor->RemoveRef();
	}

    iDevice.RemoveRef();
}
	
// CpNetworkMonitorList2Job

CpNetworkMonitorList2Job::CpNetworkMonitorList2Job(ICpNetworkMonitorList2Handler& aHandler)
{
	iHandler = &aHandler;
	iNetworkMonitor = 0;
}
	
void CpNetworkMonitorList2Job::Set(CpNetworkMonitor& aNetworkMonitor, ICpNetworkMonitorList2HandlerFunction aFunction)
{
	iNetworkMonitor = &aNetworkMonitor;
	iFunction = aFunction;
	iNetworkMonitor->AddRef();
}

void CpNetworkMonitorList2Job::Execute()
{
	if (iNetworkMonitor) {
		(iHandler->*iFunction)(*iNetworkMonitor);
		iNetworkMonitor->RemoveRef();
		iNetworkMonitor = 0;
	}
	else {
		THROW(ThreadKill);
	}
}

// CpNetworkMonitorList2

CpNetworkMonitorList2::CpNetworkMonitorList2(CpStack& aCpStack, ICpNetworkMonitorList2Handler& aHandler)
	: iFree(kMaxJobCount)
	, iReady(kMaxJobCount)
{
	for (TUint i = 0; i < kMaxJobCount; i++) {
		iFree.Write(new CpNetworkMonitorList2Job(aHandler));
	}
	
	iNetworkMonitorList1 = new CpNetworkMonitorList1(aCpStack, *this);
	
	iThread = new ThreadFunctor("NML2", MakeFunctor(*this, &CpNetworkMonitorList2::Run));

	iThread->Start();
}

CpNetworkMonitorList2::~CpNetworkMonitorList2()
{
    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2\n");

	delete (iNetworkMonitorList1);
    
    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted layer 1\n");

    std::vector<CpNetworkMonitorList2Device*>::iterator it = iDeviceList.begin();

    while (it != iDeviceList.end()) {
        delete (*it);
        it++;
    }   

    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted devices\n");

	iReady.Write(iFree.Read()); // this null job causes the thread to complete

	delete (iThread);
	
    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted thread\n");

	for (TUint i = 0; i < kMaxJobCount; i++) {
		delete (iFree.Read());
	}

    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted jobs\n");
}
    
void CpNetworkMonitorList2::Refresh()
{
	iNetworkMonitorList1->Refresh();
}

// ICpNetworkMonitorList1Handler
    
void CpNetworkMonitorList2::NetworkMonitorAdded(CpDevice& aDevice)
{
    iDeviceList.push_back(new CpNetworkMonitorList2Device(aDevice, *this));
}

void CpNetworkMonitorList2::NetworkMonitorRemoved(CpDevice& aDevice)
{
    DeviceRemoved(aDevice);
}

void CpNetworkMonitorList2::DeviceRemoved(CpDevice& aDevice)
{
    std::vector<CpNetworkMonitorList2Device*>::iterator it = iDeviceList.begin();

    while (it != iDeviceList.end()) {
        if ((*it)->IsAttachedTo(aDevice)) {
            LOG(kTopology, "CpNetworkMonitorList2::NetworkMonitorRemoved found\n");
            delete (*it);
            iDeviceList.erase(it);
            break;
        }
        it++;
    }   
}

// ICpNetworkMonitorList2Handler

void CpNetworkMonitorList2::NetworkMonitorAdded(CpNetworkMonitor& aNetworkMonitor)
{
	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aNetworkMonitor, &ICpNetworkMonitorList2Handler::NetworkMonitorAdded);
	iReady.Write(job);
}

void CpNetworkMonitorList2::NetworkMonitorRemoved(CpNetworkMonitor& aNetworkMonitor)
{
	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aNetworkMonitor, &ICpNetworkMonitorList2Handler::NetworkMonitorRemoved);
	iReady.Write(job);
}

void CpNetworkMonitorList2::Run()
{
    LOG(kTopology, "CpNetworkMonitorList2::Run Started\n");

    for (;;)
    {
	    LOG(kTopology, "CpNetworkMonitorList2::Run wait for job\n");

    	CpNetworkMonitorList2Job* job = iReady.Read();
    	
	    LOG(kTopology, "CpNetworkMonitorList2::Run execute job\n");

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

    LOG(kTopology, "CpNetworkMonitorList2::Run Exiting\n");
}
