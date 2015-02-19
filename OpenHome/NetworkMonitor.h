#ifndef HEADER_NETWORK_MONITOR
#define HEADER_NETWORK_MONITOR

#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Buffer.h>
#include <OpenHome/Net/Core/DvDevice.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/Fifo.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Stream.h>

EXCEPTION(NetworkMonitorEventInvalid);

namespace OpenHome {
class Environment;
class Timer;
namespace Av {

class NetworkMonitorEvent
{
public:
    NetworkMonitorEvent();
    void Set(Environment& aEnv, Brx& aBuf);
    TBool IsEmpty() const;
    void AsBuffer(Bwx& aBuf) const;
private:
    TUint iId;
    TUint iFrame;
    TUint iTxTimestamp;
    TUint iRxTimestamp;
};

class NetworkMonitorReceiver
{
    static const TUint kEventQueueLength = 100;
    static const TUint kMaxMessageBytes = 65536;
public:
    NetworkMonitorReceiver(Environment& aEnv);
    ~NetworkMonitorReceiver();
	TUint ReceiverPort() const;
	TUint ResultsPort() const;
private:
    void Run();
private:
    Environment& iEnv;
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
    NetworkMonitorSender(Environment& aEnv);
    ~NetworkMonitorSender();
	TUint SenderPort() const;
    void Start(Endpoint aEndpoint, TUint aPeriodUs, TUint aBytes, TUint aIterations, TUint aTtl, TUint aId);
    void Stop();
private:
    void TimerExpired();
private:
    Environment& iEnv;
    SocketUdp iSocket;
    SocketTcpServer iServer;
    Timer* iTimer;
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

class ProviderNetworkMonitor;

class NetworkMonitor
{
public:
    NetworkMonitor(Environment& aEnv, Net::DvDevice& aDevice, const Brx& aName);
	void SetName(const Brx& aValue);
	~NetworkMonitor();
private:
    ProviderNetworkMonitor* iProvider;
    NetworkMonitorSender iSender;
    NetworkMonitorReceiver iReceiver;
};

} // namespace Av
} // namespace OpenHome

#endif // HEADER_NETWORK_MONITOR

