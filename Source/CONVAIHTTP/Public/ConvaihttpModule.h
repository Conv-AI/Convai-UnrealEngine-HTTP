// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Interfaces/IConvaihttpRequest.h"
#include "Modules/ModuleInterface.h"

class FConvaihttpManager;

/**
 * Module for Convaihttp request implementations
 * Use FConvaihttpFactory to create a new Convaihttp request
 */
class FConvaihttpModule : 
	public IModuleInterface, public FSelfRegisteringExec
{

public:

	// FSelfRegisteringExec

	/**
	 * Handle exec commands starting with "CONVAIHTTP"
	 *
	 * @param InWorld	the world context
	 * @param Cmd		the exec command being executed
	 * @param Ar		the archive to log results to
	 *
	 * @return true if the handler consumed the input, false to continue searching handlers
	 */
	virtual bool Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar) override;

	/** 
	 * Exec command handlers
	 */
	bool HandleCONVAIHTTPCommand( const TCHAR* Cmd, FOutputDevice& Ar );

	// FConvaihttpModule

	/**
	 * Singleton-like access to this module's interface.  This is just for convenience!
	 * Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	 *
	 * @return Returns singleton instance, loading the module on demand if needed
	 */
	CONVAIHTTP_API static FConvaihttpModule& Get();

	/**
	 * Update all config-based values
	 */
	CONVAIHTTP_API void UpdateConfigs();

	/**
	 * Instantiates a new Convaihttp request for the current platform
	 *
	 * @return new Convaihttp request instance
	 */
	virtual TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> CreateRequest();

	/**
	 * Only meant to be used by Convaihttp request/response implementations
	 *
	 * @return Convaihttp request manager used by the module
	 */
	inline FConvaihttpManager& GetConvaihttpManager()
	{
		check(ConvaihttpManager != NULL);
		return *ConvaihttpManager;
	}

	/**
	 * @return timeout in seconds for the entire convaihttp request to complete
	 */
	inline float GetConvaihttpTimeout() const
	{
		return ConvaihttpTimeout;
	}

	/**
	 * Sets timeout in seconds for the entire convaihttp request to complete
	 */
	inline void SetConvaihttpTimeout(float TimeOutInSec)
	{
		ConvaihttpTimeout = TimeOutInSec;
	}

	/**
	 * @return timeout in seconds to establish the connection
	 */
	inline float GetConvaihttpConnectionTimeout() const
	{
		return ConvaihttpConnectionTimeout;
	}

	/**
	 * @return timeout in seconds to receive a response on the connection 
	 */
	inline float GetConvaihttpReceiveTimeout() const
	{
		return ConvaihttpReceiveTimeout;
	}

	/**
	 * @return timeout in seconds to send a request on the connection
	 */
	inline float GetConvaihttpSendTimeout() const
	{
		return ConvaihttpSendTimeout;
	}

	/**
	 * @return max number of simultaneous connections to a specific server
	 */
	inline int32 GetConvaihttpMaxConnectionsPerServer() const
	{
		return ConvaihttpMaxConnectionsPerServer;
	}

	/**
	 * @return max read buffer size for convaihttp requests
	 */
	inline int32 GetMaxReadBufferSize() const
	{
		return MaxReadBufferSize;
	}

	/**
	 * Sets the maximum size for the read buffer
	 * @param SizeInBytes	The maximum number of bytes to use for the read buffer
	 */
	inline void SetMaxReadBufferSize(int32 SizeInBytes)
	{
		MaxReadBufferSize = SizeInBytes;
	}

	/**
	 * @return true if convaihttp requests are enabled
	 */
	inline bool IsConvaihttpEnabled() const
	{
		return bEnableConvaihttp;
	}

	/**
	 * toggle null convaihttp implementation
	 */
	inline void ToggleNullConvaihttp(bool bEnabled)
	{
		bUseNullConvaihttp = bEnabled;
	}

	/**
	 * @return true if null convaihttp is being used
	 */
	inline bool IsNullConvaihttpEnabled() const
	{
		return bUseNullConvaihttp;
	}

	/**
	 * @return min delay time for each convaihttp request
	 */
	inline float GetConvaihttpDelayTime() const
	{
		return ConvaihttpDelayTime;
	}

	/**
	 * Set the min delay time for each convaihttp request
	 */
	inline void SetConvaihttpDelayTime(float InConvaihttpDelayTime)
	{
		ConvaihttpDelayTime = InConvaihttpDelayTime;
	}

	/**
	 * @return Target tick rate of an active convaihttp thread
	 */
	inline float GetConvaihttpThreadActiveFrameTimeInSeconds() const
	{
		return ConvaihttpThreadActiveFrameTimeInSeconds;
	}

	/**
	 * Set the target tick rate of an active convaihttp thread
	 */
	inline void SetConvaihttpThreadActiveFrameTimeInSeconds(float InConvaihttpThreadActiveFrameTimeInSeconds)
	{
		ConvaihttpThreadActiveFrameTimeInSeconds = InConvaihttpThreadActiveFrameTimeInSeconds;
	}

	/**
	 * @return Minimum sleep time of an active convaihttp thread
	 */
	inline float GetConvaihttpThreadActiveMinimumSleepTimeInSeconds() const
	{
		return ConvaihttpThreadActiveMinimumSleepTimeInSeconds;
	}

	/**
	 * Set the minimum sleep time of an active convaihttp thread
	 */
	inline void SetConvaihttpThreadActiveMinimumSleepTimeInSeconds(float InConvaihttpThreadActiveMinimumSleepTimeInSeconds)
	{
		ConvaihttpThreadActiveMinimumSleepTimeInSeconds = InConvaihttpThreadActiveMinimumSleepTimeInSeconds;
	}

	/**
	 * @return Target tick rate of an idle convaihttp thread
	 */
	inline float GetConvaihttpThreadIdleFrameTimeInSeconds() const
	{
		return ConvaihttpThreadIdleFrameTimeInSeconds;
	}

	/**
	 * Set the target tick rate of an idle convaihttp thread
	 */
	inline void SetConvaihttpThreadIdleFrameTimeInSeconds(float InConvaihttpThreadIdleFrameTimeInSeconds)
	{
		ConvaihttpThreadIdleFrameTimeInSeconds = InConvaihttpThreadIdleFrameTimeInSeconds;
	}

	/**
	 * @return Minimum sleep time when idle, waiting for requests
	 */
	inline float GetConvaihttpThreadIdleMinimumSleepTimeInSeconds() const
	{
		return ConvaihttpThreadIdleMinimumSleepTimeInSeconds;
	}

	/**
	 * Set the minimum sleep time when idle, waiting for requests
	 */
	inline void SetConvaihttpThreadIdleMinimumSleepTimeInSeconds(float InConvaihttpThreadIdleMinimumSleepTimeInSeconds)
	{
		ConvaihttpThreadIdleMinimumSleepTimeInSeconds = InConvaihttpThreadIdleMinimumSleepTimeInSeconds;
	}

	/**
	 * Get the default headers that are appended to every request
	 * @return the default headers
	 */
	const TMap<FString, FString>& GetDefaultHeaders() const { return DefaultHeaders; }

	/**
	 * Add a default header to be appended to future requests
	 * If a request already specifies this header, then the defaulted version will not be used
	 * @param HeaderName - Name of the header (e.g., "Content-Type")
	 * @param HeaderValue - Value of the header
	 */
	void AddDefaultHeader(const FString& HeaderName, const FString& HeaderValue) { DefaultHeaders.Emplace(HeaderName, HeaderValue); }

	/**
	 * @returns The current proxy address.
	 */
	inline const FString& GetProxyAddress() const
	{
		return ProxyAddress;
	}

	/**
	 * Setter for the proxy address.
	 * @param InProxyAddress - New proxy address to use.
	 */
	inline void SetProxyAddress(const FString& InProxyAddress)
	{
		ProxyAddress = InProxyAddress;
	}

	/**
	 * Method to check dynamic proxy setting support.
	 * @returns Whether this convaihttp implementation supports dynamic proxy setting.
	 */
	inline bool SupportsDynamicProxy() const
	{
		return bSupportsDynamicProxy;
	}

	/**
	 * @returns the list of domains allowed to be visited in a shipping build
	 */
	inline const TArray<FString>& GetAllowedDomains() const
	{
		return AllowedDomains;
	}

private:

	// IModuleInterface

	/**
	 * Called when Convaihttp module is loaded
	 * load dependant modules
	 */
	virtual void StartupModule() override;

	/**
	 * Called after Convaihttp module is loaded
	 * Initialize platform specific parts of Convaihttp handling
	 */
	virtual void PostLoadCallback() override;
	
	/**
	 * Called before Convaihttp module is unloaded
	 * Shutdown platform specific parts of Convaihttp handling
	 */
	virtual void PreUnloadCallback() override;

	/**
	 * Called when Convaihttp module is unloaded
	 */
	virtual void ShutdownModule() override;


	/** Keeps track of Convaihttp requests while they are being processed */
	FConvaihttpManager* ConvaihttpManager;
	/** timeout in seconds for the entire convaihttp request to complete. 0 is no timeout */
	float ConvaihttpTimeout;
	/** timeout in seconds to establish the connection. -1 for system defaults, 0 is no timeout */
	float ConvaihttpConnectionTimeout;
	/** timeout in seconds to receive a response on the connection. -1 for system defaults */
	float ConvaihttpReceiveTimeout;
	/** timeout in seconds to send a request on the connection. -1 for system defaults */
	float ConvaihttpSendTimeout;
	/** total time to delay the request */
	float ConvaihttpDelayTime;
	/** Time in seconds to use as frame time when actively processing requests. 0 means no frame time. */
	float ConvaihttpThreadActiveFrameTimeInSeconds;
	/** Time in seconds to sleep minimally when actively processing requests. */
	float ConvaihttpThreadActiveMinimumSleepTimeInSeconds;
	/** Time in seconds to use as frame time when idle, waiting for requests. 0 means no frame time. */
	float ConvaihttpThreadIdleFrameTimeInSeconds;
	/** Time in seconds to sleep minimally when idle, waiting for requests. */
	float ConvaihttpThreadIdleMinimumSleepTimeInSeconds;
	/** Max number of simultaneous connections to a specific server */
	int32 ConvaihttpMaxConnectionsPerServer;
	/** Max buffer size for individual convaihttp reads */
	int32 MaxReadBufferSize;
	/** toggles convaihttp requests */
	bool bEnableConvaihttp;
	/** toggles null (mock) convaihttp requests */
	bool bUseNullConvaihttp;
	/** Default headers - each request will include these headers, using the default value if not overridden */
	TMap<FString, FString> DefaultHeaders;
	/** singleton for the module while loaded and available */
	static FConvaihttpModule* Singleton;
	/** The address to use for proxy, in format IPADDRESS:PORT */
	FString ProxyAddress;
	/** Whether or not the convaihttp implementation we are using supports dynamic proxy setting. */
	bool bSupportsDynamicProxy;
	/** List of domains that can be accessed. If Empty then no filtering is applied */
	TArray<FString> AllowedDomains;
};
