#ifndef HEADER_TOPOLOGY2
#define HEADER_TOPOLOGY2

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Thread.h>

#include <vector>

#include "CpNetworkMonitorList1.h"

namespace OpenHome {
namespace Net {

class CpNetworkMonitorList2Device;

class CpNetworkMonitor
{
	static const TUint kMaxNameBytes = 41;

	friend class CpNetworkMonitorList2Device;

public:
    const Brx& Name() const;
	TIpAddress Address() const;
	TUint Sender() const;
	TUint Receiver() const;
	TUint Results() const;
	void AddRef();
	void RemoveRef();
private:
    CpNetworkMonitor(const Brx& aName, TIpAddress aAddress, TUint aSender, TUint aReceiver, TUint aResults);
    
private:
	Bws<kMaxNameBytes> iName;
	TIpAddress iAddress;
	TUint iSender;
	TUint iReceiver;
	TUint iResults;
	TUint iRefCount;
};

class ICpNetworkMonitorList2Handler
{
public:
	virtual void NetworkMonitorAdded(CpNetworkMonitor& aNetworkMonitor) = 0;
	virtual void NetworkMonitorRemoved(CpNetworkMonitor& aNetworkMonitor) = 0;
	virtual ~ICpNetworkMonitorList2Handler() {}
};

typedef void (ICpNetworkMonitorList2Handler::*ICpNetworkMonitorList2HandlerFunction)(CpNetworkMonitor&);

class CpNetworkMonitorList2Job
{
public:
	CpNetworkMonitorList2Job(ICpNetworkMonitorList2Handler& aHandler);
	void Set(CpNetworkMonitor& aNetworkMonitor, ICpNetworkMonitorList2HandlerFunction aFunction);
    virtual void Execute();
    virtual ~CpNetworkMonitorList2Job();
private:
	ICpNetworkMonitorList2Handler* iHandler;
	CpNetworkMonitor* iNetworkMonitor;
	ICpNetworkMonitorList2HandlerFunction iFunction;
};

class CpProxyAvOpenhomeOrgNetworkMonitor1;

class CpNetworkMonitorList2Device : public INonCopyable
{
public:
	CpNetworkMonitorList2Device(CpDevice& aDevice, ICpNetworkMonitorList2Handler& aHandler);
	
public:
	TBool IsAttachedTo(CpDevice& aDevice);
	~CpNetworkMonitorList2Device();

private:
	void EventInitialEvent();
	void EventNameChanged();

private:
	CpDevice& iDevice;
	ICpNetworkMonitorList2Handler& iHandler;
	TIpAddress iAddress;
	CpNetworkMonitor* iNetworkMonitor;
	CpProxyAvOpenhomeOrgNetworkMonitor1* iService;
	TUint iSender;
	TUint iReceiver;
	TUint iResults;
};

class CpStack;

class CpNetworkMonitorList2 : public ICpNetworkMonitorList1Handler, public ICpNetworkMonitorList2Handler
{
	static const TUint kMaxJobCount = 20;
	
public:
	CpNetworkMonitorList2(CpStack& aCpStack, ICpNetworkMonitorList2Handler& aHandler);

	void Refresh();
    
	~CpNetworkMonitorList2();
    
private:
	// ICpNetworkMonitorList1Handler
	virtual void NetworkMonitorAdded(CpDevice& aDevice);
	virtual void NetworkMonitorRemoved(CpDevice& aDevice);

	void DeviceRemoved(CpDevice& aDevice);

	// ICpNetworkMonitorList2Handler
	void NetworkMonitorAdded(CpNetworkMonitor& aNetworkMonitor);
	void NetworkMonitorRemoved(CpNetworkMonitor& aNetworkMonitor);

	void Run();
	
private:
	CpNetworkMonitorList1* iNetworkMonitorList1;
	Fifo<CpNetworkMonitorList2Job*> iFree;
	Fifo<CpNetworkMonitorList2Job*> iReady;
	ThreadFunctor* iThread;
    std::vector<CpNetworkMonitorList2Device*> iDeviceList;
};

} // namespace Net
} // namespace OpenHome

#endif // HEADER_TOPOLOGY2
