// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "SMCharacterAbilityComponent.h"
#include "SMGrabSmashAbilityComponent.generated.h"

/**
 * 
 */
UCLASS()
class STEREOMIXPROTOTYPE_API USMGrabSmashAbilityComponent : public USMCharacterAbilityComponent
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category="GrabSmash", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UAnimMontage> SmashMontage;

	UPROPERTY(EditDefaultsOnly, Category="GrabSmash", meta = (AllowPrivateAccess = "true"))
	float GrabRadius;

public:
	USMGrabSmashAbilityComponent();

	virtual void InitializeComponent() override;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual bool CanAction() const override;

	virtual void OnBeginAbility(const double ClientDelay) override;

	virtual void OnPerformAbility() override;

	virtual void OnClientPlayVisualEffectsOnBegin() const override;

	virtual void OnClientPlayVisualEffectsOnPerform() const override;
};
