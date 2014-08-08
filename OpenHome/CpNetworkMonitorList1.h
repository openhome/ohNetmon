#ifndef HEADER_TOPOLOGY1
#define HEADER_TOPOLOGY1

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Net/Core/CpDeviceUpnp.h>
#include <OpenHome/Net/Core/FunctorCpDevice.h>

namespace OpenHome {
namespace Net {
    class CpStack;
}
namespace Av {

class ICpNetworkMonitorList1Handler
{
public:
	virtual void NetworkMonitorAdded(Net::CpDevice& aDevice) = 0;
	virtual void NetworkMonitorRemoved(Net::CpDevice& aDevice) = 0;
	virtual ~ICpNetworkMonitorList1Handler() {}
};

typedef void (ICpNetworkMonitorList1Handler::*ICpNetworkMonitorList1HandlerFunction)(Net::CpDevice&);

class CpNetworkMonitorList1Job
{
public:
	CpNetworkMonitorList1Job(ICpNetworkMonitorList1Handler& aHandler);
	void Set(Net::CpDevice& aDevice, ICpNetworkMonitorList1HandlerFunction aFunction);
    virtual void Execute();
    virtual ~CpNetworkMonitorList1Job();
private:
	ICpNetworkMonitorList1Handler* iHandler;
	Net::CpDevice* iDevice;
	ICpNetworkMonitorList1HandlerFunction iFunction;
};

class CpNetworkMonitorList1
{
	static const TUint kMaxJobCount = 20;
	
public:
	CpNetworkMonitorList1(Net::CpStack& aCpStack, ICpNetworkMonitorList1Handler& aHandler);
	
    void Refresh();
    
    ~CpNetworkMonitorList1();
    
private:
	void NetworkMonitorAdded(Net::CpDevice& aDevice);
	void NetworkMonitorRemoved(Net::CpDevice& aDevice);

	void Run();
	
private:
	Net::CpDeviceList* iDeviceListNetworkMonitor;
	Fifo<CpNetworkMonitorList1Job*> iFree;
	Fifo<CpNetworkMonitorList1Job*> iReady;
	ThreadFunctor* iThread;
};

} // namespace Av
} // namespace OpenHome

#endif // HEADER_TOPOLOGY1
