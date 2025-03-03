// Copyright Epic Games, Inc. All Rights Reserved.

#if WITH_WINHTTP

#include "WinHttp/WinHttpConvaihttpResponse.h"
#include "Convaihttp.h"

FWinHttpConvaihttpResponse::FWinHttpConvaihttpResponse(const FString& InUrl, const EConvaihttpResponseCodes::Type InConvaihttpStatusCode, TMap<FString, FString>&& InHeaders, TArray64<uint8>&& InPayload)
	: Url(InUrl)
	, ConvaihttpStatusCode(InConvaihttpStatusCode)
	, Headers(MoveTemp(InHeaders))
	, Payload(MoveTemp(InPayload))
{
}

FString FWinHttpConvaihttpResponse::GetURL() const
{
	return Url;
}

FString FWinHttpConvaihttpResponse::GetURLParameter(const FString& ParameterName) const
{
	FString ReturnValue;
	if (TOptional<FString> OptionalParameterValue = FGenericPlatformConvaihttp::GetUrlParameter(Url, ParameterName))
	{
		ReturnValue = MoveTemp(OptionalParameterValue.GetValue());
	}
	return ReturnValue;
}

FString FWinHttpConvaihttpResponse::GetHeader(const FString& HeaderName) const
{
	FString Result;
	if (const FString* Header = Headers.Find(HeaderName))
	{
		Result = *Header;
	}
	return Result;
}

TArray64<FString> FWinHttpConvaihttpResponse::GetAllHeaders() const
{
	TArray64<FString> Result;
	Result.Reserve(Headers.Num());
	for (const TPair<FString, FString>& It : Headers)
	{
		Result.Emplace(FString::Printf(TEXT("%s: %s"), *It.Key, *It.Value));
	}
	return Result;
}

FString FWinHttpConvaihttpResponse::GetContentType() const
{
	return GetHeader(TEXT("Content-Type"));
}

int64 FWinHttpConvaihttpResponse::GetContentLength() const
{
	return Payload.Num();
}

const TArray64<uint8>& FWinHttpConvaihttpResponse::GetContent() const
{
	return Payload;
}

int32 FWinHttpConvaihttpResponse::GetResponseCode() const
{
	return ConvaihttpStatusCode;
}

FString FWinHttpConvaihttpResponse::GetContentAsString() const
{
	// Content is NOT null-terminated; we need to specify lengths here
	FUTF8ToTCHAR TCHARData(reinterpret_cast<const ANSICHAR*>(Payload.GetData()), Payload.Num());
	return FString(TCHARData.Length(), TCHARData.Get());
}

#endif // WITH_WINHTTP
