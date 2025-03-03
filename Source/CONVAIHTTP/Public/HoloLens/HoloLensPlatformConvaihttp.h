// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformConvaihttp.h"


/**
* Platform specific CONVAIHTTP implementations.
*/
class FHoloLensConvaihttp : public FGenericPlatformConvaihttp
{
public:

	/** Platform initialization step. */
	static void Init();

	/**
	* Creates a platform-specific CONVAIHTTP manager.
	*
	* @return nullptr if default implementation is to be used.
	*/
	static FConvaihttpManager* CreatePlatformConvaihttpManager();

	/** Platform shutdown step. */
	static void Shutdown();

	/**
	* Creates a new CONVAIHTTP request instance for the current platform.
	*
	* @return The request object.
	*/
	static IConvaihttpRequest* ConstructRequest();
};


typedef FHoloLensConvaihttp FPlatformConvaihttp;
