#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/OptionParser.h>
#include <OpenHome/Private/Debug.h>

#include "CpNetworkMonitorList2.h"

#include <vector>
#include <stdio.h>

#ifdef _WIN32

#pragma warning(disable:4355) // use of 'this' in ctor lists safe in this case

#define CDECL __cdecl

#include <conio.h>

int mygetch()
{
    return (_getch());
}

#elif defined(NOTERMIOS)

#define CDECL

int mygetch()
{
    return 0;
}

#else

#define CDECL

#include <termios.h>
#include <unistd.h>

int mygetch()
{
	struct termios oldt, newt;
	int ch;
	tcgetattr( STDIN_FILENO, &oldt );
	newt = oldt;
	newt.c_lflag &= ~( ICANON | ECHO );
	tcsetattr( STDIN_FILENO, TCSANOW, &newt );
	ch = getchar();
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
	return ch;
}

#endif

namespace OpenHome {
namespace Av {

	class NetworkMonitorList : public ICpNetworkMonitorList2Handler
	{
	public:
		NetworkMonitorList();
		void Report() const;
		CpNetworkMonitor* Find(const Brx& aName);
		virtual void NetworkMonitorAdded(CpNetworkMonitor& aNetworkMonitor);
		virtual void NetworkMonitorRemoved(CpNetworkMonitor& aNetworkMonitor);
		~NetworkMonitorList();
	private:
		mutable Mutex iMutex;
		std::vector<CpNetworkMonitor*> iList;
	};

} // namespace Av
} // namespace OpenHome

using namespace OpenHome;
using namespace OpenHome::Av;
using namespace OpenHome::Net;
using namespace OpenHome::TestFramework;

NetworkMonitorList::NetworkMonitorList()
	: iMutex("NMLI")
{
}

void NetworkMonitorList::NetworkMonitorAdded(CpNetworkMonitor& aNetworkMonitor)
{
	aNetworkMonitor.AddRef();
	iMutex.Wait();
	iList.push_back(&aNetworkMonitor);
	iMutex.Signal();
}

void NetworkMonitorList::NetworkMonitorRemoved(CpNetworkMonitor& /*aNetworkMonitor*/)
{
}

void NetworkMonitorList::Report() const
{
	iMutex.Wait();

	std::vector<CpNetworkMonitor*>::const_iterator it = iList.begin();

    while (it != iList.end()) {
		Brhz name((*it)->Name());
		printf("%s", name.CString());
		printf("\n");
        it++;
    }   

	iMutex.Signal();
}

CpNetworkMonitor* NetworkMonitorList::Find(const Brx& aName)
{
	iMutex.Wait();

	std::vector<CpNetworkMonitor*>::const_iterator it = iList.begin();

    while (it != iList.end()) {
		if ((*it)->Name() == aName) {
			iMutex.Signal();
			return (*it);
		}
        it++;
    }   

	iMutex.Signal();

	return (0);
}

NetworkMonitorList::~NetworkMonitorList()
{
    std::vector<CpNetworkMonitor*>::iterator it = iList.begin();

    while (it != iList.end()) {
        (*it)->RemoveRef();
        it++;
    }   
}

class ReceiverThread
{
private:
	ThreadFunctor* iThread;

	SocketTcpClient& iSocket;

	Srs<1000> iBuffer;

	TBool iAnalyse;

	TUint iId;

	TUint iTotal;
	TUint iMissed;

	TInt iMax;
	TInt iMin;
	TUint iTimings[100];

	TUint iLastFrame;
	TUint iLastTx;
	TUint iLastRx;

	TUint iTxTimebase;
	TUint iRxTimebase;

#define TOSL(x) ((signed long) x)
#define TOUL(x) ((unsigned long) x)
public:
	ReceiverThread(SocketTcpClient& aSocket, TBool aAnalyse, TUint aId)
		: iSocket(aSocket)
		, iBuffer(iSocket)
		, iAnalyse(aAnalyse)
		, iId(aId)
		, iTotal(0)
		, iMissed(0)
		, iMax(0)
		, iMin(0)
	{
		for (TUint i = 0; i < 100; i++) {
			iTimings[i] = 0;
		}

		iThread = new ThreadFunctor("RECV", MakeFunctor(*this, &ReceiverThread::Run));
		iThread->Start();
	}

	void ReportTimings()
	{
		for (TInt i = 0; i < 100; i++) {
			printf("%ld : %lu\n", TOSL(i - 10), TOUL(iTimings[i]));
		}
		printf("Total  : %lu\n", TOUL(iTotal));
		printf("Missed : %lu\n", TOUL(iMissed));
	}

	void Analyse(TUint aId, TUint aFrame, TUint aTx, TUint aRx)
	{
		if (aId != iId) {
			printf("Unrecognised Id (id: %lu, frame %lu, tx %lu, rx %lu)\n", TOUL(aId), TOUL(aFrame), TOUL(aTx), TOUL(aRx));
			return;
		}

		if (iTotal == 0) {
			iTxTimebase = aTx;
			iRxTimebase = aRx;
			iLastFrame = aFrame;
			iLastTx = 0;
			iLastRx = 0;
		}
		else {
			TUint txTimestamp = aTx - iTxTimebase;
			TUint rxTimestamp = aRx - iRxTimebase;

			if (aFrame < iLastFrame) {
				printf("Out of order frames with %lu followed by %lu\n", TOUL(iLastFrame), TOUL(aFrame));
			}
			else if (aFrame == iLastFrame) {
				printf("Repeated frame %lu\n", TOUL(aFrame));
			}
			else {
				TUint missed = aFrame - iLastFrame - 1;

				if (missed > 0) {
					printf("Missed %lu frames between %lu and %lu\n", TOUL(missed), TOUL(iLastFrame), TOUL(aFrame));
					iMissed += missed;
				}
				else {
					TInt networkTime = rxTimestamp - txTimestamp;

					TInt networkTimeMs = networkTime / 1000;

					TInt timingsIndex = networkTimeMs + 10;

					if (timingsIndex > 0 && timingsIndex < 100) {
						iTimings[timingsIndex]++;
					}

					if (networkTime > iMax) {
						iMax = networkTime;
						printf("Max %lduS on frame %lu\n", TOSL(iMax), TOUL(aFrame));
					}
					if (networkTime < iMin) {
						iMin = networkTime;
						printf("Min %lduS on frame %lu\n", TOSL(iMin), TOUL(aFrame));
					}
				}
			}

			iLastFrame = aFrame;
			iLastTx = txTimestamp;
			iLastRx = rxTimestamp;
		}

		iTotal++;
	}

	void Run()
	{
		try {
			for (;;) {
				Brn entry = iBuffer.Read(16);
				
				ReaderBuffer reader;
				
				reader.Set(entry);
				
				ReaderBinary binary(reader);

				TUint id = binary.ReadUintBe(4);
				TUint frame = binary.ReadUintBe(4);
				TUint tx = binary.ReadUintBe(4);
				TUint rx = binary.ReadUintBe(4);

				if (iAnalyse) {
					Analyse(id, frame, tx, rx);
				}
				else {
					printf("id: %lu, frame %lu, tx %lu, rx %lu\n", TOUL(id), TOUL(frame), TOUL(tx), TOUL(rx));
				}
			}
		}
		catch (ReaderError&)
		{
			printf("Receiver connection terminated\n");
		}
	}

	~ReceiverThread()
	{
		delete (iThread);
	}
};

int CDECL main(int aArgc, char* aArgv[])
{
    InitialisationParams* initParams = InitialisationParams::Create();

	Library* lib = new Library(initParams);

    OptionParser parser;
    
    OptionBool optionList("-l", "--list", "List Network Monitor Senders & Receivers");
    parser.AddOption(&optionList);
    OptionString optionSender("-s", "--sender", Brn(""), "Sender name");
    parser.AddOption(&optionSender);
    OptionString optionReceiver("-r", "--receiver", Brn(""), "Receiver name");
    parser.AddOption(&optionReceiver);
    OptionUint optionId("-i", "--id", 1, "Non-zero id for this set of messages");
    parser.AddOption(&optionId);
    OptionUint optionCount("-c", "--count", 0, "Number of messages to send (0 = infinite)");
    parser.AddOption(&optionCount);
    OptionUint optionBytes("-b", "--bytes", 12, "Number of bytes in each message (min = 12, max = 65536)");
    parser.AddOption(&optionBytes);
    OptionUint optionDelay("-d", "--delay", 10000, "Delay in uS between each message");
    parser.AddOption(&optionDelay);
    OptionUint optionTtl("-t", "--ttl", 1, "TTL used for messages");
    parser.AddOption(&optionTtl);
    OptionBool optionAnalyse("-a", "--analyse", "Analyse results");
    parser.AddOption(&optionAnalyse);

    if (!parser.Parse(aArgc, aArgv)) {
	    delete lib;
        return (1);
    }

    std::vector<NetworkAdapter*>* subnetList = lib->CreateSubnetList();
    TIpAddress subnet = (*subnetList)[0]->Subnet();
    Library::DestroySubnetList(subnetList);
    CpStack* cpStack = lib->StartCp(subnet);

	NetworkMonitorList* list = new NetworkMonitorList();
	CpNetworkMonitorList2* collector = new CpNetworkMonitorList2(*cpStack, *list);

	printf("Finding Network Monitors .");
	Thread::Sleep(1000);
	printf(".");
	Thread::Sleep(1000);
	printf(".\n");

	delete (collector);

	if (optionList.Value()) {
		list->Report();
		delete (list);
	    delete lib;
		return(0);
	}

    if (optionSender.Value().Bytes() == 0) {
    	printf("Sender not specified\n");
		delete (list);
	    delete lib;
    	return (1);
    }
    
    if (optionReceiver.Value().Bytes() == 0) {
    	printf("Receiver not specified\n");
		delete (list);
	    delete lib;
    	return (1);
    }
    
	CpNetworkMonitor* sender = list->Find(optionSender.Value());

	if (!sender) {
    	printf("Sender not found\n");
		delete (list);
	    delete lib;
    	return (1);
	}

	CpNetworkMonitor* receiver = list->Find(optionReceiver.Value());

	if (!receiver) {
    	printf("Receiver not found\n");
		delete (list);
	    delete lib;
    	return (1);
	}

	TUint id = optionId.Value();

	if (id == 0) {
    	printf("Invalid id\n");
		delete (list);
	    delete lib;
    	return (1);
	}

    TUint count = optionCount.Value();
	TUint bytes = optionBytes.Value();
	TUint ttl = optionTtl.Value();
    TUint delay = optionDelay.Value();

	if (delay == 0) {
    	printf("Invalid delay\n");
		delete (list);

    	return (1);
	}

    TBool analyse = optionAnalyse.Value();

	Brhz senderName(sender->Name());
	Brhz receiverName(receiver->Name());
	
	printf("From  : %s\n", senderName.CString());
	printf("To    : %s\n", receiverName.CString());
	printf("Count : %lu\n", TOUL(count));
	printf("Bytes : %lu\n", TOUL(bytes));
	printf("Delay : %lu\n", TOUL(delay));
	printf("TTL   : %lu\n\n", TOUL(ttl));

	SocketTcpClient receiverClient;
	SocketTcpClient senderClient;

	Endpoint receiverEndpoint(receiver->Results(), receiver->Address());

	printf("Contacting receiver\n");

	try	{
		receiverClient.Open(lib->Env());
		receiverClient.Connect(receiverEndpoint, 1000);
	}
	catch (NetworkError&) {
    	printf("Unable to contact receiver\n");
		delete (list);

    	return (1);
	}

	printf("Contacting sender\n");

	Endpoint senderEndpoint(sender->Sender(), sender->Address());

	try	{
		senderClient.Open(lib->Env());
		senderClient.Connect(senderEndpoint, 1000);
	}
	catch (NetworkError&) {
    	printf("Unable to contact sender\n");
		delete (list);

    	return (1);
	}

	printf("Issuing request to sender\n");

	Bws<1000> request;

	request.Append("start ");
	Endpoint::AppendAddress(request, receiver->Address());
	request.Append(":");
	Ascii::AppendDec(request, receiver->Receiver());
	request.Append(" ");
	Ascii::AppendDec(request, id);
	request.Append(" ");
	Ascii::AppendDec(request, count);
	request.Append(" ");
	Ascii::AppendDec(request, bytes);
	request.Append(" ");
	Ascii::AppendDec(request, delay);
	request.Append(" ");
	Ascii::AppendDec(request, ttl);
	request.Append("\n");
	
	Brhz creq(request);

	senderClient.Write(request);

	Srs<1000> responseBuffer(senderClient);

	Brn response = responseBuffer.ReadUntil('\n');

	if (response != Brn("OK")) {
		senderClient.Close();
		Brhz cresp(response);
		printf("%s\n", cresp.CString());
		delete (list);
	    delete lib;
		return (1);
	}

	printf("Starting receiver thread\n");

	ReceiverThread* thread = new ReceiverThread(receiverClient, analyse, id);

	for (;;) {
		TUint key = mygetch();

		if (key == 't') {
			thread->ReportTimings();
		}
		else if (key == 'q') {
			break;
		}
	}

	printf("Stopping sender\n");
	senderClient.Write(Brn("stop\n"));

	printf("Closing sender\n");
	senderClient.Close();

	printf("Deleting receiver thread\n");
	delete (thread);

	printf("Closing receiver\n");
	receiverClient.Close();

	printf("Deleting network monitor list\n");
	delete (list);

	printf("Closing library\n");
	delete lib;

	return (0);
}
