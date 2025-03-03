// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformConvaihttp.h"

/**
 * Platform specific Convaihttp implementations
 */
class FApplePlatformConvaihttp : public FGenericPlatformConvaihttp
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
	static FConvaihttpManager* CreatePlatformConvaihttpManager()
	{
		return nullptr;
	}

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


typedef FApplePlatformConvaihttp FPlatformConvaihttp;