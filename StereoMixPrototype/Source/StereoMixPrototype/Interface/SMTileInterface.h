// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/SMTeam.h"
#include "UObject/Interface.h"
#include "SMTileInterface.generated.h"

// This class does not need to be modified.
UINTERFACE()
class USMTileInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * 
 */
class STEREOMIXPROTOTYPE_API ISMTileInterface
{
	GENERATED_BODY()

public:
	virtual void TriggerTile(ESMTeam InTeam) = 0;
};