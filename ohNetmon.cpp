#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/Private/Network.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Private/Maths.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/OptionParser.h>
#include <OpenHome/Private/Debug.h>

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

using namespace OpenHome;
using namespace OpenHome::Net;
using namespace OpenHome::TestFramework;

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
			printf("%d : %d\n", i - 10, iTimings[i]);
		}
		printf("Total  : %d\n", iTotal);
		printf("Missed : %d\n", iMissed);
	}

	void Analyse(TUint aId, TUint aFrame, TUint aTx, TUint aRx)
	{
		if (aId != iId) {
			printf("Unrecognised Id (id: %d, frame %d, tx %d, rx %d)\n", aId, aFrame, aTx, aRx);
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
				printf("Out of order frames with %d folowed by %d\n", iLastFrame, aFrame);
			}
			else if (aFrame == iLastFrame) {
				printf("Repeated frame %d\n", aFrame);
			}
			else {
				TUint missed = aFrame - iLastFrame - 1;

				if (missed > 0) {
					printf("Missed %d frames betwwen %d and %d\n", missed, iLastFrame, aFrame);
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
						printf("Max %duS on frame %d\n", iMax, aFrame);
					}
					if (networkTime < iMin) {
						iMin = networkTime;
						printf("Min %duS on frame %d\n", iMin, aFrame);
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
					printf("id: %d, frame %d, tx %d, rx %d\n", id, frame, tx, rx);
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

	UpnpLibrary::Initialise(initParams);

    OptionParser parser;
    
    OptionString optionSender("-s", "--sender", Brn(""), "Sender endpoint (mandatory)");
    parser.AddOption(&optionSender);
    
    OptionString optionReceiver("-r", "--receiver", Brn(""), "Receiver endpoint (mandatory)");
    parser.AddOption(&optionReceiver);
    
    OptionUint optionPort("-p", "--port", 0, "Receiver datagram port (mandatory)");
    parser.AddOption(&optionPort);
    
    OptionUint optionId("-i", "--id", 1, "Non-zero id for this set of messages");
    parser.AddOption(&optionId);
    
    OptionUint optionCount("-c", "--count", 0, "Number of messages to send (0 = infinite)");
    parser.AddOption(&optionCount);
    
    OptionUint optionBytes("-b", "--bytes", 12, "Number of bytes in each message (min = 12, max = 65536)");
    parser.AddOption(&optionBytes);
    
    OptionUint optionDelay("-d", "--delay", 10000, "Delay in uS between each message");
    parser.AddOption(&optionDelay);

    OptionUint optionTtl("-t", "--ttl", 1, "Ttl used for messages");
    parser.AddOption(&optionTtl);

    OptionBool optionAnalyse("-a", "--analyse", "Analyse results");
    parser.AddOption(&optionAnalyse);

    if (!parser.Parse(aArgc, aArgv)) {
	    Net::UpnpLibrary::Close();
        return (1);
    }

	Brhz sender(optionSender.Value());

    if (sender.Bytes() == 0) {
    	printf("No sender endpoint specified\n");
	    Net::UpnpLibrary::Close();
    	return (1);
    }
    
	Parser senderParser(sender);

	Brn senderAddress = senderParser.Next(':');

	TUint senderPort;

	try {
		senderPort = Ascii::Uint(senderParser.NextToEnd());
	}
	catch (AsciiError&)	{
    	printf("Invalid sender endpoint\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

	Endpoint senderEndpoint;

	try
	{
		senderEndpoint.SetAddress(senderAddress);
		senderEndpoint.SetPort(senderPort);
	}
	catch (NetworkError&) {
    	printf("Invalid sender endpoint\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

	Brhz receiver(optionReceiver.Value());
    
    if (receiver.Bytes() == 0) {
    	printf("No receiver endpoint specified\n");
	    Net::UpnpLibrary::Close();
    	return (1);
    }
    
	Parser receiverParser(receiver);

	Brn receiverAddress = receiverParser.Next(':');

	TUint receiverPort;

	try {
		receiverPort = Ascii::Uint(receiverParser.NextToEnd());
	}
	catch (AsciiError&)	{
    	printf("Invalid receiver endpoint\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

	Endpoint receiverEndpoint;

	try	{
		receiverEndpoint.SetAddress(receiverAddress);
		receiverEndpoint.SetPort(receiverPort);
	}
	catch (NetworkError&) {
    	printf("Invalid receiver endpoint\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}


	TUint port = optionPort.Value();

	if (port == 0) {
    	printf("Invalid receiver datagram port\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

	TUint id = optionId.Value();

	if (id == 0) {
    	printf("Invalid id\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

    TUint count = optionCount.Value();
    
	TUint bytes = optionBytes.Value();
    
	TUint ttl = optionTtl.Value();

    TUint delay = optionDelay.Value();

	if (delay == 0) {
    	printf("Invalid delay\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

    TBool analyse = optionAnalyse.Value();
	
	printf("From  : %s\n", sender.CString());
	printf("To    : %s\n", receiver.CString());
	printf("Count : %d\n", count);
	printf("Bytes : %d\n", bytes);
	printf("Delay : %d\n", delay);
	printf("Ttl   : %d\n\n", ttl);

	SocketTcpClient receiverClient;
	SocketTcpClient senderClient;

	printf("Contacting receiver\n");

	try	{
		receiverClient.Open();
		receiverClient.Connect(receiverEndpoint, 1000);
	}
	catch (NetworkError&) {
    	printf("Unable to contact receiver\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

	printf("Contacting sender\n");

	try	{
		senderClient.Open();
		senderClient.Connect(senderEndpoint, 1000);
	}
	catch (NetworkError&) {
    	printf("Unable to contact sender\n");
	    Net::UpnpLibrary::Close();
    	return (1);
	}

	printf("Issuing request to sender\n");

	Bws<1000> request;

	request.Append("start ");
	request.Append(receiverAddress);
	request.Append(":");
	Ascii::AppendDec(request, port);
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

	printf(creq.CString());

	senderClient.Write(request);

	Srs<1000> responseBuffer(senderClient);

	Brn response = responseBuffer.ReadUntil('\n');

	if (response != Brn("OK")) {
		senderClient.Close();
		Brhz cresp(response);
		printf("%s\n", cresp.CString());
	    Net::UpnpLibrary::Close();
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

	printf("Closing library\n");

	Net::UpnpLibrary::Close();

	return (0);
}
