// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Interfaces/IConvaihttpRequest.h"

/**
 * Contains implementation of some common functions that don't vary between implementation
 */
class CONVAIHTTP_API FConvaihttpRequestImpl : public IConvaihttpRequest
{
public:
	// IConvaihttpRequest
	virtual FConvaihttpRequestCompleteDelegate& OnProcessRequestComplete() override;
	virtual FConvaihttpRequestProgressDelegate& OnRequestProgress() override;
	virtual FConvaihttpRequestHeaderReceivedDelegate& OnHeaderReceived() override;
	virtual FConvaihttpRequestWillRetryDelegate& OnRequestWillRetry() override;

	virtual void SetTimeout(float InTimeoutSecs) override;
	virtual void ClearTimeout() override;
	virtual TOptional<float> GetTimeout() const override;

	float GetTimeoutOrDefault() const;

protected:
	/** 
	 * Broadcast all of our response's headers as having been received
	 * Used when we don't know when we receive headers in our CONVAIHTTP implementation
	 */
	void BroadcastResponseHeadersReceived();

protected:
	/** Delegate that will get called once request completes or on any error */
	FConvaihttpRequestCompleteDelegate RequestCompleteDelegate;

	/** Delegate that will get called once per tick with bytes downloaded so far */
	FConvaihttpRequestProgressDelegate RequestProgressDelegate;

	/** Delegate that will get called for each new header received */
	FConvaihttpRequestHeaderReceivedDelegate HeaderReceivedDelegate;
	
	/** Delegate that will get called when request will be retried */
	FConvaihttpRequestWillRetryDelegate OnRequestWillRetryDelegate;

	/** Timeout in seconds for the entire CONVAIHTTP request to complete */
	TOptional<float> TimeoutSecs;
};
