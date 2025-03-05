// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"

class CONVAIHTTP_API FCH_WinHttpErrorHelper
{
public:
	/**
	 * Log that an WinHttpOpen call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpOpenFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpCloseHandle call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpCloseHandleFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSetTimeouts call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpSetTimeoutsFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpConnect call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpConnectFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpOpenRequest call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpOpenRequestFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSetOption call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpSetOptionFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpAddRequestHeaders call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpAddRequestHeadersFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSetStatusCallback call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpSetStatusCallbackFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpSendRequest call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpSendRequestFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpReceiveResponse call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpReceiveResponseFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpQueryHeaders call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpQueryHeadersFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpQueryDataAvailable call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpQueryDataAvailableFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpReadData call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpReadDataFailure(const uint32 ErrorCode);

	/**
	 * Log that an WinHttpWriteData call failed with the specified ErrorCode
	 *
	 * @param ErrorCode The error that occured
	 */
	static void LogWinConvaiHttpWriteDataFailure(const uint32 ErrorCode);

private:
	/** This class should not be instantiated */
	FCH_WinHttpErrorHelper() = delete;
};


#endif // WITH_WINHTTP
