// Copyright Epic Games, Inc. All Rights Reserved.

#include "HoloLens/HoloLensPlatformConvaihttp.h"
#include "IXML/ConvaihttpIXML.h"

void FHoloLensConvaihttp::Init()
{
}

void FHoloLensConvaihttp::Shutdown()
{
}

FConvaihttpManager * FHoloLensConvaihttp::CreatePlatformConvaihttpManager()
{
	return nullptr;
}

IConvaihttpRequest* FHoloLensConvaihttp::ConstructRequest()
{
	return new FConvaihttpRequestIXML();
}