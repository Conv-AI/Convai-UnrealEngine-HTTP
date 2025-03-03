// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_CURL

#include "ConvaihttpThread.h"

#if PLATFORM_MICROSOFT
#include "Microsoft/WindowsHWrapper.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
#endif
#if WITH_CURL_XCURL
//We copied this template to include the windows file from WindowsHWrapper's way if including MinWindows.h, since including xcurl.h directly caused gnarly build errors
#include "CoreTypes.h"
#include "HAL/PlatformMemory.h"
#include "Microsoft/PreWindowsApi.h"
#ifndef STRICT
#define STRICT
#endif
#include "xcurl.h"
#include "Microsoft/PostWindowsApi.h"
#else
#include "curl/curl.h"
#endif
#if PLATFORM_MICROSOFT
#include "Microsoft/HideMicrosoftPlatformTypes.h"
#endif

#endif //WITH_CURL

class IConvaihttpThreadedRequest;

#if WITH_CURL

class FCurlConvaihttpThread
	: public FConvaihttpThread
{
public:
	
	FCurlConvaihttpThread();

protected:
	//~ Begin FConvaihttpThread Interface
	virtual void ConvaihttpThreadTick(float DeltaSeconds) override;
	virtual bool StartThreadedRequest(IConvaihttpThreadedRequest* Request) override;
	virtual void CompleteThreadedRequest(IConvaihttpThreadedRequest* Request) override;
	//~ End FConvaihttpThread Interface
protected:

	/** Mapping of libcurl easy handles to CONVAIHTTP requests */
	TMap<CURL*, IConvaihttpThreadedRequest*> HandlesToRequests;
};


#endif //WITH_CURL
