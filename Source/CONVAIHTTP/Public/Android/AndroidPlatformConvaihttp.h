// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GenericPlatform/GenericPlatformConvaihttp.h"

/**
 * Platform specific Convaihttp implementations
 */
class FAndroidPlatformConvaihttp : public FGenericPlatformConvaihttp
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

	/**
	 * Get the proxy address specified by the operating system
	 *
	 * @return optional FString: If unset: we are unable to get information from the operating system. If set: the proxy address set by the operating system (may be blank)
	 */
	static TOptional<FString> GetOperatingSystemProxyAddress();

	/**
	 * Check if getting proxy information from the current operating system is supported
	 * Useful for "Network Settings" type pages.  GetProxyAddress may return an empty or populated string but that does not imply
	 * the operating system does or does not support proxies (or that it has been implemented here)
	 * 
	 * @return true if we are able to get proxy information from the current operating system, false if not
	 */
	static bool IsOperatingSystemProxyInformationSupported();
};


typedef FAndroidPlatformConvaihttp FPlatformConvaihttp;