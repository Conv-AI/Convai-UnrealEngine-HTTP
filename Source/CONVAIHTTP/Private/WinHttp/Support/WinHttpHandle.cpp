// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/Support/WinHttpHandle.h"
#include "WinHttp/Support/WinHttpErrorHelper.h"
#include "WinHttp/Support/WinHttpTypes.h"
#include "Convaihttp.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <errhandlingapi.h>

FCH_WinHttpHandle::FCH_WinHttpHandle(HINTERNET NewHandle)
	: Handle(NewHandle)
{
}

FCH_WinHttpHandle::~FCH_WinHttpHandle()
{
	if (Handle != nullptr)
	{
		if (!WinHttpCloseHandle(Handle))
		{
			const DWORD ErrorCode = GetLastError();
			FCH_WinHttpErrorHelper::LogWinConvaiHttpCloseHandleFailure(ErrorCode);
		}
		Handle = nullptr;
	}
}

FCH_WinHttpHandle::FCH_WinHttpHandle(FCH_WinHttpHandle&& Other)
	: Handle(Other.Handle)
{
	Other.Handle = nullptr;
}

FCH_WinHttpHandle& FCH_WinHttpHandle::operator=(FCH_WinHttpHandle&& Other)
{
	if (this != &Other)
	{
		HINTERNET TempHandle = Other.Handle;
		Other.Handle = Handle;
		Handle = TempHandle;
	}

	return *this;
}

FCH_WinHttpHandle& FCH_WinHttpHandle::operator=(HINTERNET NewHandle)
{
	if (Handle != NewHandle)
	{
		*this = FCH_WinHttpHandle(NewHandle);
	}

	return *this;
}

void FCH_WinHttpHandle::Reset()
{
	*this = FCH_WinHttpHandle();
}

FCH_WinHttpHandle::operator bool() const
{
	return IsValid();
}

bool FCH_WinHttpHandle::IsValid() const
{
	return Handle != nullptr;
}

HINTERNET FCH_WinHttpHandle::Get() const
{
	return Handle;
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif // WITH_WINHTTP
