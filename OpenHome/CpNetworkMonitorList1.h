#ifndef HEADER_TOPOLOGY1
#define HEADER_TOPOLOGY1

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Net/Core/CpDeviceUpnp.h>
#include <OpenHome/Net/Core/FunctorCpDevice.h>

namespace OpenHome {
namespace Net {

class ICpNetworkMonitorList1Handler
{
public:
	virtual void NetworkMonitorAdded(CpDevice& aDevice) = 0;
	virtual void NetworkMonitorRemoved(CpDevice& aDevice) = 0;
	virtual ~ICpNetworkMonitorList1Handler() {}
};

typedef void (ICpNetworkMonitorList1Handler::*ICpNetworkMonitorList1HandlerFunction)(CpDevice&);

class CpNetworkMonitorList1Job
{
public:
	CpNetworkMonitorList1Job(ICpNetworkMonitorList1Handler& aHandler);
	void Set(CpDevice& aDevice, ICpNetworkMonitorList1HandlerFunction aFunction);
    virtual void Execute();
    virtual ~CpNetworkMonitorList1Job();
private:
	ICpNetworkMonitorList1Handler* iHandler;
	CpDevice* iDevice;
	ICpNetworkMonitorList1HandlerFunction iFunction;
};

class CpStack;

class CpNetworkMonitorList1
{
	static const TUint kMaxJobCount = 20;
	
public:
	CpNetworkMonitorList1(CpStack& aCpStack, ICpNetworkMonitorList1Handler& aHandler);
	
    void Refresh();
    
    ~CpNetworkMonitorList1();
    
private:
	void NetworkMonitorAdded(CpDevice& aDevice);
	void NetworkMonitorRemoved(CpDevice& aDevice);

	void Run();
	
private:
	CpDeviceList* iDeviceListNetworkMonitor;
	Fifo<CpNetworkMonitorList1Job*> iFree;
	Fifo<CpNetworkMonitorList1Job*> iReady;
	ThreadFunctor* iThread;
};

} // namespace Net
} // namespace OpenHome

#endif // HEADER_TOPOLOGY1
