// Copyright Epic Games, Inc. All Rights Reserved.

#include "Unix/UnixPlatformConvaihttp.h"
#include "Curl/CurlConvaihttp.h"
#include "Curl/CurlConvaihttpManager.h"

void FUnixPlatformConvaihttp::Init()
{
	FCurlConvaihttpManager::InitCurl();
}

class FConvaihttpManager * FUnixPlatformConvaihttp::CreatePlatformConvaihttpManager()
{
	return new FCurlConvaihttpManager();
}

void FUnixPlatformConvaihttp::Shutdown()
{
	FCurlConvaihttpManager::ShutdownCurl();
}

IConvaihttpRequest* FUnixPlatformConvaihttp::ConstructRequest()
{
	return new FCurlConvaihttpRequest();
}

