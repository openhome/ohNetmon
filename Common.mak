all : ohNetmon ohNetworkMonitor

.PHONY : ohNetworkMonitor

ohNetworkMonitor : $(objdir) $(objdir)/ohNetworkMonitor.$(exeext)

$(objdir)/ohNetworkMonitor.$(exeext) : ohNetworkMonitor.cpp NetworkMonitor.cpp
	$(comp) $(compout)/ohNetworkMonitor.$(objext) -c ohNetworkMonitor.cpp
	$(comp) $(compout)/NetworkMonitor.$(objext) -c NetworkMonitor.cpp
	$(link) $(linkout)/ohNetworkMonitor.$(exeext) $(objdir)/ohNetworkMonitor.$(objext) $(objdir)/NetworkMonitor.$(objext) ../ohNet/$(objdir)/DvAvOpenhomeOrgNetworkMonitor1.$(objext) ../ohNet/$(objdir)/$(libprefix)ohNetCore.$(libext) ../ohNet/$(objdir)/$(libprefix)TestFramework.$(libext)

.PHONY : ohNetmon

ohNetmon : $(objdir) $(objdir)/ohNetmon.$(exeext)

$(objdir)/ohNetmon.$(exeext) : ohNetmon.cpp CpNetworkMonitorList1.cpp CpNetworkMonitorList2.cpp
	$(comp) $(compout)/ohNetmon.$(objext) -c ohNetmon.cpp
	$(comp) $(compout)/CpNetworkMonitorList1.$(objext) -c CpNetworkMonitorList1.cpp
	$(comp) $(compout)/CpNetworkMonitorList2.$(objext) -c CpNetworkMonitorList2.cpp
	$(link) $(linkout)/ohNetmon.$(exeext) $(objdir)/ohNetmon.$(objext) $(objdir)/CpNetworkMonitorList1.$(objext) $(objdir)/CpNetworkMonitorList2.$(objext) ../ohNet/$(objdir)/CpAvOpenhomeOrgNetworkMonitor1.$(objext) ../ohNet/$(objdir)/$(libprefix)ohNetCore.$(libext) ../ohNet/$(objdir)/$(libprefix)TestFramework.$(libext)

