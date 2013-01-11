#include <OpenHome/OhNetTypes.h>
#include <OpenHome/Net/Core/DvDevice.h>
#include <OpenHome/Net/Core/OhNet.h>
#include <OpenHome/Net/Core/CpDevice.h>
#include <OpenHome/Net/Core/CpDeviceUpnp.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Private/Maths.h>
#include <OpenHome/Private/Thread.h>
#include <OpenHome/Private/OptionParser.h>
#include <OpenHome/Private/Debug.h>

#include <vector>
#include <stdio.h>

#include "NetworkMonitor.h"

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

int CDECL main(int aArgc, char* aArgv[])
{
    OptionParser parser;
    
    OptionString optionName("-n", "--name", Brx::Empty(), "[name] name of the NetworkMonitor");
    parser.AddOption(&optionName);
    
    if (!parser.Parse(aArgc, aArgv)) {
        return (1);
    }

	if (optionName.Value().Bytes() == 0) {
		printf("Name not specified\n");
		return (1);
	}

    InitialisationParams* initParams = InitialisationParams::Create();

	Library* lib = new Library(initParams);
    DvStack* dvStack = lib->StartDv();

	Bws<100> udn("OpenHome-NetworkMonitor-");
    udn.Append(optionName.Value());

    DvDeviceStandard* device = new DvDeviceStandard(*dvStack, udn);
    
    device->SetAttribute("Upnp.Domain", "av.openhome.org");
    device->SetAttribute("Upnp.Type", "Sender");
    device->SetAttribute("Upnp.Version", "1");
    device->SetAttribute("Upnp.FriendlyName", (const TChar*)udn.PtrZ());
    device->SetAttribute("Upnp.Manufacturer", "Openhome");
    device->SetAttribute("Upnp.ManufacturerUrl", "http://www.openhome.org");
    device->SetAttribute("Upnp.ModelDescription", "Openhome Network Monitor");
    device->SetAttribute("Upnp.ModelName", "Openhome Network Monitor");
    device->SetAttribute("Upnp.ModelNumber", "1");
    device->SetAttribute("Upnp.ModelUrl", "http://www.openhome.org");
    device->SetAttribute("Upnp.SerialNumber", "");
    device->SetAttribute("Upnp.Upc", "");

	NetworkMonitor* nm = new NetworkMonitor(lib->Env(), *device, optionName.Value());
	
    device->SetEnabled();

    for (;;) {
    	int key = mygetch();
    	
    	if (key == 'q') {
    		break;
    	}
    }
       
    delete (nm);

    delete (device);
    delete lib;
    
	printf("\n");
	
    return (0);
}
