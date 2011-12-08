all : ohNetmon ohNetworkMonitor

.PHONY : ohNetworkMonitor

ohNetworkMonitor : make_obj_dir $(objdir)ohNetworkMonitor.$(exeext)

$(objdir)ohNetworkMonitor.$(exeext) : ohNetworkMonitor.cpp NetworkMonitor.cpp
	$(compiler)ohNetworkMonitor.$(objext) -c $(cflags) $(includes) ohNetworkMonitor.cpp
	$(compiler)NetworkMonitor.$(objext) -c $(cflags) $(includes) NetworkMonitor.cpp
	$(link) $(linkoutput)$(objdir)ohNetworkMonitor.$(exeext) $(objdir)ohNetworkMonitor.$(objext) $(objdir)NetworkMonitor.$(objext) $(ohnetdir)DvAvOpenhomeOrgNetworkMonitor1.$(objext) $(ohnetdir)$(libprefix)ohNetCore.lib $(ohnetdir)$(libprefix)TestFramework.$(libext)

.PHONY : ohNetmon

ohNetmon : make_obj_dir $(objdir)ohNetmon.$(exeext)

$(objdir)ohNetmon.$(exeext) : ohNetmon.cpp CpNetworkMonitorList1.cpp CpNetworkMonitorList2.cpp
	$(compiler)ohNetmon.$(objext) -c $(cflags) $(includes) ohNetmon.cpp
	$(compiler)CpNetworkMonitorList1.$(objext) -c $(cflags) $(includes) CpNetworkMonitorList1.cpp
	$(compiler)CpNetworkMonitorList2.$(objext) -c $(cflags) $(includes) CpNetworkMonitorList2.cpp
	$(link) $(linkoutput)$(objdir)ohNetmon.$(exeext) $(objdir)ohNetmon.$(objext) $(objdir)CpNetworkMonitorList1.$(objext) $(objdir)CpNetworkMonitorList2.$(objext) $(ohnetdir)CpAvOpenhomeOrgNetworkMonitor1.$(objext) $(ohnetdir)$(libprefix)ohNetCore.$(libext) $(ohnetdir)$(libprefix)TestFramework.$(libext)

