// Copyright Convai Inc. All Rights Reserved.
#pragma once
#include "Commandlets/Commandlet.h"
#include "ConvaiAvatarTransportCommandlet.generated.h"

/** A file-only worker. The host owns credentials, metadata, and avatar installation. */
UCLASS()
class CONVAIHTTPTRANSFER_API UConvaiAvatarTransportCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UConvaiAvatarTransportCommandlet();
    virtual int32 Main(const FString& Params) override;
};
