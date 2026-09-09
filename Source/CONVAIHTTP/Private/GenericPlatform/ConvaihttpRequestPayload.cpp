// Copyright Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/ConvaihttpRequestPayload.h"
#include "GenericPlatform/GenericPlatformFile.h"
#include "GenericPlatform/GenericPlatformConvaihttp.h"
#include "HAL/PlatformFileManager.h"

bool FGenericPlatformConvaihttp::CH_IsURLEncoded(const TArray64<uint8>& Payload)
{
	static char AllowedChars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_.~";
	static bool bTableFilled = false;
	static bool AllowedTable[256] = { false };

	if (!bTableFilled)
	{
		for (int32 Idx = 0; Idx < UE_ARRAY_COUNT(AllowedChars) - 1; ++Idx)	// -1 to avoid trailing 0
		{
			uint8 AllowedCharIdx = static_cast<uint8>(AllowedChars[Idx]);
			check(AllowedCharIdx < UE_ARRAY_COUNT(AllowedTable));
			AllowedTable[AllowedCharIdx] = true;
		}

		bTableFilled = true;
	}

	const int32 Num = Payload.Num();
	for (int32 Idx = 0; Idx < Num; ++Idx)
	{
		if (!AllowedTable[Payload[Idx]])
			return false;
	}

	return true;
}

FCH_RequestPayloadInFileStream::FCH_RequestPayloadInFileStream(TSharedRef<FArchive, ESPMode::ThreadSafe> InFile) : File(InFile)
{
}

FCH_RequestPayloadInFileStream::~FCH_RequestPayloadInFileStream()
{
}

uint64 FCH_RequestPayloadInFileStream::GetContentLength() const
{
	const int64 Length = File->TotalSize();
	return Length >= 0 ? static_cast<uint64>(Length) : 0;
}

const TArray64<uint8>& FCH_RequestPayloadInFileStream::GetContent() const
{
	ensureMsgf(false, TEXT("GetContent() on a streaming request payload is not allowed"));
	static const TArray64<uint8> NotSupported;
	return NotSupported;
}

bool FCH_RequestPayloadInFileStream::CH_IsURLEncoded() const
{
	// Assume that files are not URL encoded, because they probably aren't.
	// This implies that POST requests with streamed files will need the caller to set a Content-Type.
	return false;
}

size_t FCH_RequestPayloadInFileStream::FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent)
{
	// Write into the caller's buffer, without allocating or copying a temporary array.
	const uint64 Length = GetContentLength();
	if (File->IsError() || static_cast<uint64>(SizeAlreadySent) > Length) return 0;
	const uint64 Count = FMath::Min<uint64>(Length - static_cast<uint64>(SizeAlreadySent), static_cast<uint64>(MaxOutputBufferSize));
	if (Count == 0) return 0;
	if (File->Tell() != static_cast<int64>(SizeAlreadySent)) File->Seek(static_cast<int64>(SizeAlreadySent));
	if (File->IsError()) return 0;
	File->Serialize(OutputBuffer, static_cast<int64>(Count));
	return File->IsError() ? 0 : static_cast<size_t>(Count);
}

size_t FCH_RequestPayloadInFileStream::FillOutputBuffer(TArray64<uint8> OutputBuffer, size_t SizeAlreadySent)
{
	const size_t ContentLength = static_cast<size_t>(GetContentLength());
	check(SizeAlreadySent <= ContentLength);
	const size_t SizeToSend = ContentLength - SizeAlreadySent;
	const size_t SizeToSendThisTime = FMath::Min(SizeToSend, static_cast<size_t>(OutputBuffer.Num()));
	if (SizeToSendThisTime != 0)
	{
		if (File->Tell() != SizeAlreadySent)
		{
			File->Seek(SizeAlreadySent);
		}
		File->Serialize(OutputBuffer.GetData(), static_cast<int64>(SizeToSendThisTime));
	}
	return SizeToSendThisTime;
}

FCH_RequestPayloadInMemory::FCH_RequestPayloadInMemory(const TArray64<uint8>& Array) : Buffer(Array)
{
}

FCH_RequestPayloadInMemory::FCH_RequestPayloadInMemory(TArray64<uint8>&& Array) : Buffer(MoveTemp(Array))
{
}

FCH_RequestPayloadInMemory::~FCH_RequestPayloadInMemory()
{
}

uint64 FCH_RequestPayloadInMemory::GetContentLength() const
{
	return Buffer.Num();
}

const TArray64<uint8>& FCH_RequestPayloadInMemory::GetContent() const
{
	return Buffer;
}

bool FCH_RequestPayloadInMemory::CH_IsURLEncoded() const
{
	return FGenericPlatformConvaihttp::CH_IsURLEncoded(Buffer);
}

size_t FCH_RequestPayloadInMemory::FillOutputBuffer(void* OutputBuffer, size_t MaxOutputBufferSize, size_t SizeAlreadySent)
{
	const size_t ContentLength = static_cast<size_t>(Buffer.Num());
	check(SizeAlreadySent <= ContentLength);
	const size_t SizeToSend = ContentLength - SizeAlreadySent;
	const size_t SizeToSendThisTime = FMath::Min(SizeToSend, MaxOutputBufferSize);

	if (SizeToSendThisTime != 0)
	{
		FMemory::Memcpy(OutputBuffer, Buffer.GetData() + SizeAlreadySent, SizeToSendThisTime);
	}
	return SizeToSendThisTime;
	//return FillOutputBuffer(TArray64<uint8>(static_cast<uint8*>(OutputBuffer), MaxOutputBufferSize), SizeAlreadySent);
}

size_t FCH_RequestPayloadInMemory::FillOutputBuffer(TArray64<uint8> OutputBuffer, size_t SizeAlreadySent)
{
	const size_t ContentLength = static_cast<size_t>(Buffer.Num());
	check(SizeAlreadySent <= ContentLength);
	const size_t SizeToSend = ContentLength - SizeAlreadySent;
	const size_t SizeToSendThisTime = FMath::Min(SizeToSend, static_cast<size_t>(OutputBuffer.Num()));
	if (SizeToSendThisTime != 0)
	{
		FMemory::Memcpy(OutputBuffer.GetData(), Buffer.GetData() + SizeAlreadySent, SizeToSendThisTime);
	}
	return SizeToSendThisTime;
}
