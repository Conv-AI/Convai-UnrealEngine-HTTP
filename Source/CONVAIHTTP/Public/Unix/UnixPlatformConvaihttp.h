// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformConvaihttp.h"

class FConvaihttpManager;
class IConvaihttpRequest;

/**
 * Platform specific Convaihttp implementations
 */
class FUnixPlatformConvaihttp : public FGenericPlatformConvaihttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific CONVAIHTTP manager.
	 *
	 * @return NULL if default implementation is to be used
	 */
	static FConvaihttpManager* CreatePlatformConvaihttpManager();

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Convaihttp request instance for the current platform
	 *
	 * @return request object
	 */
	static IConvaihttpRequest* ConstructRequest();
};


typedef FUnixPlatformConvaihttp FPlatformConvaihttp;
