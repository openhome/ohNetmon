#ifndef HEADER_NETWORK_MONITOR
#define HEADER_NETWORK_MONITOR

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Net/Core/DvDevice.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Timer.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Net/Core/DvAvOpenhomeOrgNetworkMonitor1.h>

namespace OpenHome {
namespace Net {

class NetworkMonitorEvent
{
public:
    NetworkMonitorEvent();
    NetworkMonitorEvent(Brx& aBuf);
    TBool IsEmpty() const;
	Brn Buffer() const;
private:
    TUint iId;
    TUint iFrame;
    TUint iTxTimestamp;
    TUint iRxTimestamp;
};

class NetworkMonitorReceiver
{
    static const TUint kEventQueueLength = 65536;
    static const TUint kMaxMessageBytes = 65536;
    static const TUint kReceiverPort = 8889;
    static const TUint kResultsPort  = 8889;
public:
    NetworkMonitorReceiver();
	TUint ReceiverPort() const;
	TUint ResultsPort() const;
    ~NetworkMonitorReceiver();
    void Run();
private:
    SocketUdp iSocket;
    SocketTcpServer iServer;
    Fifo<NetworkMonitorEvent> iFifo;
    Bws<kMaxMessageBytes> iBuffer;
	ThreadFunctor* iThread;
};

class NetworkMonitorSender
{
    static const TUint kMaxMessageBytes = 65536;
public:
    NetworkMonitorSender();
	TUint SenderPort() const;
    void Start(Endpoint aEndpoint, TUint aPeriodUs, TUint aBytes, TUint aIterations, TUint aTtl, TUint aId);
    void Stop();
private:
    void TimerExpired();
private:
    SocketUdp iSocket;
    SocketTcpServer iServer;
    Timer iTimer;
    Endpoint iEndpoint;
    TUint iPeriodMs;
    TUint iBytes;
    TUint iIterations;
    TUint iTtl;
    TUint iId;
    TUint iFrame;
    TUint iNext;
    Bws<kMaxMessageBytes> iBuffer;
};

class NetworkMonitorProvider : public DvProviderAvOpenhomeOrgNetworkMonitor1
{
	static const TUint kMaxNameBytes = 100;
public:
	NetworkMonitorProvider(DvDevice& aDevice, const Brx& aName, TUint aSenderPort, TUint aReceiverPort, TUint aResultsPort);
	void SetName(const Brx& aValue);
private:
	virtual void Name(IDvInvocation& aInvocation, IDvInvocationResponseString& aValue);
	virtual void Ports(IDvInvocation& aInvocation, IDvInvocationResponseUint& aSender, IDvInvocationResponseUint& aReceiver, IDvInvocationResponseUint& aResults);
};

class NetworkMonitor
{
public:
    NetworkMonitor(DvDevice& aDevice, const Brx& aName);
	void SetName(const Brx& aValue);
	~NetworkMonitor();
private:
    NetworkMonitorProvider* iProvider;
    NetworkMonitorSender iSender;
    NetworkMonitorReceiver iReceiver;
};

} // namespace Net
} // namespace OpenHome

#endif // HEADER_NETWORK_MONITOR

