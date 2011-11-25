#include "CpNetworkMonitorList2.h"

#include <OpenHome/Private/Debug.h>
#include <OpenHome/Private/Parser.h>
#include <OpenHome/Private/Ascii.h>
#include <OpenHome/Net/Private/XmlParser.h>

using namespace OpenHome;
using namespace OpenHome::Net;

// CpNetworkMonitor

CpNetworkMonitor::CpNetworkMonitor(const Brx& aName, TUint aSender, TUint aReceiver, TUint aResults)
	: iName(aName)
	, iSender(aSender)
	, iReceiver(aReceiver)
	, iResults(aResults)
{
}

const Brx& CpNetworkMonitor::Name() const
{
	return (iName);
}

TUint CpNetworkMonitor:Sender() const
{
	return (iSender);
}

TUint CpNetworkMonitor:Receiver() const
{
	return (iReceiver);
}

TUint CpNetworkMonitor:Results() const
{
	return (iResults);
}

void CpNetworkMonitor::Update(const Brx& aName)
{
	iName.Replace(aName);
}

// CpNetworkMonitorList2Group

CpNetworkMonitorList2Group::CpNetworkMonitorList2Group(CpDevice& aDevice, ICpNetworkMonitorList2GroupHandler& aHandler, TBool aStandby, const Brx& aRoom, const Brx& aName, TUint aSourceIndex, TBool aHasVolumeControl)
    : iDevice(aDevice)
    , iHandler(aHandler)
    , iStandby(aStandby)
    , iRoom(aRoom)
    , iName(aName)
    , iSourceIndex(aSourceIndex)
    , iHasVolumeControl(aHasVolumeControl)
    , iUserData(0)
    , iRefCount(1)
{
	if (&iDevice != 0) {  // device with zero address used by test code
		iDevice.AddRef();
	}
}

CpNetworkMonitorList2Group::~CpNetworkMonitorList2Group()
{
	if (&iDevice != 0) { // device with zero address used by test code
		iDevice.RemoveRef();
	}

	for (size_t i=0; i<iSourceList.size(); i++) {
        delete iSourceList[i];
    }
    iSourceList.clear();
}

void CpNetworkMonitorList2Group::AddSource(const Brx& aName, const Brx& aType, TBool aVisible)
{
	iSourceList.push_back(new CpNetworkMonitorList2Source(aName, aType, aVisible));
}

void CpNetworkMonitorList2Group::UpdateRoom(const Brx& aValue)
{
	iRoom.Replace(aValue);
}

void CpNetworkMonitorList2Group::UpdateName(const Brx& aValue)
{
	iName.Replace(aValue);
}

void CpNetworkMonitorList2Group::UpdateSourceIndex(TUint aValue)
{
	iSourceIndex = aValue;
}

void CpNetworkMonitorList2Group::UpdateStandby(TBool aValue)
{
	iStandby = aValue;
}

void CpNetworkMonitorList2Group::UpdateSource(TUint aIndex, const Brx& aName, const Brx& aType, TBool aVisible)
{
	iSourceList[aIndex]->Update(aName, aType, aVisible);
}

void CpNetworkMonitorList2Group::AddRef()
{
    iRefCount++;
}

void CpNetworkMonitorList2Group::RemoveRef()
{
	if (--iRefCount == 0) {
        delete this;
    }
}

CpDevice& CpNetworkMonitorList2Group::Device() const
{
    return (iDevice);
}

TUint CpNetworkMonitorList2Group::SourceCount() const
{
	return ((TUint)iSourceList.size());
}

TUint CpNetworkMonitorList2Group::SourceIndex() const
{
	return (iSourceIndex);
}

TBool CpNetworkMonitorList2Group::Standby() const
{
	return (iStandby);
}

void CpNetworkMonitorList2Group::SetSourceIndex(TUint aIndex)
{
	ASSERT(aIndex < iSourceList.size());
	iHandler.SetSourceIndex(aIndex);
}

void CpNetworkMonitorList2Group::SetStandby(TBool aValue)
{
	iHandler.SetStandby(aValue);
}

const Brx& CpNetworkMonitorList2Group::Room() const
{
	return (iRoom);
}

const Brx& CpNetworkMonitorList2Group::Name() const
{
	return (iName);
}

const Brx& CpNetworkMonitorList2Group::SourceName(TUint aIndex) const
{
	return (iSourceList[aIndex]->Name());
}

const Brx& CpNetworkMonitorList2Group::SourceType(TUint aIndex) const
{
	return (iSourceList[aIndex]->Type());
}

TBool CpNetworkMonitorList2Group::SourceVisible(TUint aIndex) const
{
	return (iSourceList[aIndex]->Visible());
}

TBool CpNetworkMonitorList2Group::HasVolumeControl() const
{
	return (iHasVolumeControl);
}

void CpNetworkMonitorList2Group::SetUserData(void* aValue)
{
	iUserData = aValue;
}

void* CpNetworkMonitorList2Group::UserData() const
{
	return (iUserData);
}

// CpNetworkMonitorList2Device

CpNetworkMonitorList2Device::CpNetworkMonitorList2Device(CpDevice& aDevice)
    : iDevice(aDevice)
{
    iDevice.AddRef();
}
	
TBool CpNetworkMonitorList2Device::IsAttachedTo(CpDevice& aDevice)
{
	return (iDevice.Udn() == aDevice.Udn());
}

CpNetworkMonitorList2Device::~CpNetworkMonitorList2Device()
{
    iDevice.RemoveRef();
}
	
// CpNetworkMonitorList2Product

CpNetworkMonitorList2Product::CpNetworkMonitorList2Product(CpDevice& aDevice, ICpNetworkMonitorList2Handler& aHandler)
    : CpNetworkMonitorList2Device(aDevice)
    , iHandler(aHandler)
    , iServiceProduct(0)
    , iGroup(0)
{
    iFunctorSetSourceIndex = MakeFunctorAsync(*this, &CpNetworkMonitorList2Product::CallbackSetSourceIndex);
    iFunctorSetStandby = MakeFunctorAsync(*this, &CpNetworkMonitorList2Product::CallbackSetStandby);

	iServiceProduct = new CpProxyAvOpenhomeOrgProduct1(iDevice);

	Functor functorInitial = MakeFunctor(*this, &CpNetworkMonitorList2Product::EventProductInitialEvent);

    iServiceProduct->SetPropertyInitialEvent(functorInitial);

	iServiceProduct->Subscribe();
}

CpNetworkMonitorList2Product::~CpNetworkMonitorList2Product()
{
    LOG(kTopology, "CpNetworkMonitorList2Product::~CpNetworkMonitorList2Product\n");

	delete (iServiceProduct);
	
	if (iGroup != 0) {
		iHandler.GroupRemoved(*iGroup);
		iGroup->RemoveRef();
	}
}

void CpNetworkMonitorList2Product::EventProductInitialEvent()
{
    Functor functorRoom = MakeFunctor(*this, &CpNetworkMonitorList2Product::EventProductRoomChanged);
    Functor functorName = MakeFunctor(*this, &CpNetworkMonitorList2Product::EventProductNameChanged);
    Functor functorStandby = MakeFunctor(*this, &CpNetworkMonitorList2Product::EventProductStandbyChanged);
    Functor functorSourceIndex = MakeFunctor(*this, &CpNetworkMonitorList2Product::EventProductSourceIndexChanged);
    Functor functorSourceXml = MakeFunctor(*this, &CpNetworkMonitorList2Product::EventProductSourceXmlChanged);

    iServiceProduct->SetPropertyProductRoomChanged(functorRoom);    
    iServiceProduct->SetPropertyProductNameChanged(functorName);    
    iServiceProduct->SetPropertyStandbyChanged(functorStandby); 
    iServiceProduct->SetPropertySourceIndexChanged(functorSourceIndex); 
    iServiceProduct->SetPropertySourceXmlChanged(functorSourceXml);
      
	TBool hasVolumeControl = false;

	Brhz attributes;

	iServiceProduct->PropertyAttributes(attributes);

    Parser parser(attributes);
    
    for (;;) {
        Brn attribute = parser.Next();
        
        if (attribute.Bytes() == 0) {
            break;
        }
        
        if (attribute == Brn("Volume")) {
			hasVolumeControl = true;
            break;
        }
    }
    
	Brhz room;
	Brhz name;
	TUint sourceIndex;
	TBool standby;
	Brhz xml;
	
	iServiceProduct->PropertyProductRoom(room);
	iServiceProduct->PropertyProductName(name);
	iServiceProduct->PropertySourceIndex(sourceIndex);
	iServiceProduct->PropertyStandby(standby);
	iServiceProduct->PropertySourceXml(xml);
	
	iGroup = new CpNetworkMonitorList2Group(iDevice, *this, standby, room, name, sourceIndex, hasVolumeControl);
	
	ProcessSourceXml(xml, true);
	
	iHandler.GroupAdded(*iGroup);
}

/*  Example source xml

	<SourceList>
		<Source>
			<Name>Playlist</Name>
			<Type>Playlist</Type>
			<Visible>1</Visible>
		</Source>
		<Source>
			<Name>Radio</Name>
			<Type>Radio</Type>
			<Visible>1</Visible>
		</Source>
	</SourceList>
*/

void CpNetworkMonitorList2Product::ProcessSourceXml(const Brx& aXml, TBool aInitial)
{
	TUint count = 0;

	TBool updated = false;
	
	try {
	    Brn sourceList = XmlParserBasic::Find("SourceList", aXml);

		for (;;) {
			Brn source = XmlParserBasic::Find("Source", sourceList, sourceList);
			Brn name = XmlParserBasic::Find("Name", source);
			Brn type = XmlParserBasic::Find("Type", source);
			Brn visible = XmlParserBasic::Find("Visible", source);
			
			TBool vis = false;
			
			if (visible == Brn("true")) {
				vis = true;
			}
			
			if (aInitial) {
				iGroup->AddSource(name, type, vis);
			}
			else {
				iGroup->UpdateSource(count, name, type, vis);
				updated = true;
			}

			count++;
		}
	}
	catch (XmlError) {
	}
	
	if (updated) {
		iHandler.GroupSourceListChanged(*iGroup);
	}
}

void CpNetworkMonitorList2Product::EventProductRoomChanged()
{
    LOG(kTopology, "CpNetworkMonitorList2::EventProductRoomChanged ");
    LOG(kTopology, iGroup->Room());
    LOG(kTopology, ":");
    LOG(kTopology, iGroup->Name());
    LOG(kTopology, "\n");

	Brhz room;
	iServiceProduct->PropertyProductRoom(room);
	
	iHandler.GroupRemoved(*iGroup);
	iGroup->UpdateRoom(room);
	iHandler.GroupAdded(*iGroup);
}
	
void CpNetworkMonitorList2Product::EventProductNameChanged()
{
    LOG(kTopology, "CpNetworkMonitorList2::EventProductNameChanged ");
    LOG(kTopology, iGroup->Room());
    LOG(kTopology, ":");
    LOG(kTopology, iGroup->Name());
    LOG(kTopology, "\n");

	Brhz name;
	iServiceProduct->PropertyProductName(name);
	
	iHandler.GroupRemoved(*iGroup);
	iGroup->UpdateName(name);
	iHandler.GroupAdded(*iGroup);
}

void CpNetworkMonitorList2Product::EventProductStandbyChanged()
{
    LOG(kTopology, "CpNetworkMonitorList2::EventProductStandbyChanged ");
    LOG(kTopology, iGroup->Room());
    LOG(kTopology, ":");
    LOG(kTopology, iGroup->Name());
    LOG(kTopology, "\n");

	TBool standby;
	iServiceProduct->PropertyStandby(standby);
	
	iGroup->UpdateStandby(standby);
	iHandler.GroupStandbyChanged(*iGroup);
}
	
void CpNetworkMonitorList2Product::EventProductSourceIndexChanged()
{
    LOG(kTopology, "CpNetworkMonitorList2::EventProductSourceIndexChanged ");
    LOG(kTopology, iGroup->Room());
    LOG(kTopology, ":");
    LOG(kTopology, iGroup->Name());
    LOG(kTopology, "\n");

	TUint sourceIndex;
	iServiceProduct->PropertySourceIndex(sourceIndex);
	
	iGroup->UpdateSourceIndex(sourceIndex);
	iHandler.GroupSourceIndexChanged(*iGroup);
}
	
void CpNetworkMonitorList2Product::EventProductSourceXmlChanged()
{
    LOG(kTopology, "CpNetworkMonitorList2::EventProductSourceXmlChanged ");
    LOG(kTopology, iGroup->Room());
    LOG(kTopology, ":");
    LOG(kTopology, iGroup->Name());
    LOG(kTopology, "\n");

	Brhz xml;
	
	iServiceProduct->PropertySourceXml(xml);

	ProcessSourceXml(xml, false);	
}	

void CpNetworkMonitorList2Product::SetSourceIndex(TUint aIndex)
{
	iServiceProduct->BeginSetSourceIndex(aIndex, iFunctorSetSourceIndex);
}

void CpNetworkMonitorList2Product::CallbackSetSourceIndex(IAsync& aAsync)
{
	iServiceProduct->EndSetSourceIndex(aAsync);
}
	
void CpNetworkMonitorList2Product::SetStandby(TBool aValue)
{
	iServiceProduct->BeginSetStandby(aValue, iFunctorSetStandby);
}

void CpNetworkMonitorList2Product::CallbackSetStandby(IAsync& aAsync)
{
	iServiceProduct->EndSetStandby(aAsync);
}

// CpNetworkMonitorList2Job

CpNetworkMonitorList2Job::CpNetworkMonitorList2Job(ICpNetworkMonitorList2Handler& aHandler)
{
	iHandler = &aHandler;
	iGroup = 0;
}
	
void CpNetworkMonitorList2Job::Set(CpNetworkMonitorList2Group& aGroup, ICpNetworkMonitorList2HandlerFunction aFunction)
{
	iGroup = &aGroup;
	iFunction = aFunction;
	iGroup->AddRef();
}

void CpNetworkMonitorList2Job::Execute()
{
	if (iGroup) {
		(iHandler->*iFunction)(*iGroup);
		iGroup->RemoveRef();
		iGroup = 0;
	}
	else {
		THROW(ThreadKill);
	}
}

// CpNetworkMonitorList2

CpNetworkMonitorList2::CpNetworkMonitorList2(ICpNetworkMonitorList2Handler& aHandler)
	: iFree(kMaxJobCount)
	, iReady(kMaxJobCount)
{
	for (TUint i = 0; i < kMaxJobCount; i++) {
		iFree.Write(new CpNetworkMonitorList2Job(aHandler));
	}
	
	iNetworkMonitorList1 = new CpNetworkMonitorList1(*this);
	
	iThread = new ThreadFunctor("NML2", MakeFunctor(*this, &CpNetworkMonitorList2::Run));

	iThread->Start();
}

CpNetworkMonitorList2::~CpNetworkMonitorList2()
{
    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2\n");

	delete (iNetworkMonitorList1);
    
    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted layer 1\n");

    std::vector<CpNetworkMonitor*>::iterator it = iDeviceList.begin();

    while (it != iDeviceList.end()) {
        delete (*it);
        it++;
    }   

    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted devices\n");

	iReady.Write(iFree.Read()); // this null job causes the thread to complete

	delete (iThread);
	
    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted thread\n");

	for (TUint i = 0; i < kMaxJobCount; i++) {
		delete (iFree.Read());
	}

    LOG(kTopology, "CpNetworkMonitorList2::~CpNetworkMonitorList2 deleted jobs\n");
}
    
void CpNetworkMonitorList2::Refresh()
{
	iTopology1->Refresh();
}

// ICpNetworkMonitorList1Handler
    
void CpNetworkMonitorList2::NetworkMonitorAdded(CpDevice& aDevice)
{
    iDeviceList.push_back(new CpNetworkMonitorList2Device(aDevice, *this));
}

void CpNetworkMonitorList2::NetworkMonitorRemoved(CpDevice& aDevice)
{
    DeviceRemoved(aDevice);
}

void CpNetworkMonitorList2::DeviceRemoved(CpDevice& aDevice)
{
    std::vector<CpNetworkMonitorList2Device*>::iterator it = iDeviceList.begin();

    while (it != iDeviceList.end()) {
        if ((*it)->IsAttachedTo(aDevice)) {
            LOG(kTopology, "CpNetworkMonitorList2::NetworkMonitorRemoved found\n");
            delete (*it);
            iDeviceList.erase(it);
            break;
        }
        it++;
    }   
}

// ICpNetworkMonitorList1Handler

void CpNetworkMonitorList2::GroupAdded(CpNetworkMonitorList2Group& aGroup)
{
    LOG(kTopology, "CpNetworkMonitorList2::GroupAdded ");
    LOG(kTopology, aGroup.Room());
    LOG(kTopology, ":");
    LOG(kTopology, aGroup.Name());
    LOG(kTopology, "\n");

	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aGroup, &ICpNetworkMonitorList2Handler::GroupAdded);
	iReady.Write(job);
}

void CpNetworkMonitorList2::GroupStandbyChanged(CpNetworkMonitorList2Group& aGroup)
{
    LOG(kTopology, "CpNetworkMonitorList2::GroupStandbyChanged ");
    LOG(kTopology, aGroup.Room());
    LOG(kTopology, ":");
    LOG(kTopology, aGroup.Name());
    LOG(kTopology, "\n");

	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aGroup, &ICpNetworkMonitorList2Handler::GroupStandbyChanged);
	iReady.Write(job);
}

void CpNetworkMonitorList2::GroupSourceIndexChanged(CpNetworkMonitorList2Group& aGroup)
{
    LOG(kTopology, "CpNetworkMonitorList2::GroupSourceIndexChanged ");
    LOG(kTopology, aGroup.Room());
    LOG(kTopology, ":");
    LOG(kTopology, aGroup.Name());
    LOG(kTopology, "\n");

	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aGroup, &ICpNetworkMonitorList2Handler::GroupSourceIndexChanged);
	iReady.Write(job);
}

void CpNetworkMonitorList2::GroupSourceListChanged(CpNetworkMonitorList2Group& aGroup)
{
    LOG(kTopology, "CpNetworkMonitorList2::GroupSourceListChanged ");
    LOG(kTopology, aGroup.Room());
    LOG(kTopology, ":");
    LOG(kTopology, aGroup.Name());
    LOG(kTopology, "\n");

	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aGroup, &ICpNetworkMonitorList2Handler::GroupSourceListChanged);
	iReady.Write(job);
}

void CpNetworkMonitorList2::GroupRemoved(CpNetworkMonitorList2Group& aGroup)
{
    LOG(kTopology, "CpNetworkMonitorList2::GroupRemoved ");
    LOG(kTopology, aGroup.Room());
    LOG(kTopology, ":");
    LOG(kTopology, aGroup.Name());
    LOG(kTopology, "\n");

	CpNetworkMonitorList2Job* job = iFree.Read();
	job->Set(aGroup, &ICpNetworkMonitorList2Handler::GroupRemoved);
	iReady.Write(job);
}

void CpNetworkMonitorList2::Run()
{
    LOG(kTopology, "CpNetworkMonitorList2::Run Started\n");

    for (;;)
    {
	    LOG(kTopology, "CpNetworkMonitorList2::Run wait for job\n");

    	CpNetworkMonitorList2Job* job = iReady.Read();
    	
	    LOG(kTopology, "CpNetworkMonitorList2::Run execute job\n");

    	try {
	    	job->Execute();
	    	iFree.Write(job);
	    }
	    catch (ThreadKill)
	    {
	    	iFree.Write(job);
	    	break;
	    }
    }

    LOG(kTopology, "CpNetworkMonitorList2::Run Exiting\n");
}
