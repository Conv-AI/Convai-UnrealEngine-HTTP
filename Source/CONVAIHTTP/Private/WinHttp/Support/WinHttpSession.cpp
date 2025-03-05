// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/Support/WinHttpSession.h"
#include "WinHttp/Support/WinHttpErrorHelper.h"
#include "WinHttp/Support/WinHttpConnection.h"
#include "WinHttp/Support/WinHttpTypes.h"
#include "GenericPlatform/GenericPlatformConvaihttp.h"
#include "Convaihttp.h"

#include "Windows/AllowWindowsPlatformTypes.h"
#include <errhandlingapi.h>

// Check if we support CONVAIHTTP2
#if defined(WINHTTP_PROTOCOL_FLAG_CONVAIHTTP2) && defined(WINHTTP_OPTION_ENABLE_CONVAIHTTP_PROTOCOL)
#define UE_HAS_CONVAIHTTP2_SUPPORT 1
#else // ^^^ defined(WINHTTP_PROTOCOL_FLAG_CONVAIHTTP2) && defined(WINHTTP_OPTION_ENABLE_CONVAIHTTP_PROTOCOL) ^^^ /// vvv !defined(WINHTTP_PROTOCOL_FLAG_CONVAIHTTP2) || !defined(WINHTTP_OPTION_ENABLE_CONVAIHTTP_PROTOCOL) vvv
#define UE_HAS_CONVAIHTTP2_SUPPORT 0
#endif

FCH_WinHttpSession::FCH_WinHttpSession(uint32 SecurityProtocolFlags, const bool bInForceSecureConnections)
	: bForceSecureConnections(bInForceSecureConnections)
{
	const FString UserAgent = FGenericPlatformConvaihttp::GetDefaultUserAgent();
	const DWORD AccessType = WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY;
	LPCWSTR ProxyAddress = WINHTTP_NO_PROXY_NAME;
	LPCWSTR ProxyBypass = WINHTTP_NO_PROXY_BYPASS;
	DWORD Flags = WINHTTP_FLAG_ASYNC;

	// Disable this on Windows now until we can do a runtime check for a future version of Windows10 that supports this flag
#if defined(WINHTTP_FLAG_SECURE_DEFAULTS) && !PLATFORM_WINDOWS
	if (bForceSecureConnections)
	{
		Flags |= WINHTTP_FLAG_SECURE_DEFAULTS;
	}
#endif

	SessionHandle = WinHttpOpen(TCHAR_TO_WCHAR(*UserAgent), AccessType, ProxyAddress, ProxyBypass, Flags);
	if (!SessionHandle.IsValid())
	{
		const DWORD ErrorCode = GetLastError();
		FCH_WinHttpErrorHelper::LogWinConvaiHttpOpenFailure(ErrorCode);
		return;
	}

	DWORD dwSecurityProtocolFlags = SecurityProtocolFlags;
	if (!WinHttpSetOption(SessionHandle.Get(), WINHTTP_OPTION_SECURE_PROTOCOLS, &dwSecurityProtocolFlags, sizeof(dwSecurityProtocolFlags)))
	{
		// Get last error
		const DWORD ErrorCode = GetLastError();
		FCH_WinHttpErrorHelper::LogWinConvaiHttpSetOptionFailure(ErrorCode);

		// Reset handle to signify we failed
		SessionHandle.Reset();
		return;
	}

	// Opportunistically Enable CONVAIHTTP2 if we can
#if UE_HAS_CONVAIHTTP2_SUPPORT
	DWORD FlagEnableCONVAIHTTP2 = WINHTTP_PROTOCOL_FLAG_CONVAIHTTP2;
	if (WinHttpSetOption(SessionHandle.Get(), WINHTTP_OPTION_ENABLE_CONVAIHTTP_PROTOCOL, &FlagEnableCONVAIHTTP2, sizeof(FlagEnableCONVAIHTTP2)))
	{
		UE_LOG(LogWinConvaiHttp, Verbose, TEXT("WinHttp local machine has support for CONVAIHTTP/2"));
	}
	else
	{
		UE_LOG(LogWinConvaiHttp, Verbose, TEXT("WinHttp local machine does not support CONVAIHTTP/2, CONVAIHTTP/1.1 will be used"));
	}
#else // ^^^ UE_HAS_CONVAIHTTP2_SUPPORT ^^^ // vvv !UE_HAS_CONVAIHTTP2_SUPPORT vvv
	UE_LOG(LogWinConvaiHttp, Verbose, TEXT("UE WinHttp compiled without CONVAIHTTP/2 support"));
#endif // !UE_HAS_CONVAIHTTP2_SUPPORT

	// Opportunistically enable request compression if we can
	DWORD FlagEnableCompression = WINHTTP_DECOMPRESSION_FLAG_ALL;
	if (WinHttpSetOption(SessionHandle.Get(), WINHTTP_OPTION_DECOMPRESSION, &FlagEnableCompression, sizeof(FlagEnableCompression)))
	{
		UE_LOG(LogWinConvaiHttp, Verbose, TEXT("WinHttp local machine has support for compression"));
	}
	else
	{
		UE_LOG(LogWinConvaiHttp, Verbose, TEXT("WinHttp local machine does not support for compression"));
	}

	FConvaihttpModule& ConvaihttpModule = FConvaihttpModule::Get();
	const FTimespan ConnectionTimeout = FTimespan::FromSeconds(ConvaihttpModule.GetConvaihttpConnectionTimeout());
	const FTimespan ResolveTimeout = ConnectionTimeout;
	const FTimespan SendTimeout = FTimespan::FromSeconds(ConvaihttpModule.GetConvaihttpSendTimeout());
	const FTimespan ReceiveTimeout = FTimespan::FromSeconds(ConvaihttpModule.GetConvaihttpReceiveTimeout());

	if (!WinHttpSetTimeouts(SessionHandle.Get(), ResolveTimeout.GetTotalMilliseconds(), ConnectionTimeout.GetTotalMilliseconds(), SendTimeout.GetTotalMilliseconds(), ReceiveTimeout.GetTotalMilliseconds()))
	{
		// Get last error
		const DWORD ErrorCode = GetLastError();
		FCH_WinHttpErrorHelper::LogWinConvaiHttpSetTimeoutsFailure(ErrorCode);
	}

	// Success
}

bool FCH_WinHttpSession::IsValid() const
{
	return SessionHandle.IsValid();
}

HINTERNET FCH_WinHttpSession::Get() const
{
	return SessionHandle.Get();
}

bool FCH_WinHttpSession::AreOnlySecureConnectionsAllowed() const
{
	return bForceSecureConnections;
}

#include "Windows/HideWindowsPlatformTypes.h"

#endif // WITH_WINHTTP
