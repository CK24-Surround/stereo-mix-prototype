// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SMCharacterAnimationAssetData.generated.h"

class UNiagaraSystem;
/**
 * 
 */
UCLASS()
class STEREOMIXPROTOTYPE_API USMCharacterAnimationAssetData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> CatchMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> CaughtMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> SmashMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> DownStartMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> DownEndMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> RangedAttackMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> Stun;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> GrabSmash_DashMontage;

	UPROPERTY(EditDefaultsOnly, Category = "AnimationMontage")
	TObjectPtr<UAnimMontage> GrabSmash_SmashMontage;

public:
	UPROPERTY(EditDefaultsOnly, Category = "AnimationSequence")
	TObjectPtr<UAnimSequence> StunEnd;
};
