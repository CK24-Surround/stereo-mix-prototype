// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/SMTeam.h"
#include "SMTeamComponent.generated.h"

class ISMTeamComponentInterface;

DECLARE_LOG_CATEGORY_CLASS(LogSMTeamComponent, Log, All);

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STEREOMIXPROTOTYPE_API USMTeamComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USMTeamComponent();

public:
	virtual void InitializeComponent() override;

protected:
	virtual void BeginPlay() override;

protected:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

// ~TeamNumber Section
public:
	FORCEINLINE ESMTeam GetCurrentTeam() const { return CurrentTeam; }

	void SetTeam(ESMTeam InTeam);

protected:
	UFUNCTION()
	void OnRep_CurrentTeam();

protected:
	ISMTeamComponentInterface* TeamComponentInterface;
	
	UPROPERTY(ReplicatedUsing = OnRep_CurrentTeam)
	ESMTeam CurrentTeam;
// ~End of TeamNumber Section
};
