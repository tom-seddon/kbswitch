#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <shellapi.h>

#include "resource.h"

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const char g_aMutexName[]="kbswitch_mutex";

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static const TCHAR g_aWndClassName[]=_T("kbswitch_wnd");
static HWND g_hWnd;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#ifdef _DEBUG

// Rather unsafe
static void dprintf(const TCHAR *pFmt,...)
{
	TCHAR aStr[1000];
	va_list v;

	va_start(v,pFmt);
	wvsprintf(aStr,pFmt,v);
	va_end(v);

	OutputDebugString(aStr);
}

#else

#define dprintf(...) ((void)0)

#endif

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// Got bored of dynamically allocating everything...
enum {LOCALIZED_STR_SIZE=100};

// Random...
enum {NOTIFY_MSG=WM_APP+1};
enum {NOTIFY_ID=1};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// http://blogs.msdn.com/b/michkap/archive/2006/05/06/591174.aspx -- :(
static const char g_aRegKeyNameRoot[]="SYSTEM\\CurrentControlSet\\Control\\Keyboard Layouts\\";
enum {REG_KEY_NAME_ROOT_LEN=sizeof g_aRegKeyNameRoot/sizeof g_aRegKeyNameRoot[0]-1};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int NotifyCtl(DWORD action,const TCHAR *pTip)
{
	NOTIFYICONDATA nid;

	nid.cbSize=sizeof nid;
	nid.hWnd=g_hWnd;
	nid.uID=NOTIFY_ID;
	nid.uFlags=NIF_ICON|NIF_MESSAGE;
	nid.uCallbackMessage=NOTIFY_MSG;
	nid.hIcon=LoadIcon(GetModuleHandle(0),MAKEINTRESOURCE(IDI_ICON1));

	if(pTip)
	{
		nid.uFlags|=NIF_TIP;
		lstrcpyn(nid.szTip,pTip,sizeof nid.szTip/sizeof nid.szTip[0]);
	}

	if(!Shell_NotifyIcon(action,&nid))
		return 0;

	return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static TCHAR *GetRegString(HKEY hRootKey,const char *pKeyName,const wchar_t *pValueName)
{
	HKEY hKey;
	DWORD valueType,valueSizeBytes;
	int good=0;
	TCHAR *pValueValue=0;

	if(RegOpenKeyExA(hRootKey,pKeyName,0,KEY_READ,&hKey)!=ERROR_SUCCESS)
		goto bye;

	if(RegQueryValueEx(hKey,pValueName,0,&valueType,0,&valueSizeBytes)!=ERROR_SUCCESS)
		goto bye;

	if(valueType!=REG_SZ)
		goto bye;

	// Add one for the '\x0', if it wasn't there already, and make it
    // at least LOCALIZED_STR_SIZE too.
	valueSizeBytes+=sizeof(TCHAR);
	valueSizeBytes=min(valueSizeBytes,LOCALIZED_STR_SIZE*sizeof(TCHAR));

	pValueValue=LocalAlloc(0,valueSizeBytes);
	if(!pValueValue)
		goto bye;

	if(RegQueryValueEx(hKey,pValueName,0,&valueType,(BYTE *)pValueValue,
		&valueSizeBytes)!=ERROR_SUCCESS)
	{
		goto bye;
	}

	good=1;

bye:
	if(hKey)
		RegCloseKey(hKey);

	if(!good)
	{
		LocalFree(pValueValue);
		pValueValue=0;
	}

	return pValueValue;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static TCHAR *GetLayoutDisplayName(HKL hkl)
{
	TCHAR *pDisplayName;
	char aRegKeyName[REG_KEY_NAME_ROOT_LEN+KL_NAMELENGTH];

	// Activate keyboard layout to get its name, forming the registry key name.
	{
		HKL hklOld=GetKeyboardLayout(0);

		if(!ActivateKeyboardLayout(hkl,0))
			return 0;

		lstrcpyA(aRegKeyName,g_aRegKeyNameRoot);
		
		if(!GetKeyboardLayoutNameA(aRegKeyName+REG_KEY_NAME_ROOT_LEN))
			return 0;

		if(!ActivateKeyboardLayout(hklOld,0))
			return 0;
	}

	pDisplayName=GetRegString(HKEY_LOCAL_MACHINE,aRegKeyName,_T("Layout Display Name"));
	if(!pDisplayName)
		return 0;

	if(pDisplayName[0]==_T('@'))
	{
		// With any luck, SHLoadIndirectString will just leave the input alone
        // if it fails...
		SHLoadIndirectString(pDisplayName,pDisplayName,LOCALIZED_STR_SIZE,0);
	}

	return pDisplayName;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

// static int GetCountryName(HKL hkl,TCHAR *pCountryName)
// {
// 	if(!GetLocaleInfo(LOWORD(hkl),LOCALE_SCOUNTRY,pCountryName,LOCALIZED_STR_SIZE))
// 		return 0;
// 
// 	return 1;
// }

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

struct LayoutsList
{
	int numHKLs;
	HKL *pHKLs;
	TCHAR **ppDisplayNames;
};
typedef struct LayoutsList LayoutsList;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DeleteLayoutsList(LayoutsList *pList)
{
	if(pList)
	{
		int i;

		LocalFree(pList->pHKLs);

		for(i=0;i<pList->numHKLs;++i)
			LocalFree(pList->ppDisplayNames[i]);

		LocalFree(pList->ppDisplayNames);

		LocalFree(pList);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LayoutsList *CreateLayoutsList(void)
{
	int good=0;
	int i;
	LayoutsList *pList;

	pList=LocalAlloc(LMEM_ZEROINIT,sizeof *pList);
	if(!pList)
		return 0;

	pList->numHKLs=GetKeyboardLayoutList(0,0);
	if(pList->numHKLs==0)
		goto bye;

	pList->pHKLs=LocalAlloc(0,pList->numHKLs*sizeof *pList->pHKLs);
	pList->ppDisplayNames=LocalAlloc(LMEM_ZEROINIT,pList->numHKLs*sizeof *pList->ppDisplayNames);
	if(!pList->pHKLs||!pList->ppDisplayNames)
		goto bye;

	if(GetKeyboardLayoutList(pList->numHKLs,pList->pHKLs)!=pList->numHKLs)
		goto bye;

	dprintf(_T("%d layouts:\n"),pList->numHKLs);
	for(i=0;i<pList->numHKLs;++i)
	{
// 		TCHAR aLayoutName[LOCALIZED_STR_SIZE];
// 		TCHAR aCountryName[LOCALIZED_STR_SIZE];
// 
// 		if(!GetCountryName(g_pHKLs[i],aCountryName))
// 			return 0;

		pList->ppDisplayNames[i]=GetLayoutDisplayName(pList->pHKLs[i]);
		if(!pList->ppDisplayNames[i])
			goto bye;

		dprintf(_T("    %u. HKL=0x%08X: \"%s\".\n"),i,pList->pHKLs[i],pList->ppDisplayNames[i]);
	}

	good=1;

bye:
	if(!good)
	{
		DeleteLayoutsList(pList);
		pList=0;
	}

	return pList;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int GetActiveLayoutIdx(LayoutsList *pList)
{
	if(pList)
	{
		HKL hklCur=GetKeyboardLayout(0);
		int i;

		for(i=0;i<pList->numHKLs;++i)
		{
			if(pList->pHKLs[i]==hklCur)
				return i;
		}
	}

	return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static void DoPopupMenu(void)
{
	int idLayoutsBegin,idLayoutsEnd,idExit,cmd;
	POINT mpt;
	LayoutsList *pList=CreateLayoutsList();
	HMENU hMenu=CreatePopupMenu();

	SetForegroundWindow(g_hWnd);

	// Fill menu
	{
		int id=1;

		idLayoutsBegin=id;
		if(pList)
		{
			int i;
			int activeIdx=GetActiveLayoutIdx(pList);

			for(i=0;i<pList->numHKLs;++i)
			{
				UINT flags=0;

				if(i==activeIdx)
					flags|=MF_CHECKED;

				AppendMenu(hMenu,flags,id++,pList->ppDisplayNames[i]);
			}

			AppendMenu(hMenu,MF_SEPARATOR,0,0);
		}
		idLayoutsEnd=id;

		idExit=id++;
		AppendMenuA(hMenu,0,idExit,"E&xit");
	}

	// Popup bum
	GetCursorPos(&mpt);
	cmd=TrackPopupMenuEx(hMenu,TPM_RETURNCMD,mpt.x,mpt.y,g_hWnd,0);

	// Do what?
	if(cmd==idExit)
	{
		// Bye...
		PostQuitMessage(0);
	}
	else if(cmd>=idLayoutsBegin&&cmd<idLayoutsEnd)
	{
		// Set new layout
		int idx=cmd-idLayoutsBegin;

		SystemParametersInfo(SPI_SETDEFAULTINPUTLANG,0,&pList->pHKLs[idx],0);
		PostMessage(HWND_BROADCAST,WM_INPUTLANGCHANGEREQUEST,0,(LPARAM)pList->pHKLs[idx]);

		NotifyCtl(NIM_MODIFY,pList->ppDisplayNames[idx]);
	}
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static LRESULT CALLBACK WndProc(HWND hWnd,UINT uMsg,WPARAM wParam,LPARAM lParam)
{
	switch(uMsg)
	{
	case NOTIFY_MSG:
		{
			switch(lParam)
			{
			case WM_RBUTTONDOWN:
				DoPopupMenu();
				return 0;
			}
		}
		break;

	case WM_CLOSE:
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProc(hWnd,uMsg,wParam,lParam);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int CreateWnd(void)
{
	WNDCLASSEX w;

	w.cbClsExtra=0;
	w.cbSize=sizeof w;
	w.cbWndExtra=0;
	w.hbrBackground=0;//GetStockObject(NULL_BRUSH);
	w.hCursor=0;//LoadCursor(0,IDC_ARROW);
	w.hIcon=0;//LoadIcon(0,IDI_APPLICATION);
	w.hIconSm=w.hIcon;
	w.hInstance=GetModuleHandle(0);
	w.lpfnWndProc=&WndProc;
	w.lpszClassName=g_aWndClassName;
	w.lpszMenuName=0;
	w.style=CS_HREDRAW|CS_VREDRAW;

	if(!RegisterClassEx(&w))
		return 0;

	g_hWnd=CreateWindow(g_aWndClassName,_T("kbswitch"),WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,CW_USEDEFAULT,0,0,
		GetModuleHandle(0),0);
	if(!g_hWnd)
		return 0;

#ifdef _DEBUG
	ShowWindow(g_hWnd,SW_SHOW);
#endif//_DEBUG

	return 1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

static int Main()
{
	// Create "window"
	if(!CreateWnd())
		return 1;

	// Create notify icon with display name of currently selected layout
	{
		LayoutsList *pList=CreateLayoutsList();
		int good;
		const TCHAR *pDisplayName=0;
		int activeIdx=GetActiveLayoutIdx(pList);

		if(activeIdx>=0)
			pDisplayName=pList->ppDisplayNames[activeIdx];

		good=NotifyCtl(NIM_ADD,pDisplayName);

		DeleteLayoutsList(pList);

		if(!good)
			return 1;
	}

	// Standard message guff
	for(;;)
	{
		int r;
		MSG msg;

		r=GetMessage(&msg,g_hWnd,0,0);
		if(r==0||r==-1)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	// Remove notify icon
	NotifyCtl(NIM_DELETE,0);

	return 0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void EntryPoint(void)
{
	HANDLE hMutex;
	int r=1;

	hMutex=OpenMutexA(MUTEX_ALL_ACCESS,FALSE,g_aMutexName);
	if(!hMutex)
	{
		hMutex=CreateMutexA(0,FALSE,g_aMutexName);

			r=Main();

		ReleaseMutex(hMutex);
	}

	ExitProcess(r);
}
