// Copyright Epic Games, Inc. All Rights Reserved.

#include "Android/AndroidPlatformConvaihttp.h"
#include "Curl/CurlConvaihttp.h"
#include "Curl/CurlConvaihttpManager.h"

void FAndroidPlatformConvaihttp::Init()
{
	FCurlConvaihttpManager::InitCurl();
}

class FConvaihttpManager * FAndroidPlatformConvaihttp::CreatePlatformConvaihttpManager()
{
	return new FCurlConvaihttpManager();
}

void FAndroidPlatformConvaihttp::Shutdown()
{
	FCurlConvaihttpManager::ShutdownCurl();
}

IConvaihttpRequest* FAndroidPlatformConvaihttp::ConstructRequest()
{
	return new FCurlConvaihttpRequest();
}

TOptional<FString> FAndroidPlatformConvaihttp::GetOperatingSystemProxyAddress()
{
	FString ProxyAddress;

#if USE_ANDROID_JNI
	extern int32 AndroidThunkCpp_GetMetaDataInt(const FString& Key);
	extern FString AndroidThunkCpp_GetMetaDataString(const FString& Key);

	FString ProxyHost = AndroidThunkCpp_GetMetaDataString(TEXT("unreal.convaihttp.proxy.proxyHost"));
	int32 ProxyPort = AndroidThunkCpp_GetMetaDataInt(TEXT("unreal.convaihttp.proxy.proxyPort"));

	if (ProxyPort != -1 && !ProxyHost.IsEmpty())
	{
		ProxyAddress = FString::Printf(TEXT("%s:%d"), *ProxyHost, ProxyPort);
	}
#endif

	return ProxyAddress;
}

bool FAndroidPlatformConvaihttp::IsOperatingSystemProxyInformationSupported()
{
	return true;
}