// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SMTeam.h"
#include "SMSmashComponent.generated.h"

DECLARE_LOG_CATEGORY_CLASS(LogSMSmashComponent, Log, All);

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class STEREOMIXPROTOTYPE_API USMSmashComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	USMSmashComponent();

protected:
	virtual void BeginPlay() override;

public:
	void TriggerTile(ESMTeam InTeam);

protected:
	UFUNCTION(Server, Reliable)
	void ServerRPCTriggerTile(AActor* InTarget, ESMTeam InTeam);
};
