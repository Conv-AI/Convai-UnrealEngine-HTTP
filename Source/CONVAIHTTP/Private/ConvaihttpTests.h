// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interfaces/IConvaihttpRequest.h"

/**
 * Test an Convaihttp request to a specified endpoint Url
 */
class FConvaihttpTest
{
public:

	/**
	 * Constructor
	 *
	 * @param Verb - verb to use for request (GET,POST,DELETE,etc)
	 * @param Payload - optional payload string
	 * @param Url - url address to connect to
	 * @param InIterations - total test iterations to run
	 */
	FConvaihttpTest(const FString& InVerb, const FString& InPayload, const FString& InUrl, int32 InIterations);

	/**
	 * Kick off the Convaihttp request for the test and wait for delegate to be called
	 */
	void Run(void);

	/**
	 * Delegate called when the request completes
	 *
	 * @param ConvaihttpRequest - object that started/processed the request
	 * @param ConvaihttpResponse - optional response object if request completed
	 * @param bSucceeded - true if Url connection was made and response was received
	 */
	void RequestComplete(FConvaihttpRequestPtr ConvaihttpRequest, FConvaihttpResponsePtr ConvaihttpResponse, bool bSucceeded);

private:
	FString Verb;
	FString Payload;
	FString Url;
	int32 TestsToRun;
};

