// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"

using HINTERNET = void*;

class CONVAIHTTP_API FCH_WinHttpHandle
{
public:
	/**
	 * Construct an invalid object
	 */
	FCH_WinHttpHandle() = default;

	/**
	 * Wrap a new HINTERNET Handle
	 */
	explicit FCH_WinHttpHandle(HINTERNET NewHandle);

	/**
	 * Destroy any currently held object
	 */
	~FCH_WinHttpHandle();

	// Copy/Move constructors
	FCH_WinHttpHandle(const FCH_WinHttpHandle& Other) = delete;
	FCH_WinHttpHandle(FCH_WinHttpHandle&& Other);
	FCH_WinHttpHandle& operator=(const FCH_WinHttpHandle& Other) = delete;
	FCH_WinHttpHandle& operator=(FCH_WinHttpHandle&& Other);

	/**
	 * Wrap a new handle (destroying any previously held object)
	 *
	 * @param NewHandle The new handle to wrap (must not be wrapped by anything else)
	 * @return A reference to this object that now wraps NewHandle
	 */
	FCH_WinHttpHandle& operator=(HINTERNET NewHandle);

	/**
	 * Destroy our current handle and reset our state to holding nothing
	 */
	void Reset();

	/**
	 * Do we contain a valid handle?
	 *
	 * @return True if we have a valid handle, false otherwise
	 */
	explicit operator bool() const;

	/**
	 * Do we contain a valid handle?
	 *
	 * @return True if we have a valid handle, false otherwise
	 */
	bool IsValid() const;

	/**
	 * Get the underlying handle for use
	 *
	 * @return The HINTERNET handle we're wrapping
	 */
	HINTERNET Get() const;

protected:
	/**
	 * The handle we're wrap (if we have one)
	 */
	HINTERNET Handle = nullptr;
};

#endif // WITH_WINHTTP
