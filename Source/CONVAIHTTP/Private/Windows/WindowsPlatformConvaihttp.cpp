// Copyright Epic Games, Inc. All Rights Reserved.

#include "Windows/WindowsPlatformConvaihttp.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/ConfigCacheIni.h"
#if WITH_CURL
	#include "Curl/CurlConvaihttp.h"
	#include "Curl/CurlConvaihttpManager.h"
#else // ^^^ WITH_CURL  ^^^ // vvv !WITH_CURL  vvv
	#include "WinHttp/WinHttpConvaihttpManager.h"
	#include "WinHttp/WinHttpConvaihttpRequest.h"
#endif // !WITH_CURL 
#include "Convaihttp.h"
#include "WinHttp/Support/WinHttpTypes.h" // Always include for OS proxy settings

static bool IsUnsignedInteger(const FString& InString)
{
	bool bResult = true;
	for (auto CharacterIter : InString)
	{
		if (!FChar::IsDigit(CharacterIter))
		{
			bResult = false;
			break;
		}
	}
	return bResult;
}

static bool IsValidIPv4Address(const FString& InString)
{
	bool bResult = false;

	FString Temp;
	FString AStr, BStr, CStr, DStr, PortStr;

	bool bWasPatternMatched = false;
	if (InString.Split(TEXT("."), &AStr, &Temp))
	{
		if (Temp.Split(TEXT("."), &BStr, &Temp))
		{
			if (Temp.Split(TEXT("."), &CStr, &Temp))
			{
				if (Temp.Split(TEXT(":"), &DStr, &PortStr))
				{
					bWasPatternMatched = true;
				}
			}
		}
	}

	if (bWasPatternMatched)
	{
		if (IsUnsignedInteger(AStr) && IsUnsignedInteger(BStr) && IsUnsignedInteger(CStr) && IsUnsignedInteger(DStr) && IsUnsignedInteger(PortStr))
		{
			uint32 A, B, C, D, Port;
			LexFromString(A, *AStr);
			LexFromString(B, *BStr);
			LexFromString(C, *CStr);
			LexFromString(D, *DStr);
			LexFromString(Port, *PortStr);

			if (A < 256 && B < 256 && C < 256 && D < 256 && Port < 65536)
			{
				bResult = true;
			}
		}
	}

	return bResult;
}

void FWindowsPlatformConvaihttp::Init()
{
	FString ConvaihttpMode;
	if (FParse::Value(FCommandLine::Get(), TEXT("CONVAIHTTP="), ConvaihttpMode) &&
		(ConvaihttpMode.Equals(TEXT("WinInet"), ESearchCase::IgnoreCase)))
	{
		UE_LOG(LogConvaihttp, Warning, TEXT("-CONVAIHTTP=WinInet is no longer valid"));
	}

	FGenericPlatformConvaihttp::Init();

#if WITH_CURL
	FCurlConvaihttpManager::InitCurl();
#endif // WITH_WINHTTP
}

void FWindowsPlatformConvaihttp::Shutdown()
{
#if WITH_CURL
	FCurlConvaihttpManager::ShutdownCurl();
#endif // WITH_CURL 

	FGenericPlatformConvaihttp::Shutdown();
}

FConvaihttpManager * FWindowsPlatformConvaihttp::CreatePlatformConvaihttpManager()
{
#if WITH_CURL
	return new FCurlConvaihttpManager();
#else // ^^^ WITH_CURL  ^^^ // vvv WITH_CURL  vvv
	return new FWinHttpConvaihttpManager();
#endif // !WITH_CURL 
}

IConvaihttpRequest* FWindowsPlatformConvaihttp::ConstructRequest()
{
#if WITH_CURL
		return new FCurlConvaihttpRequest();
#else // ^^^ WITH_CURL  ^^^ // vvv WITH_CURL  vvv
		return new FWinHttpConvaihttpRequest();
#endif // !WITH_CURL 
}

FString FWindowsPlatformConvaihttp::GetMimeType(const FString& FilePath)
{
	FString MimeType = TEXT("application/unknown");
	const FString FileExtension = FPaths::GetExtension(FilePath, true);

	HKEY hKey;
	if ( ::RegOpenKeyEx(HKEY_CLASSES_ROOT, *FileExtension, 0, KEY_READ, &hKey) == ERROR_SUCCESS )
	{
		TCHAR MimeTypeBuffer[128];
		DWORD MimeTypeBufferSize = sizeof(MimeTypeBuffer);
		DWORD KeyType = 0;

		if ( ::RegQueryValueEx(hKey, TEXT("Content Type"), NULL, &KeyType, (BYTE*)MimeTypeBuffer, &MimeTypeBufferSize) == ERROR_SUCCESS && KeyType == REG_SZ )
		{
			MimeType = MimeTypeBuffer;
		}

		::RegCloseKey(hKey);
	}

	return MimeType;
}

TOptional<FString> FWindowsPlatformConvaihttp::GetOperatingSystemProxyAddress()
{
	FString ProxyAddress;

	// Retrieve the default proxy configuration.
	WINHTTP_PROXY_INFO DefaultProxyInfo;
	memset(&DefaultProxyInfo, 0, sizeof(DefaultProxyInfo));
	WinHttpGetDefaultProxyConfiguration(&DefaultProxyInfo);

	if (DefaultProxyInfo.lpszProxy != nullptr)
	{
		FString TempProxy(DefaultProxyInfo.lpszProxy);
		if (IsValidIPv4Address(TempProxy))
		{
			ProxyAddress = MoveTemp(TempProxy);
		}
		else
		{
			if (TempProxy.Split(TEXT("convaihttps="), nullptr, &TempProxy))
			{
				TempProxy.Split(TEXT(";"), &TempProxy, nullptr);
				if (IsValidIPv4Address(TempProxy))
				{
					ProxyAddress = MoveTemp(TempProxy);
				}
			}
		}
	}

	// Look for the proxy setting for the current user. Charles proxies count in here.
	if (ProxyAddress.IsEmpty())
	{
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG IeProxyInfo;
		memset(&IeProxyInfo, 0, sizeof(IeProxyInfo));
		WinHttpGetIEProxyConfigForCurrentUser(&IeProxyInfo);

		if (IeProxyInfo.lpszProxy != nullptr)
		{
			FString TempProxy(IeProxyInfo.lpszProxy);
			if (IsValidIPv4Address(TempProxy))
			{
				ProxyAddress = MoveTemp(TempProxy);
			}
			else
			{
				if (TempProxy.Split(TEXT("convaihttps="), nullptr, &TempProxy))
				{
					TempProxy.Split(TEXT(";"), &TempProxy, nullptr);
					if (IsValidIPv4Address(TempProxy))
					{
						ProxyAddress = MoveTemp(TempProxy);
					}
				}
			}
		}
	}
	return ProxyAddress;
}

bool FWindowsPlatformConvaihttp::IsOperatingSystemProxyInformationSupported()
{
	return true;
}

bool FWindowsPlatformConvaihttp::VerifyPeerSslCertificate(bool verify)
{
	bool prev = false;
#if WITH_CURL
	prev = FCurlConvaihttpManager::CurlRequestOptions.bVerifyPeer;
	FCurlConvaihttpManager::CurlRequestOptions.bVerifyPeer = verify;
#endif // #if WITH_CURL 
	return prev;
}
