#include <OpenHome/NetworkMonitor.h>
#include <OpenHome/Types.h>
#include <OpenHome/Defines.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Debug.h>
#include <OpenHome/Private/Env.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Private/Stream.h>
#include <OpenHome/Private/Timer.h>
#include <OpenHome/OsWrapper.h>
#include <Generated/DvAvOpenhomeOrgNetworkMonitor1.h>

#include <stdio.h>

namespace OpenHome {
class Environment;
namespace Av {

class ProviderNetworkMonitor : public Net::DvProviderAvOpenhomeOrgNetworkMonitor1
{
    static const TUint kMaxNameBytes = 100;
public:
    ProviderNetworkMonitor(Net::DvDevice& aDevice, const Brx& aName, TUint aSenderPort, TUint aReceiverPort, TUint aResultsPort);
    void SetName(const Brx& aValue);
private: // from Net::DvProviderAvOpenhomeOrgNetworkMonitor1
    void Name(Net::IDvInvocation& aInvocation, Net::IDvInvocationResponseString& aValue);
    void Ports(Net::IDvInvocation& aInvocation, Net::IDvInvocationResponseUint& aSender, Net::IDvInvocationResponseUint& aReceiver, Net::IDvInvocationResponseUint& aResults);
};

class NetworkMonitorSenderSession : public SocketTcpSession
{
public:
    NetworkMonitorSenderSession(NetworkMonitorSender& aSender);
private: // from SocketTcpSession
    void Run() override;
private:
    NetworkMonitorSender& iSender;
};

class NetworkMonitorReceiverSession : public SocketTcpSession
{
public:
    NetworkMonitorReceiverSession(Fifo<NetworkMonitorEvent>& aFifo);
private: // from SocketTcpSession
    void Run() override;
private:
    Fifo<NetworkMonitorEvent>& iFifo;
};

} // namespace Av
} // namespace OpenHome


using namespace OpenHome;
using namespace OpenHome::Av;

// NetworkMonitor

NetworkMonitor::NetworkMonitor(Environment& aEnv, Net::DvDevice& aDevice, const Brx& aName)
    : iSender(aEnv)
    , iReceiver(aEnv)
{
    iProvider = new ProviderNetworkMonitor(aDevice, aName, iSender.SenderPort(), iReceiver.ReceiverPort(), iReceiver.ResultsPort());
}

void NetworkMonitor::SetName(const Brx& aValue)
{
    iProvider->SetName(aValue);
}

NetworkMonitor::~NetworkMonitor()
{
    delete iProvider;
}


// NetworkMonitorEvent

NetworkMonitorEvent::NetworkMonitorEvent()
    : iId(0)
    , iFrame(0)
    , iTxTimestamp(0)
    , iRxTimestamp(0)
{
}

void NetworkMonitorEvent::Set(Environment& aEnv, Brx& aBuf)
{
    ASSERT(aBuf.Bytes() >= 3 * sizeof(TUint));

    ReaderBuffer reader(aBuf);
    ReaderBinary rb(reader);
    iId = rb.ReadUintBe(sizeof(iId));
    iFrame = rb.ReadUintBe(sizeof(iFrame));
    iTxTimestamp = rb.ReadUintBe(sizeof(iTxTimestamp));
    iRxTimestamp = (TUint)Os::TimeInUs(aEnv.OsCtx());
}

TBool NetworkMonitorEvent::IsEmpty() const
{
    return (iId == 0);
}

void NetworkMonitorEvent::AsBuffer(Bwx& aBuf) const
{
    ASSERT(aBuf.MaxBytes() - aBuf.Bytes() >= 4 * sizeof(TUint));

    WriterBuffer writer(aBuf);
    WriterBinary wb(writer);
    wb.WriteUint32Be(iId);
    wb.WriteUint32Be(iFrame);
    wb.WriteUint32Be(iTxTimestamp);
    wb.WriteUint32Be(iRxTimestamp);
}


// NetworkMonitorSenderSession

NetworkMonitorSenderSession::NetworkMonitorSenderSession(NetworkMonitorSender& aSender)
    : iSender(aSender)
{
}

void NetworkMonitorSenderSession::Run()
{
    const TUint kMaxLineBytes = 128;
    Srs<kMaxLineBytes> buffer(*this);
    for (;;) {
        try {
            Brn line = buffer.ReadUntil('\n');
            Parser parser(Ascii::Trim(line));
            const Brn command = parser.Next();
            if (Ascii::CaseInsensitiveEquals(command, Brn("start"))) {
                try {
                    const Brn ip  = parser.Next(':');
                    TUint port    = Ascii::Uint(parser.Next());
                    TUint id      = Ascii::Uint(parser.Next());
                    TUint count   = Ascii::Uint(parser.Next());
                    TUint bytes   = Ascii::Uint(parser.Next());
                    TUint period  = Ascii::Uint(parser.Next());
                    TUint ttl     = Ascii::Uint(parser.Next());

                    if (id == 0) {
                        Write(Brn("ERROR Invalid ID (cannot be zero)\n"));
                        continue;
                    }
                    if (bytes > 9000) {
                        Write(Brn("ERROR Unable to send messages longer than 9000 bytes\n"));
                        continue;
                    }
                    if (period < 10000) {
                        Write(Brn("ERROR Unable to send messages quicker than every 10mS\n"));
                        continue;
                    }

                    Endpoint endpoint(port, ip);
                    if (endpoint.Address() == 0) {
                        Write(Brn("ERROR Invalid 0.0.0.0 ip address\n"));
                        continue;
                    }

                    Write(Brn("OK\n"));
                    iSender.Start(endpoint, period, bytes, count, ttl, id);
                }
                catch (AsciiError&) {
                    Write(Brn("ERROR Could not parse command\n"));
                }
                catch (NetworkError&) {
                    Write(Brn("ERROR Could not parse endpoint\n"));
                }
            }
            else if (Ascii::CaseInsensitiveEquals(command, Brn("stop"))) {
                iSender.Stop();
                Write(Brn("OK\n"));
            }
            else {
                Write(Brn("ERROR Unrecognised command\n"));
            }
        }
        catch (ReaderError&) {
            break;
        }
    }
}


// NetworkMonitorSender

NetworkMonitorSender::NetworkMonitorSender(Environment& aEnv)
    : iEnv(aEnv)
    , iSocket(aEnv)
    , iServer(aEnv, "NetmonTxServer", 0, 0)
{
    iTimer = new Timer(aEnv, MakeFunctor(*this, &NetworkMonitorSender::TimerExpired), "NetworkMonitorSender");
    iBuffer.FillZ();
    iServer.Add("NetmonTxSession", new NetworkMonitorSenderSession(*this));
}

NetworkMonitorSender::~NetworkMonitorSender()
{
    delete iTimer;
}

void NetworkMonitorSender::Start(Endpoint aEndpoint, TUint aPeriodUs, TUint aBytes, TUint aIterations, TUint aTtl, TUint aId)
{
    iTimer->Cancel();
    iEndpoint.Replace(aEndpoint);
    iPeriodMs = aPeriodUs / 1000;
    iBytes = aBytes;
    iIterations = aIterations;
    iTtl = aTtl;
    iId = aId;
    iFrame = 0;
    iNext = Time::Now(iEnv) + iPeriodMs;
    iTimer->FireAt(iNext);
}

void NetworkMonitorSender::Stop()
{
    iBuffer.FillZ();
    iBuffer.SetBytes(12);
    iSocket.Send(iBuffer, iEndpoint);
    iTimer->Cancel();
}

void NetworkMonitorSender::TimerExpired()
{
    iBuffer.SetBytes(0);
    WriterBuffer writer(iBuffer);
    WriterBinary binary(writer);
    binary.WriteUint32Be(iId);
    binary.WriteUint32Be(iFrame++);
    binary.WriteUint32Be(Time::Now(iEnv) * 1000);
    binary.WriteUint32Be(iId);
    iBuffer.SetBytes(iBytes);
    iSocket.Send(iBuffer, iEndpoint);

    if (iIterations == 0 || --iIterations > 0) {
        iNext += iPeriodMs;
        iTimer->FireAt(iNext);
    }
}

TUint NetworkMonitorSender::SenderPort() const
{
    return iServer.Port();
}


// NetworkMonitorReceiverSession

NetworkMonitorReceiverSession::NetworkMonitorReceiverSession(Fifo<NetworkMonitorEvent>& aFifo)
    : iFifo(aFifo)
{
}

void NetworkMonitorReceiverSession::Run()
{
    Bws<16> buf;
    for (;;) {
        NetworkMonitorEvent rxevent = iFifo.Read();
        if (rxevent.IsEmpty()) {
            break;
        }
        rxevent.AsBuffer(buf);
        Send(buf);
        buf.SetBytes(0);
    }
}


// NetworkMonitorReceiver

NetworkMonitorReceiver::NetworkMonitorReceiver(Environment& aEnv)
    : iEnv(aEnv)
    , iSocket(aEnv, 0)
    , iServer(aEnv, "NetmonRxServer", 0, 0)
    , iFifo(kEventQueueLength)
{
    iServer.Add("NetmonRxSession", new NetworkMonitorReceiverSession(iFifo));
    iThread = new ThreadFunctor("NetmonRxThread", MakeFunctor(*this, &NetworkMonitorReceiver::Run), kPriorityHigh);
    iThread->Start();
}

NetworkMonitorReceiver::~NetworkMonitorReceiver()
{
    iSocket.Interrupt(true);
    delete iThread;
}

TUint NetworkMonitorReceiver::ReceiverPort() const
{
    return iSocket.Port();
}

TUint NetworkMonitorReceiver::ResultsPort() const
{
    return iServer.Port();
}

void NetworkMonitorReceiver::Run()
{
    TBool overflow = false;
    for (;;) {
        try {
            iSocket.Receive(iBuffer);
        }
        catch (NetworkError&) {
            LOG(kError, "NetworkMonitorReceiver::Run exiting due to NetworkError exception\n");
            break;
        }

        NetworkMonitorEvent rxevent;
        try {
            rxevent.Set(iEnv, iBuffer);
        }
        catch (NetworkMonitorEventInvalid&) {
            LOG(kError, "ERROR: NetworkMonitorReceiver - read %u byte packet\n", iBuffer.Bytes());
            break;
        }
        if (overflow) {
            if (iFifo.SlotsFree() >= 10) {  // resume when there is a little space
                iFifo.Write(NetworkMonitorEvent()); // overflow entry
                iFifo.Write(rxevent);
                overflow = false;
            }
            // else discard the packet.
        }
        else {
            if (iFifo.SlotsFree() > 0) {
                iFifo.Write(rxevent);
            }
            else {
                overflow = true;
            }
        }
    }
}


// ProviderNetworkMonitor

ProviderNetworkMonitor::ProviderNetworkMonitor(Net::DvDevice& aDevice, const Brx& aName, TUint aSenderPort, TUint aReceiverPort, TUint aResultsPort)
    : DvProviderAvOpenhomeOrgNetworkMonitor1(aDevice)
{
    EnablePropertyName();
    EnablePropertySender();
    EnablePropertyReceiver();
    EnablePropertyResults();
    EnableActionName();
    EnableActionPorts();

    SetPropertyName(aName);
    SetPropertySender(aSenderPort);
    SetPropertyReceiver(aReceiverPort);
    SetPropertyResults(aResultsPort);
}

void ProviderNetworkMonitor::SetName(const Brx& aValue)
{
    SetPropertyName(aValue);
}

void ProviderNetworkMonitor::Name(Net::IDvInvocation& aInvocation, Net::IDvInvocationResponseString& aValue)
{
    Brhz value;
    GetPropertyName(value);
    aInvocation.StartResponse();
    aValue.Write(value);
    aValue.WriteFlush();
    aInvocation.EndResponse();
}

void ProviderNetworkMonitor::Ports(Net::IDvInvocation& aInvocation, Net::IDvInvocationResponseUint& aSender, Net::IDvInvocationResponseUint& aReceiver, Net::IDvInvocationResponseUint& aResults)
{
    TUint sender;
    TUint receiver;
    TUint results;
    GetPropertySender(sender);
    GetPropertyReceiver(receiver);
    GetPropertyResults(results);
    aInvocation.StartResponse();
    aSender.Write(sender);
    aReceiver.Write(receiver);
    aResults.Write(results);
    aInvocation.EndResponse();
}

