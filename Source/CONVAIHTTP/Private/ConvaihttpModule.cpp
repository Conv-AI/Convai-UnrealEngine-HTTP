// Copyright Epic Games, Inc. All Rights Reserved.

#include "ConvaihttpModule.h"
#include "Misc/ConfigCacheIni.h"
#include "Modules/ModuleManager.h"
#include "ConvaihttpManager.h"
#include "Convaihttp.h"
#include "NullConvaihttp.h"
#include "ConvaihttpTests.h"
#include "Misc/CommandLine.h"
#include "Misc/CoreDelegates.h"

DEFINE_LOG_CATEGORY(LogConvaihttp);

// FConvaihttpModule

IMPLEMENT_MODULE(FConvaihttpModule, CONVAIHTTP);

FConvaihttpModule* FConvaihttpModule::Singleton = NULL;

static bool ShouldLaunchUrl(const TCHAR* Url)
{
	FString SchemeName;
	if (FParse::SchemeNameFromURI(Url, SchemeName) && (SchemeName == TEXT("convaihttp") || SchemeName == TEXT("convaihttps")))
	{
		FConvaihttpManager& ConvaihttpManager = FConvaihttpModule::Get().GetConvaihttpManager();
		return ConvaihttpManager.IsDomainAllowed(Url);
	}

	return true;
}

void FConvaihttpModule::UpdateConfigs()
{
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpTimeout"), ConvaihttpTimeout, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpConnectionTimeout"), ConvaihttpConnectionTimeout, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpReceiveTimeout"), ConvaihttpReceiveTimeout, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpSendTimeout"), ConvaihttpSendTimeout, GEngineIni);
	GConfig->GetInt(TEXT("CONVAIHTTP"), TEXT("ConvaihttpMaxConnectionsPerServer"), ConvaihttpMaxConnectionsPerServer, GEngineIni);
	GConfig->GetBool(TEXT("CONVAIHTTP"), TEXT("bEnableConvaihttp"), bEnableConvaihttp, GEngineIni);
	GConfig->GetBool(TEXT("CONVAIHTTP"), TEXT("bUseNullConvaihttp"), bUseNullConvaihttp, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpDelayTime"), ConvaihttpDelayTime, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpThreadActiveFrameTimeInSeconds"), ConvaihttpThreadActiveFrameTimeInSeconds, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpThreadActiveMinimumSleepTimeInSeconds"), ConvaihttpThreadActiveMinimumSleepTimeInSeconds, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpThreadIdleFrameTimeInSeconds"), ConvaihttpThreadIdleFrameTimeInSeconds, GEngineIni);
	GConfig->GetFloat(TEXT("CONVAIHTTP"), TEXT("ConvaihttpThreadIdleMinimumSleepTimeInSeconds"), ConvaihttpThreadIdleMinimumSleepTimeInSeconds, GEngineIni);

	AllowedDomains.Empty();
	GConfig->GetArray(TEXT("CONVAIHTTP"), TEXT("AllowedDomains"), AllowedDomains, GEngineIni);

	if (ConvaihttpManager != nullptr)
	{
		ConvaihttpManager->UpdateConfigs();
	}
}

void FConvaihttpModule::StartupModule()
{	
	Singleton = this;

	MaxReadBufferSize = 256 * 1024;
	ConvaihttpTimeout = 300.0f;
	ConvaihttpConnectionTimeout = -1;
	ConvaihttpReceiveTimeout = ConvaihttpConnectionTimeout;
	ConvaihttpSendTimeout = ConvaihttpConnectionTimeout;
	ConvaihttpMaxConnectionsPerServer = 16;
	bEnableConvaihttp = true;
	bUseNullConvaihttp = false;
	ConvaihttpDelayTime = 0;
	ConvaihttpThreadActiveFrameTimeInSeconds = 1.0f / 200.0f; // 200Hz
	ConvaihttpThreadActiveMinimumSleepTimeInSeconds = 0.0f;
	ConvaihttpThreadIdleFrameTimeInSeconds = 1.0f / 30.0f; // 30Hz
	ConvaihttpThreadIdleMinimumSleepTimeInSeconds = 0.0f;	

	// override the above defaults from configs
	UpdateConfigs();

	if (!FParse::Value(FCommandLine::Get(), TEXT("convaihttpproxy="), ProxyAddress))
	{
		if (!GConfig->GetString(TEXT("CONVAIHTTP"), TEXT("ConvaihttpProxyAddress"), ProxyAddress, GEngineIni))
		{
			if (TOptional<FString> OperatingSystemProxyAddress = FPlatformConvaihttp::GetOperatingSystemProxyAddress())
			{
				ProxyAddress = MoveTemp(OperatingSystemProxyAddress.GetValue());
			}
		}
	}

	// Initialize FPlatformConvaihttp after we have read config values
	FPlatformConvaihttp::Init();

	ConvaihttpManager = FPlatformConvaihttp::CreatePlatformConvaihttpManager();
	if (nullptr == ConvaihttpManager)
	{
		// platform does not provide specific CONVAIHTTP manager, use generic one
		ConvaihttpManager = new FConvaihttpManager();
	}
	ConvaihttpManager->Initialize();

	bSupportsDynamicProxy = ConvaihttpManager->SupportsDynamicProxy();

	FCoreDelegates::ShouldLaunchUrl.BindStatic(ShouldLaunchUrl);
}

void FConvaihttpModule::PostLoadCallback()
{

}

void FConvaihttpModule::PreUnloadCallback()
{
}

void FConvaihttpModule::ShutdownModule()
{
	FCoreDelegates::ShouldLaunchUrl.Unbind();

	if (ConvaihttpManager != nullptr)
	{
		// block on any convaihttp requests that have already been queued up
		ConvaihttpManager->Flush(EConvaihttpFlushReason::Shutdown);
	}

	// at least on Linux, the code in CONVAIHTTP manager (e.g. request destructors) expects platform to be initialized yet
	delete ConvaihttpManager;	// can be passed NULLs

	FPlatformConvaihttp::Shutdown();

	ConvaihttpManager = nullptr;
	Singleton = nullptr;
}

bool FConvaihttpModule::HandleCONVAIHTTPCommand(const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (FParse::Command(&Cmd, TEXT("TEST")))
	{
		int32 Iterations=1;
		FString IterationsStr;
		FParse::Token(Cmd, IterationsStr, true);
		if (!IterationsStr.IsEmpty())
		{
			Iterations = FCString::Atoi(*IterationsStr);
		}		
		FString Url;
		FParse::Token(Cmd, Url, true);
		if (Url.IsEmpty())
		{
			Url = TEXT("convaihttp://www.google.com");
		}		
		FConvaihttpTest* ConvaihttpTest = new FConvaihttpTest(TEXT("GET"),TEXT(""),Url,Iterations);
		ConvaihttpTest->Run();
	}
	else if (FParse::Command(&Cmd, TEXT("DUMPREQ")))
	{
		GetConvaihttpManager().DumpRequests(Ar);
	}
	else if (FParse::Command(&Cmd, TEXT("FLUSH")))
	{
		GetConvaihttpManager().Flush(EConvaihttpFlushReason::Default);
	}
#if !UE_BUILD_SHIPPING
	else if (FParse::Command(&Cmd, TEXT("FILEUPLOAD")))
	{
		FString UploadUrl, UploadFilename;
		bool bIsCmdOk = FParse::Token(Cmd, UploadUrl, false);
		bIsCmdOk &= FParse::Token(Cmd, UploadFilename, false);
		if (bIsCmdOk)
		{
			FString ConvaihttpMethod;
			if (!FParse::Token(Cmd, ConvaihttpMethod, false))
			{
				ConvaihttpMethod = TEXT("PUT");
			}

			TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> Request = CreateRequest();
			Request->SetURL(UploadUrl);
			Request->SetVerb(ConvaihttpMethod);
			Request->SetHeader(TEXT("Content-Type"), TEXT("application/x-ueconvaihttp-upload-test"));
			Request->SetContentAsStreamedFile(UploadFilename);
			Request->ProcessRequest();
		}
		else
		{
			UE_LOG(LogConvaihttp, Warning, TEXT("Command expects params <upload url> <upload filename> [convaihttp verb]"))
		}
	}
#endif
	else if (FParse::Command(&Cmd, TEXT("LAUNCHREQUESTS")))
	{
		FString Verb = FParse::Token(Cmd, false);
		FString Url = FParse::Token(Cmd, false);
		int32 NumRequests = FCString::Atoi(*FParse::Token(Cmd, false));
		bool bCancelRequests = FCString::ToBool(*FParse::Token(Cmd, false));

		TArray64<TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe>> Requests;

		for (int32 i = 0; i < NumRequests; ++i)
		{
			TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> ConvaihttpRequest = FConvaihttpModule::Get().CreateRequest();
			ConvaihttpRequest->SetURL(*Url);
			ConvaihttpRequest->SetVerb(*Verb);
			ConvaihttpRequest->OnProcessRequestComplete().BindLambda([](FConvaihttpRequestPtr ConvaihttpRequest, FConvaihttpResponsePtr ConvaihttpResponse, bool bSucceeded) {});
			ConvaihttpRequest->ProcessRequest();

			Requests.Add(ConvaihttpRequest);
		}

		if (bCancelRequests)
		{
			for (auto Request : Requests)
			{
				Request->CancelRequest();
			}
		}
	}

	return true;
}

bool FConvaihttpModule::Exec(UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	// Ignore any execs that don't start with CONVAIHTTP
	if (FParse::Command(&Cmd, TEXT("CONVAIHTTP")))
	{
		return HandleCONVAIHTTPCommand( Cmd, Ar );
	}
	return false;
}

FConvaihttpModule& FConvaihttpModule::Get()
{
	if (Singleton == NULL)
	{
		check(IsInGameThread());
		FModuleManager::LoadModuleChecked<FConvaihttpModule>("CONVAIHTTP");
	}
	check(Singleton != NULL);
	return *Singleton;
}

TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe> FConvaihttpModule::CreateRequest()
{
	if (bUseNullConvaihttp)
	{
		return TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe>(new FNullConvaihttpRequest());
	}
	else
	{
		// Create the platform specific Convaihttp request instance
		return TSharedRef<IConvaihttpRequest, ESPMode::ThreadSafe>(FPlatformConvaihttp::ConstructRequest());
	}
}
