// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "SMTeamComponentInterface.generated.h"

// This class does not need to be modified.
class USMTeamComponent;

UINTERFACE()
class USMTeamComponentInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class STEREOMIXPROTOTYPE_API ISMTeamComponentInterface
{
	GENERATED_BODY()

public:
	virtual USMTeamComponent* GetTeamComponent() = 0;
	virtual void ResetTeamMaterial() = 0;
};
