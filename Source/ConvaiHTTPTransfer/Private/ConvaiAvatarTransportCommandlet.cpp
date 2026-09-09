// Copyright Convai Inc. All Rights Reserved.
#include "ConvaiAvatarTransportCommandlet.h"
#include "Convaihttp.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Misc/ScopeLock.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Windows/WindowsHWrapper.h"

namespace
{
constexpr uint64 Limit = 10485760000ULL;
constexpr double TransferTimeout = 21600.0;

bool SaveJson(const TSharedRef<FJsonObject>& Json, const FString& Path)
{
    FString Text;
    const FString Temp = Path + TEXT(".tmp");
    return FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)) &&
        FFileHelper::SaveStringToFile(Text, *Temp, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM) &&
        IFileManager::Get().Move(*Path, *Temp, true, false, false, true);
}
bool SafeLocalPath(const FString& Path)
{
    if (Path.IsEmpty() || FPaths::IsRelative(Path) || Path.Contains(TEXT("\"")) || Path.StartsWith(TEXT("\\\\"))) return false;
    for (const TCHAR C : Path) if (C < 32) return false;
    FString Current = FPaths::ConvertRelativePathToFull(Path);
    FPaths::NormalizeFilename(Current);
    for (;;)
    {
        const DWORD Attributes = GetFileAttributesW(*Current);
        if (Attributes != INVALID_FILE_ATTRIBUTES && (Attributes & FILE_ATTRIBUTE_REPARSE_POINT)) return false;
        const FString Parent = FPaths::GetPath(Current);
        if (Parent.IsEmpty() || Parent == Current) break;
        Current = Parent;
    }
    return true;
}
bool ReadPrivateRequest(const FString& Path, FString& Text)
{
    // The parent keeps a noninherited DELETE_ON_CLOSE handle. Explicit sharing
    // permits reading that protected handle without relaxing its write lock.
    HANDLE File = CreateFileW(*Path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (File == INVALID_HANDLE_VALUE) return false;
    ON_SCOPE_EXIT { CloseHandle(File); };
    LARGE_INTEGER Size{};
    if (!GetFileSizeEx(File, &Size) || Size.QuadPart <= 0 || Size.QuadPart > 64 * 1024) return false;
    TArray<uint8> Bytes;
    Bytes.SetNumZeroed(static_cast<int32>(Size.QuadPart) + 1);
    DWORD Read = 0;
    if (!ReadFile(File, Bytes.GetData(), static_cast<DWORD>(Size.QuadPart), &Read, nullptr) || Read != Size.QuadPart) return false;
    Text = UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Bytes.GetData()));
    return true;
}
bool AllowedUrl(const FString& Url, bool bTest)
{
    if (Url.Contains(TEXT("\r")) || Url.Contains(TEXT("\n")) || Url.Contains(TEXT("@"))) return false;
    if (Url.StartsWith(TEXT("https://storage.googleapis.com/user-assets-storage/"), ESearchCase::IgnoreCase) ||
        Url.StartsWith(TEXT("https://user-assets-storage.storage.googleapis.com/"), ESearchCase::IgnoreCase)) return true;
    // Direct commandlet fixture only. The host has no option that enables this.
    if (!bTest) return false;
    const FString HttpPrefix(TEXT("http://127.0.0.1:"));
    const FString HttpsPrefix(TEXT("https://127.0.0.1:"));
    const int32 PrefixLength = Url.StartsWith(HttpPrefix) ? HttpPrefix.Len() : (Url.StartsWith(HttpsPrefix) ? HttpsPrefix.Len() : 0);
    if (PrefixLength == 0) return false;
    FString Port = Url.Mid(PrefixLength);
    int32 Slash = INDEX_NONE;
    if (!Port.FindChar(TEXT('/'), Slash)) return false;
    Port.LeftInline(Slash);
    return !Port.IsEmpty() && Port.IsNumeric() && FCString::Atoi(*Port) > 0 && FCString::Atoi(*Port) <= 65535;
}

/** Upload hash covers the bytes read by CONVAIHTTP, including a clean rewind. */
class FUploadStream final : public FArchive
{
public:
    explicit FUploadStream(TUniquePtr<FArchive> InFile) : File(MoveTemp(InFile)) { SetIsLoading(true); SetIsPersistent(true); }
    virtual int64 TotalSize() override { return File->TotalSize(); }
    virtual int64 Tell() override { return File->Tell(); }
    virtual void Seek(int64 Position) override
    {
        if (Position != Tell())
        {
            if (Position != 0) { SetError(); return; }
            Hash = FMD5();
            Bytes = 0;
            File->Seek(0);
        }
    }
    virtual void Serialize(void* Data, int64 Count) override
    {
        if (Count < 0 || Count > MAX_int32 || Bytes + static_cast<uint64>(Count) > Limit) { SetError(); return; }
        File->Serialize(Data, Count);
        if (File->IsError()) { SetError(); return; }
        Hash.Update(static_cast<const uint8*>(Data), static_cast<uint32>(Count));
        Bytes += static_cast<uint64>(Count);
    }
    FString FinishHash() { FMD5Hash Result; Result.Set(Hash); return LexToString(Result); }
    uint64 Bytes = 0;
private:
    TUniquePtr<FArchive> File;
    FMD5 Hash;
};

/** Response bytes never accumulate in a TArray, even for a failed storage response. */
class FResponseStream final : public FArchive
{
public:
    FResponseStream(TUniquePtr<FArchive> InFile, uint64 InLimit) : File(MoveTemp(InFile)), MaxBytes(InLimit)
    { SetIsSaving(true); SetIsPersistent(true); }
    virtual void Serialize(void* Data, int64 Count) override
    {
        FScopeLock Guard(&Mutex);
        if (Count < 0 || static_cast<uint64>(Count) > MaxBytes - Bytes || Count > MAX_int32) { SetError(); return; }
        if (File)
        {
            File->Serialize(Data, Count);
            if (File->IsError()) { SetError(); return; }
        }
        Hash.Update(static_cast<const uint8*>(Data), static_cast<uint32>(Count));
        Bytes += static_cast<uint64>(Count);
    }
    uint64 GetBytes() { FScopeLock Guard(&Mutex); return Bytes; }
    FString FinishHash() { FScopeLock Guard(&Mutex); FMD5Hash Result; Result.Set(Hash); return LexToString(Result); }
    virtual bool Close() override
    {
        FScopeLock Guard(&Mutex);
        if (File && !File->Close()) SetError();
        File.Reset();
        return !IsError();
    }
private:
    FCriticalSection Mutex;
    TUniquePtr<FArchive> File;
    FMD5 Hash;
    uint64 Bytes = 0;
    uint64 MaxBytes;
};
}

UConvaiAvatarTransportCommandlet::UConvaiAvatarTransportCommandlet()
{
    IsClient = false;
    IsServer = false;
    IsEditor = false;
    LogToConsole = false;
    ShowErrorCount = false;
}

int32 UConvaiAvatarTransportCommandlet::Main(const FString& Params)
{
    // The upstream HTTP debug/error category logs complete URLs and headers.
    LogConvaihttp.SetVerbosity(ELogVerbosity::NoLogging);
    FString RequestPath;
    if (!FParse::Value(*Params, TEXT("AvatarTransferRequest="), RequestPath) || !SafeLocalPath(RequestPath)) return 1;
    const FString ResultPath = RequestPath + TEXT(".result.json");
    const FString ProgressPath = RequestPath + TEXT(".progress.json");
    if (!SafeLocalPath(ResultPath) || !SafeLocalPath(ProgressPath) || IFileManager::Get().FileSize(*RequestPath) > 64 * 1024) return 1;
    FString Error;
    int32 Code = 0;
    uint64 Bytes = 0;
    FString Hash;
    ON_SCOPE_EXIT
    {
        TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetNumberField(TEXT("schema_version"), 1);
        Result->SetBoolField(TEXT("success"), Error.IsEmpty());
        Result->SetStringField(TEXT("error"), Error);
        Result->SetNumberField(TEXT("http_status"), Code);
        Result->SetStringField(TEXT("bytes"), LexToString(Bytes));
        Result->SetStringField(TEXT("md5"), Hash);
        Result->SetStringField(TEXT("transport"), TEXT("CONVAIHTTP-transfer-v3"));
        SaveJson(Result, ResultPath);
    };
    FString Text;
    TSharedPtr<FJsonObject> Job;
    FString Method, Url, FilePath;
    if (!ReadPrivateRequest(RequestPath, Text) ||
        !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Job) || !Job ||
        !Job->TryGetStringField(TEXT("method"), Method) || !Job->TryGetStringField(TEXT("url"), Url) ||
        !Job->TryGetStringField(TEXT("file"), FilePath) || (Method != TEXT("GET") && Method != TEXT("PUT")))
    { Error = TEXT("The transfer request is invalid."); return 1; }
    Text.Reset();
    if (!AllowedUrl(Url, FParse::Param(*Params, TEXT("AvatarTransportTestLoopback"))) || !SafeLocalPath(FilePath))
    { Error = TEXT("The transfer destination is not allowed."); return 1; }
    const bool bUpload = Method == TEXT("PUT");
    const FString Part = RequestPath + TEXT(".body.part");
    if (!SafeLocalPath(Part) || IFileManager::Get().FileExists(*Part) || (!bUpload && IFileManager::Get().FileExists(*FilePath)))
    { Error = TEXT("The transfer destination already exists. Retry with a new download file."); return 1; }
    ON_SCOPE_EXIT { IFileManager::Get().Delete(*Part, false, true); };
    TSharedPtr<FUploadStream, ESPMode::ThreadSafe> Upload;
    uint64 ExpectedSize = 0;
    if (bUpload)
    {
        TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FilePath, FILEREAD_Silent));
        if (!Reader || Reader->TotalSize() <= 0 || static_cast<uint64>(Reader->TotalSize()) > Limit)
        { Error = TEXT("Select a readable, nonempty upload file no larger than 10,485,760,000 bytes."); return 1; }
        ExpectedSize = static_cast<uint64>(Reader->TotalSize());
        Upload = MakeShared<FUploadStream, ESPMode::ThreadSafe>(MoveTemp(Reader));
    }
    TUniquePtr<FArchive> Writer;
    if (!bUpload)
    {
        Writer.Reset(IFileManager::Get().CreateFileWriter(*Part, FILEWRITE_NoReplaceExisting | FILEWRITE_Silent));
        if (!Writer) { Error = TEXT("Could not create the temporary download file."); return 1; }
    }
    // Upload responses should be empty/small; do not permit unbounded error payloads.
    TSharedRef<FResponseStream> Sink = MakeShared<FResponseStream>(MoveTemp(Writer), bUpload ? 1024 * 1024ULL : Limit);
    TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> Request = FConvaihttpModule::Get().CreateRequest();
    FConvaihttpTransportSecurityOptions Security;
    Security.bRequireVerifiedTls = true;
    Security.bFollowRedirects = false;
    if (!Request->SetTransportSecurityOptions(Security))
    { Error = TEXT("This HTTP backend does not support the required secure transfer policy."); return 1; }
    Request->SetURL(Url);
    Url.Reset();
    Job.Reset();
    Request->SetVerb(Method);
    Request->SetTimeout(static_cast<float>(TransferTimeout));
    Request->SetHeader(TEXT("Accept-Encoding"), TEXT("identity"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/octet-stream"));
    if (bUpload)
    {
        Request->SetHeader(TEXT("Access-Control-Allow-Origin"), TEXT("*"));
        Request->SetHeader(TEXT("x-goog-content-length-range"), TEXT("0,10485760000"));
        if (!Request->SetContentFromStream(Upload.ToSharedRef()))
        { Error = TEXT("The upload stream could not be opened."); return 1; }
    }
    if (!Request->SetResponseBodyReceiveStream(Sink))
    { Error = TEXT("The download stream could not be opened."); return 1; }
    bool bDone = false;
    bool bSucceeded = false;
    uint64 DeclaredSize = 0;
    bool bHasDeclaredSize = false;
    uint64 Sent = 0, Received = 0;
    Request->OnHeaderReceived().BindLambda([&](FConvaihttpRequestPtr, const FString& Name, const FString& Value)
    {
        if (Name.Equals(TEXT("Content-Length"), ESearchCase::IgnoreCase))
        {
            bHasDeclaredSize = true;
            if (Value.IsEmpty() || !Value.IsNumeric() || !LexTryParseString(DeclaredSize, *Value) || DeclaredSize > (bUpload ? 1024 * 1024ULL : Limit))
            { Error = TEXT("The storage response exceeds the transfer limit or has an invalid size."); Request->CancelRequest(); }
        }
    });
    Request->OnRequestProgress().BindLambda([&](FConvaihttpRequestPtr, uint64 InSent, uint64 InReceived)
    { Sent = InSent; Received = InReceived; });
    Request->OnProcessRequestComplete().BindLambda([&](FConvaihttpRequestPtr, FConvaihttpResponsePtr Response, bool bConnected)
    {
        Code = Response ? Response->GetResponseCode() : 0;
        bSucceeded = bConnected && Response && Code >= 200 && Code < 300;
        bDone = true;
    });
    if (!Request->ProcessRequest()) { Error = TEXT("Could not start the file transfer."); return 1; }
    const double Started = FPlatformTime::Seconds();
    double Previous = Started, LastProgress = 0;
    while (!bDone)
    {
        const double Now = FPlatformTime::Seconds();
        FTSTicker::GetCoreTicker().Tick(static_cast<float>(Now - Previous));
        FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
        Previous = Now;
        if (Now - LastProgress >= 0.2)
        {
            TSharedRef<FJsonObject> Progress = MakeShared<FJsonObject>();
            Progress->SetStringField(TEXT("bytes"), LexToString(bUpload ? Sent : Received));
            Progress->SetStringField(TEXT("total"), LexToString(bUpload ? ExpectedSize : DeclaredSize));
            SaveJson(Progress, ProgressPath);
            LastProgress = Now;
        }
        if (Now - Started > TransferTimeout + 30)
        { Error = TEXT("The file transfer timed out. Refresh the link and try again."); Request->CancelRequest(); break; }
        FPlatformProcess::Sleep(0.01f);
    }
    Request->OnHeaderReceived().Unbind();
    Request->OnRequestProgress().Unbind();
    Request->OnProcessRequestComplete().Unbind();
    // Normal completion joins the HTTP body callback before invoking its delegate.
    if (!bDone) { Request->CancelRequest(); return 1; }
    if (!Sink->Close() && Error.IsEmpty()) Error = TEXT("Could not finish writing the downloaded file.");
    Bytes = bUpload ? Upload->Bytes : Sink->GetBytes();
    if (Error.IsEmpty() && (!bSucceeded || Sink->IsError() || (Upload && Upload->IsError())))
        Error = Code ? FString::Printf(TEXT("The file transfer failed (HTTP %d). Refresh the link before retrying."), Code) : TEXT("The file transfer did not complete. Check your connection and retry.");
    if (Error.IsEmpty() && (Bytes == 0 || (bUpload && Bytes != ExpectedSize) || (!bUpload && bHasDeclaredSize && Bytes != DeclaredSize)))
        Error = TEXT("The transferred file size does not match its expected size.");
    if (!Error.IsEmpty()) return 1;
    Hash = bUpload ? Upload->FinishHash() : Sink->FinishHash();
    if (!bUpload && !IFileManager::Get().Move(*FilePath, *Part, false, false, false, true))
    { Error = TEXT("Could not install the completed download. The destination may already exist."); return 1; }
    return 0;
}
