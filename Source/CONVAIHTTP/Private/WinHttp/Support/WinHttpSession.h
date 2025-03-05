// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"
#include "WinHttp/Support/WinHttpHandle.h"

class IWinHttpConnection;

using HINTERNET = void*;

class CONVAIHTTP_API FCH_WinHttpSession
{
public:
	/**
	 * Construct a new WinHttp session with the specified security protocols flags
	 */
	FCH_WinHttpSession(const uint32 SecurityProtocolFlags, const bool bForceSecureConnections);

	/**
	 * FCH_WinHttpSession is move-only
	 */
	FCH_WinHttpSession(const FCH_WinHttpSession& Other) = delete;
	FCH_WinHttpSession(FCH_WinHttpSession&& Other) = default;
	FCH_WinHttpSession& operator=(const FCH_WinHttpSession& Other) = delete;
	FCH_WinHttpSession& operator=(FCH_WinHttpSession&& Other) = default;

	/**
	 * Did this session initialize successfully?
	 */
	bool IsValid() const;

	/**
	 * Get the underlying session handle
	 *
	 * @return the HINTERNET for this session
	 */
	HINTERNET Get() const;

	/**
	 * Are we only allowed to make secure connection requests (CONVAIHTTPS, etc)
	 *
	 * @return True if the platform does not allow for insecure messages
	 */
	bool AreOnlySecureConnectionsAllowed() const;

private:
	/** The handle for our session that we are wrapping */
	FCH_WinHttpHandle SessionHandle;
	/** Should we force connections to be secure? */
	bool bForceSecureConnections = false;
};

#endif // WITH_WINHTTP
