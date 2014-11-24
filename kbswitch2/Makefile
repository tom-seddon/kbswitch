CP:=cp.exe
O:=.Output
B:=bin
MKDIR:=mkdir.exe

.PHONY:default
default:
	$(error specify target)

.PHONY:bin
bin:
	$(MKDIR) -p $(B)
	$(CP) -v $(O)/kbswitch2.exe $(B)/
	$(CP) -v $(O)/kbswitch2_dll_x64.dll $(B)/
	$(CP) -v $(O)/kbswitch2_dll_x86.dll $(B)/
	$(CP) -v $(O)/kbswitch2_helper_x64.exe $(B)/
	$(CP) -v $(O)/kbswitch2_helper_x86.exe $(B)/
