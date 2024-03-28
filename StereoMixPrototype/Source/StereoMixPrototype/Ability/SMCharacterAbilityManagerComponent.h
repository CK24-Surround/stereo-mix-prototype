// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SMCharacterAbilityComponent.h"
#include "Components/ActorComponent.h"
#include "SMCharacterAbilityManagerComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STEREOMIXPROTOTYPE_API USMCharacterAbilityManagerComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TMap<FString, USMCharacterAbilityComponent*> Abilities;

public:
	// Sets default values for this component's properties
	USMCharacterAbilityManagerComponent();

	virtual void InitializeComponent() override;

	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	USMCharacterAbilityComponent* GetAbility(const FString& AbilityName) const
	{
		return Abilities.FindRef(AbilityName);
	}

	void AddAbility(const FString& AbilityName, USMCharacterAbilityComponent* Ability)
	{
		Abilities.Add(AbilityName, Ability);
	}

protected:
};
