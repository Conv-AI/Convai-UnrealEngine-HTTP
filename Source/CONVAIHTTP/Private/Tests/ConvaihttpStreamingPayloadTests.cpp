// Copyright Convai Inc. All Rights Reserved.
#if WITH_DEV_AUTOMATION_TESTS
#include "GenericPlatform/ConvaihttpRequestPayload.h"
#include "Misc/AutomationTest.h"

namespace
{
/** A sparse synthetic stream: exercises offsets above 4 GiB without disk or large allocations. */
class FPatternArchive final : public FArchive
{
public:
    explicit FPatternArchive(int64 InSize) : Size(InSize) { SetIsLoading(true); }
    virtual int64 TotalSize() override { return Size; }
    virtual int64 Tell() override { return Position; }
    virtual void Seek(int64 Offset) override
    {
        if (bFailSeek || Offset < 0 || Offset > Size) { SetError(); return; }
        Position = Offset;
    }
    virtual void Serialize(void* Data, int64 Count) override
    {
        ++ReadCalls;
        if (Count < 0 || Count > 64 || Position + Count > Size) { SetError(); return; }
        for (int64 Index = 0; Index < Count; ++Index) static_cast<uint8*>(Data)[Index] = static_cast<uint8>((Position + Index) % 251);
        Position += Count;
    }
    int64 Size, Position = 0;
    int32 ReadCalls = 0;
    bool bFailSeek = false;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvaihttpLargeStreamingPayloadTest,
    "ConvaiHTTP.Streaming.DirectBufferAnd64BitOffsets", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConvaihttpLargeStreamingPayloadTest::RunTest(const FString&)
{
    if (sizeof(size_t) < 8) { AddInfo(TEXT("Offsets above 4 GiB require a 64-bit platform.")); return true; }
    constexpr int64 Length = 5LL * 1024 * 1024 * 1024 + 123;
    const auto File = MakeShared<FPatternArchive, ESPMode::ThreadSafe>(Length);
    FCH_RequestPayloadInFileStream Payload(File);
    TestEqual(TEXT("File length retains all 64 bits"), Payload.GetContentLength(), static_cast<uint64>(Length));
    uint8 Buffer[32]; FMemory::Memset(Buffer, 255, sizeof(Buffer));
    TestEqual(TEXT("Read directly into the caller's actual buffer"), Payload.FillOutputBuffer(Buffer, sizeof(Buffer), 0), sizeof(Buffer));
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buffer); ++Index)
        TestEqual(FString::Printf(TEXT("Caller byte %d changed without a copied TArray"), Index), Buffer[Index], static_cast<uint8>(Index));
    FMemory::Memset(Buffer, 255, sizeof(Buffer));
    const size_t Offset = static_cast<size_t>(Length - 7);
    TestEqual(TEXT("A high offset returns only the remaining bytes"), Payload.FillOutputBuffer(Buffer, sizeof(Buffer), Offset), static_cast<size_t>(7));
    for (int32 Index = 0; Index < 7; ++Index)
        TestEqual(FString::Printf(TEXT("High-offset byte %d"), Index), Buffer[Index], static_cast<uint8>((Length - 7 + Index) % 251));
    TestEqual(TEXT("Capacity beyond the tail stays untouched"), Buffer[7], static_cast<uint8>(255));
    const int32 Reads = File->ReadCalls;
    TestEqual(TEXT("Exact EOF returns zero"), Payload.FillOutputBuffer(Buffer, sizeof(Buffer), static_cast<size_t>(Length)), static_cast<size_t>(0));
    TestEqual(TEXT("Out-of-range offset returns zero"), Payload.FillOutputBuffer(Buffer, sizeof(Buffer), static_cast<size_t>(Length + 1)), static_cast<size_t>(0));
    TestEqual(TEXT("Zero capacity performs no read"), Payload.FillOutputBuffer(Buffer, 0, 0), static_cast<size_t>(0));
    TestEqual(TEXT("EOF and invalid reads do not touch the archive"), File->ReadCalls, Reads);
    TestEqual(TEXT("A clean rewind can resend the first bytes"), Payload.FillOutputBuffer(Buffer, sizeof(Buffer), 0), sizeof(Buffer));
    TestEqual(TEXT("Rewound content is correct"), Buffer[1], static_cast<uint8>(1));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FConvaihttpStreamingPayloadFailureTest,
    "ConvaiHTTP.Streaming.ArchiveFailures", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FConvaihttpStreamingPayloadFailureTest::RunTest(const FString&)
{
    uint8 Buffer[8]{};
    const auto Unknown = MakeShared<FPatternArchive, ESPMode::ThreadSafe>(-1);
    FCH_RequestPayloadInFileStream UnknownPayload(Unknown);
    TestEqual(TEXT("An unknown negative archive size does not become a huge upload"), UnknownPayload.GetContentLength(), static_cast<uint64>(0));
    const auto Failed = MakeShared<FPatternArchive, ESPMode::ThreadSafe>(64);
    Failed->SetError(); FCH_RequestPayloadInFileStream FailedPayload(Failed);
    TestEqual(TEXT("An errored archive supplies no bytes"), FailedPayload.FillOutputBuffer(Buffer, sizeof(Buffer), 0), static_cast<size_t>(0));
    TestEqual(TEXT("An errored archive is not read"), Failed->ReadCalls, 0);
    const auto SeekFailure = MakeShared<FPatternArchive, ESPMode::ThreadSafe>(64);
    SeekFailure->bFailSeek = true; FCH_RequestPayloadInFileStream SeekPayload(SeekFailure);
    TestEqual(TEXT("A failed seek supplies no bytes"), SeekPayload.FillOutputBuffer(Buffer, sizeof(Buffer), 16), static_cast<size_t>(0));
    TestEqual(TEXT("No read follows a failed seek"), SeekFailure->ReadCalls, 0);
    return true;
}
#endif
