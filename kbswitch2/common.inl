#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <tchar.h>

#pragma warning(error:4024)//different types for formal and actual parameter 

static void dprintf(const char *fmt,...)
{
	char str[1000];
	va_list v;

	va_start(v,fmt);
	wvsprintfA(str,fmt,v);
	va_end(v);

	OutputDebugStringA(str);
}
