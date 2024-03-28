// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SMCharacterAnimInstance.generated.h"

class ISMCharacterAnimationInterface;
class USMCharacterAnimationAssetData;

DECLARE_LOG_CATEGORY_CLASS(LogSMAnim, Log, All);

/**
 * 
 */
UCLASS()
class STEREOMIXPROTOTYPE_API USMCharacterAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	USMCharacterAnimInstance();

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

protected: // Data Asset Section
	void AssetCheck();

	UPROPERTY()
	TObjectPtr<const USMCharacterAnimationAssetData> AssetData;

protected: // Cached Section
	ISMCharacterAnimationInterface* StoredAnimationInterface;

	// ~Montage Section
public:
	void PlayCatch();
	void PlayCaught();

	void PlaySmash();

	void PlayKnockDown();
	void PlayStandUp();

	void PlayRangedAttack();

	void PlayStun();
	void PlayStunEnd();
	// ~End of Montage Section

	// ~Delegate Section
public:
	FOnMontageEnded OnSmashEnded;
	FOnMontageEnded OnStandUpEnded;
	FOnMontageEnded OnStunEnded;
	// ~End of Delegate Section

	// ~State Section
protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CharacterState")
	uint32 bHasAcceleration : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CharacterState")
	uint32 bIsFalling : 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CharacterState")
	float ZVelocity;
	// ~End of State Section

	// ~Animation Data Section
public:
	const USMCharacterAnimationAssetData* GetAssetData() const
	{
		return AssetData;
	}

	float GetStunEndLength();
	// ~End of Animation Data Section
};
