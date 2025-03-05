// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if WITH_WINHTTP

#include "CoreMinimal.h"
#include "Interfaces/IConvaihttpResponse.h"
#include "Containers/Queue.h"

/**
 * WinHttp implementation of an CONVAIHTTP response
 */
class FCH_WinHttpConvaihttpResponse : public IConvaihttpResponse
{
public:
	FCH_WinHttpConvaihttpResponse(const FString& InUrl, const EConvaihttpResponseCodes::Type InConvaihttpStatusCode, TMap<FString, FString>&& InHeaders, TArray64<uint8>&& InPayload);
	virtual ~FCH_WinHttpConvaihttpResponse() = default;

	//~ Begin IConvaihttpBase Interface
	virtual FString GetURL() const override;
	virtual FString GetURLParameter(const FString& ParameterName) const override;
	virtual FString GetHeader(const FString& HeaderName) const override;
	virtual TArray64<FString> GetAllHeaders() const override;	
	virtual FString GetContentType() const override;
	virtual uint64 GetContentLength() const override;
	virtual const TArray64<uint8>& GetContent() const override;
	//~ End IConvaihttpBase Interface

	//~ Begin IConvaihttpResponse Interface
	virtual int32 GetResponseCode() const override;
	virtual FString GetContentAsString() const override;
	//~ End IConvaihttpResponse Interface

	void AppendHeader(const FString& HeaderKey, const FString& HeaderValue) { Headers.Add(HeaderKey, HeaderValue); }
	void AppendPayload(const TArray64<uint8>& InPayload) { Payload.Append(InPayload); }

protected:
	/** The URL we requested data from*/
	FString Url;
	/** Cached code from completed response */
	EConvaihttpResponseCodes::Type ConvaihttpStatusCode;
	/** Cached key/value header pairs. Parsed once request completes. Only accessible on the game thread. */
	TMap<FString, FString> Headers;
	/** Byte array of the data we received */
	TArray64<uint8> Payload;
};

#endif // WITH_WINHTTP
