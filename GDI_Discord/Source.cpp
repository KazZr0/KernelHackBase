#include "Global.h"

#include "Internals.h"
#include "CryptSTR.h"
#include "CRT.h"
#include "Helpers.h"

//tmp hook data
PVOID* xKdEnumerateDebuggingDevicesPtr;
PVOID xKdEnumerateDebuggingDevicesVal;

bool SetupKernelThread(PVOID KBase, PVOID ThreadStartAddr)
{
	//get thread fake start address
	PVOID hMsVCRT = nullptr;
	auto Process = GetProcessWModule(E("explorer.exe"), E("msvcrt.dll"), &hMsVCRT);
	auto FakeStartAddr = (PUCHAR)GetProcAdress(hMsVCRT, E("_endthreadex")) + 0x30;

	//get usermode func
	auto Var = UAlloc(0x1000); HANDLE Thread = nullptr;
	auto hNtdll = GetUserModuleBase(Process, E("ntdll.dll"));
	auto CallBack = GetProcAdress(hNtdll, E("NtQueryAuxiliaryCounterFrequency"));

	//set kernel hook
	xKdEnumerateDebuggingDevicesPtr = (PVOID*)RVA((ULONG64)KeQueryAuxiliaryCounterFrequency + 4, 7);
	xKdEnumerateDebuggingDevicesVal = _InterlockedExchangePointer(xKdEnumerateDebuggingDevicesPtr, ThreadStartAddr);

	//create new thread
	CLIENT_ID Cid;
	RtlCreateUserThread(ZwCurrentProcess(), nullptr, false, 0, 0, 0, CallBack, Var, &Thread, &Cid);

	if (Thread)
	{
		//close useless handle
		ZwClose(Thread);

		//spoof thread start address
		PETHREAD EThread;
		PsLookupThreadByThreadId(Cid.UniqueThread, &EThread);
		auto StartAddrOff = *(USHORT*)(FindPatternSect(KBase, E("PAGE"), E("48 89 86 ? ? ? ? 48 8B 8C")) + 3);
		*(PVOID*)((ULONG64)EThread + StartAddrOff/*Win32StartAddress*/) = FakeStartAddr;
		ObfDereferenceObject(EThread);

		//wait exec kernel callback
		while (xKdEnumerateDebuggingDevicesPtr && 
			   xKdEnumerateDebuggingDevicesVal) {
			Sleep(10);
		}
	}

	//cleanup
	UFree(Var);
	DetachFromProcess(Process);

	//ret create status
	return (bool)Thread;
}

NTSTATUS FakeThread()
{
	//disable apc
	KeEnterGuardedRegion();

	//unhook kernel hook
	_InterlockedExchangePointer(xKdEnumerateDebuggingDevicesPtr, xKdEnumerateDebuggingDevicesVal);
	xKdEnumerateDebuggingDevicesPtr = 0; xKdEnumerateDebuggingDevicesVal = 0;

	//create gui thread context
	auto hNtdll = GetUserModuleBase(IoGetCurrentProcess(), E("user32.dll"));
	auto CallBack = GetProcAdress(hNtdll, E("GetForegroundWindow"));
	CallUserMode(CallBack);

	//your cheat
	//while (true)
	{
		sp("hook!1!");

		//gRender.NewFrame(1024, 768);

		////gRender.Rectangle(50, 50, 100, 100, RGB(255, 0, 0));

		//gRender.FillRectangle(200, 200, 100, 100, RGB(255, 255, 0));

		//gRender.Line(0, 0, 600, 600, RGB(0, 255, 0));

		////gRender.EndFrame((PUCHAR)buff);

		//Sleep(1);
	}

	//enable all apc
	KeLeaveGuardedRegion();

	//lol return 1!!
	return STATUS_NOT_IMPLEMENTED;
}

//driver entry point
NTSTATUS DriverEntry(PVOID a1, PVOID KBase)
{
	//disable apc
	KeEnterGuardedRegion();

	//create kernel usermode thread
	SetupKernelThread(KBase, FakeThread);

	//enable all apc
	KeLeaveGuardedRegion();

	return STATUS_SUCCESS;
}