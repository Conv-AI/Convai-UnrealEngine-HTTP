// Copyright Epic Games, Inc. All Rights Reserved.

#include "Apple/ApplePlatformConvaihttp.h"
#include "AppleCONVAIHTTP.h"

#if WITH_SSL
#include "Ssl.h"
#endif

void FApplePlatformConvaihttp::Init()
{
#if WITH_SSL
	// Load SSL module during CONVAIHTTP module's StatupModule() to make sure module manager figures out the dependencies correctly
	// and doesn't unload SSL before unloading CONVAIHTTP module at exit
	FSslModule::Get();
#endif
}


void FApplePlatformConvaihttp::Shutdown()
{
}


IConvaihttpRequest* FApplePlatformConvaihttp::ConstructRequest()
{
	return new FAppleConvaihttpRequest();
}
