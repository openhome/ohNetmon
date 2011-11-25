#ifndef HEADER_TOPOLOGY2
#define HEADER_TOPOLOGY2

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Net/Core/CpAvOpenhomeOrgNetworkMonitor1.h>

#include <vector>

#include "CpNetworkMonitorList1.h"

namespace OpenHome {
namespace Net {

class CpNetworkMonitor
{
	static const TUint kMaxNameBytes = 41;

public:
    const Brx& Name() const;
	TUint Sender() const;
	TUint Receiver() const;
	TUint Results() const;

private:
    CpNetworkMonitor(const Brx& aName, TUint aSender, TUint aReceiver, TUint aResults);

	void Update(const Brx& aName);
    
private:
	Bws<kMaxNameBytes> iName;
	TUint iSender;
	TUint iReceiver;
	TUint iResults;
};

class ICpNetworkMonitorList2Handler
{
public:
	virtual void NetworkManagerAdded(CpNetworkMonitor& aNetworkMonitor) = 0;
	virtual void NetworkManagerChanged(CpNetworkMonitor& aNetworkMonitor) = 0;
	virtual void NetworkManagerRemoved(CpNetworkMonitor& aNetworkMonitor) = 0;
	~ICpNetworkMonitorList2Handler() {}
};

typedef void (ICpNetworkMonitorList2Handler::*ICpNetworkMonitorList2HandlerFunction)(CpNetworkMonitorList2Group&);

class CpNetworkMonitorList2Job
{
public:
	CpNetworkMonitorList2Job(ICpNetworkMonitorList2Handler& aHandler);
	void Set(CpNetworkMonitorList2Group& aGroup, ICpNetworkMonitorList2HandlerFunction aFunction);
    virtual void Execute();
private:
	ICpNetworkMonitorList2Handler* iHandler;
	CpNetworkMonitorList2Group* iGroup;
	ICpNetworkMonitorList2HandlerFunction iFunction;
};

class CpNetworkMonitorList2Device : public INonCopyable, public ICpNetworkMonitorList2Handler
{
protected:
	CpNetworkMonitorList2Device(CpDevice& aDevice, ICpNetworkMonitorList2Handler aHandler);
	
public:
	TBool IsAttachedTo(CpDevice& aDevice);
	virtual ~CpNetworkMonitorList2Device();

private:
	// ICpTopology2GroupHandler
    virtual void SetSourceIndex(TUint aIndex) = 0;
    virtual void SetStandby(TBool aValue) = 0;

protected:
	CpDevice& iDevice;
	ICpNetworkMonitorList2Handler iHandler;
};

class CpNetworkMonitorList2 : public ICpNetworkMonitorList1Handler, public ICpNetworkMonitorList2Handler
{
	static const TUint kMaxJobCount = 20;
	
public:
	CpNetworkMonitorList2(ICpNetworkMonitorList2Handler& aHandler);

	void Refresh();
    
	~CpNetworkMonitorList2();
    
private:
	// ICpNetworkMonitorList1Handler
	virtual void NetworkMonitorAdded(CpDevice& aDevice);
	virtual void NetworkMonitorRemoved(CpDevice& aDevice);

	void DeviceRemoved(CpDevice& aDevice);

	// ICpNetworkMonitorList2Handler
	void NetworkManagerAdded(CpNetworkMonitor& aNetworkMonitor);
	void NetworkManagerChanged(CpNetworkMonitor& aNetworkMonitor);
	void NetworkManagerRemoved(CpNetworkMonitor& aNetworkMonitor);

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
