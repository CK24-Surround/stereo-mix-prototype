// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/SMCharacterBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interface/SMCharacterAnimationInterface.h"
#include "Interface/SMProjectileInterface.h"
#include "SMPlayerCharacter.generated.h"

class USMPostureGaugeWidget;
class UWidgetComponent;
class USMCharacterAnimInstance;
class AAimPlane;
class ASMPlayerController;
class USpringArmComponent;
class UCameraComponent;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_CLASS(LogSMCharacter, Log, All);

UENUM(BlueprintType)
enum class EPlayerCharacterState : uint8
{
	Normal,
	Stun,
	Caught,
	Down,
	Smash
};

UENUM()
enum class EGrabState : uint8
{
	Idle,
	Charge,
	Dash,
	Smash
};

/**
 * 
 */
UCLASS()
class STEREOMIXPROTOTYPE_API ASMPlayerCharacter : public ASMCharacterBase, public ISMCharacterAnimationInterface,
                                                  public ISMProjectileInterface
{
	GENERATED_BODY()

public:
	ASMPlayerCharacter();

public:
	virtual void PostInitializeComponents() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

	// ~Property Replicate Section
public:
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void OnRep_Controller() override;

protected:
	UFUNCTION()
	void OnRep_bCanControl();

	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_bEnableMovement();

	UFUNCTION()
	void OnRep_bEnableCollision();

	UFUNCTION()
	void OnRep_CollisionProfileName();

	UFUNCTION()
	void OnRep_bIsStunned();

protected:
	UPROPERTY(ReplicatedUsing = OnRep_bCanControl)
	uint32 bCanControl : 1;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentState)
	EPlayerCharacterState CurrentState;

	UPROPERTY(ReplicatedUsing = OnRep_bEnableMovement)
	uint32 bEnableMovement : 1;

	UPROPERTY(ReplicatedUsing = OnRep_bEnableCollision)
	uint32 bEnableCollision : 1;

	UPROPERTY(ReplicatedUsing = OnRep_CollisionProfileName)
	FName CollisionProfileName;

	UPROPERTY(ReplicatedUsing = OnRep_bIsStunned)
	uint32 bIsStunned : 1;
	// ~End of Property Replicate Section

protected:
	virtual void BeginPlay() override;

public:
	virtual void Tick(float DeltaSeconds) override;

	// ~Design Section
protected:
	// ~End of Design Section

	// ~Camera Section
protected:
	/** 카메라 초기세팅에 사용되는 함수입니다. */
	void InitCamera();

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
	// ~End of Camera Section

	// ~Input Section
protected:
	/** 컨트롤러의 초기 세팅에 사용되는 함수입니다. */
	void InitCharacterControl();

	/** 캐릭터 기준으로 마우스 포인터가 가리키는 방향을 반환합니다. */
	FVector GetMousePointingDirection();

	/** GetMousePointingDirection()를 통해 얻어낸 방향으로 캐릭터를 회전시킵니다. */
	void UpdateRotateToMousePointer();
	// ~End of Input Section

	// ~Movement Section
public:
	void SetEnableMovement(bool bInEnableMovement);

protected:
	void Move(const FInputActionValue& InputActionValue);
	// ~End of Movement Section

	// ~Character State Section
public:
	FORCEINLINE EPlayerCharacterState GetCurrentState() { return CurrentState; }
	void SetCurrentState(EPlayerCharacterState InState);
	void SetEnableCollision(bool bInEnableCollision);
	void SetCollisionProfileName(FName InCollisionProfileName);

	FORCEINLINE void SetStunned(bool bInStunned)
	{
		if (HasAuthority())
		{
			bIsStunned = bInStunned;
			OnRep_bIsStunned();
		}
	}

protected:
	/** 서버에서 실행됩니다.*/
	void ApplyStunned();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCPlayStunAnimation();

	/** 서버에서 실행됩니다.*/
	void RecoverStunned();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastRPCPlayStunEndAnimation();

	void StunEnded(UAnimMontage* InAnimMontage, bool bInterrupted);

protected:
	FTimerHandle StunTimerHandle;
	// ~End of Character Section

	// ~Aim Section
protected:
	UPROPERTY()
	TObjectPtr<AAimPlane> AimPlane;
	// ~End of Aim Section

	// ~Control Section
public:
	void SetCanControl(bool bInEnableControl);

protected:
	UPROPERTY()
	TObjectPtr<ASMPlayerController> StoredSMPlayerController;
	// ~End of Control Section

	// ~Util Section
public:
	/** 서버와 시전자를 제외한 플레이어 캐릭터들을 반환합니다. */
	TArray<ASMPlayerCharacter*> GetCharactersExcludingServerAndCaster();

	/** 현재 캐릭터의 위치에서 가장 가까운 바닥까지의 거리를 반환합니다. */
	float DistanceHeightFromFloor();
	// ~End of Util Section

	// ~Jump Section
protected:
	/** 점프 시 호출되는 이벤트입니다. */
	virtual void OnJumped_Implementation() override;

	/** 착지 시 호출되는 이벤트입니다. */
	virtual void Landed(const FHitResult& Hit) override;
	// ~Jump Section

	// ~Stat Section
protected:
	const float MoveSpeed = 700.0f;
	// ~End of Stat Section

	// ~Catch Section
protected:
	struct FPullData
	{
		uint32 bIsPulling : 1 = false;
		ASMPlayerCharacter* Caster;
		FVector StartLocation;
		FVector EndLocation;
		float ElapsedTime = 0.0f;
		float TotalTime = 0.25f;
		float LastServerTime = 0.0f;
	};

public:
	FORCEINLINE ASMPlayerCharacter* GetCaughtCharacter() { return CaughtCharacter; }
	void SetCaughtCharacter(ASMPlayerCharacter* InCaughtCharacter);

protected:
	void Catch();

	void CanCatch();

	UFUNCTION(Server, Unreliable)
	void ServerRPCPlayCatchAnimation();

	UFUNCTION(Client, Unreliable)
	void ClientRPCPlayCatchAnimation(const ASMPlayerCharacter* InPlayAnimationCharacter) const;

	virtual void AnimNotify_Catch() override;

	UFUNCTION(Server, Reliable)
	void ServerRPCPerformPull(ASMPlayerCharacter* InTargetCharacter);

	UFUNCTION(Client, Reliable)
	void ClientRPCLastTimeCheck();

	void UpdatePerformPull();

	UFUNCTION(Client, Unreliable)
	void ClientRPCInterpolationPull(FVector_NetQuantize10 InInterpolationLocation);

	void UpdateInterpolationPull();

	void HandlePullEnd();

	UFUNCTION(Client, Reliable)
	void ClientRPCPlayCaughtAnimation(ASMPlayerCharacter* InPlayAnimationCharacter) const;

protected:
	FPullData PullData;

	UPROPERTY()
	TObjectPtr<USMCharacterAnimInstance> StoredSMAnimInstance;

	UPROPERTY(Replicated)
	TObjectPtr<ASMPlayerCharacter> CaughtCharacter;

	uint32 bCanCatch : 1 = true;

public: // Animation Interface Section
	FORCEINLINE virtual bool GetHasAcceleration() override
	{
		return GetCharacterMovement()->GetCurrentAcceleration() != FVector::ZeroVector;
	}

	FORCEINLINE virtual bool GetIsFalling() override { return GetCharacterMovement()->IsFalling(); }
	FORCEINLINE virtual float GetZVelocity() override { return GetCharacterMovement()->Velocity.Z; }

	// ~Smash Section
protected:
	void Smash();

	UFUNCTION(Server, Unreliable)
	void ServerRPCPlaySmashAnimation();

	UFUNCTION(Client, Unreliable)
	void ClientRPCPlaySmashAnimation(const ASMPlayerCharacter* InCharacterToAnimation) const;

	/** 애님 노티파이에 의해 호출됩니다. */
	virtual void AnimNotify_Smash() override;

	UFUNCTION(Server, Unreliable)
	void ServerRPCPlayKnockDownAnimation();

	UFUNCTION(Client, Unreliable)
	void ClientRPCPlayKnockDownAnimation(ASMPlayerCharacter* InCharacterToAnimation);

	UFUNCTION(Server, Reliable)
	void ServerRPCDetachToCaster(FVector_NetQuantize10 InLocation, FRotator InRotation);

	/** 스매시 애니메이션이 종료되고난 뒤에 호출됩니다. */
	void SmashEnded(UAnimMontage* PlayAnimMontage, bool bInterrupted);

	UFUNCTION(Client, Reliable)
	void ClientRPCSetRotation(FRotator InRotation);

	/** 타이머 실행 이후(기상 시간) 기상 애니메이션을 재생시킵니다. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPCPlayStandUpAnimation();

	/** 기상 애니메이션이 종료되고난 뒤에 호출됩니다. */
	void StandUpEnded(UAnimMontage* PlayAnimMontage, bool bInterrupted);
	// ~End of Smash Section

	// ~Grab Smash Section
private:
	TOptional<FVector> SmashPoint;

	float LastChargedGauge;

	UPROPERTY(EditAnywhere, Category = "Charge")
	float MaxChargeGauge;

	UPROPERTY(ReplicatedUsing = OnRep_ChargeGauge, VisibleInstanceOnly, Category = "Charge",
		meta = (AllowPrivateAccess = true))
	float ChargeGauge;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentGrabState, VisibleInstanceOnly, Category = "Charge",
		meta = (AllowPrivateAccess = true))
	EGrabState CurrentGrabState;

	UFUNCTION()
	void OnRep_ChargeGauge() const;

	UFUNCTION()
	void OnRep_CurrentGrabState() const;

	UFUNCTION(Server, Reliable)
	void ServerRPCBeginCharge(const double LocalBeginTime);

	UFUNCTION(Server, Reliable)
	void ServerRPCBeginGrab(const double LocalBeginTime);

	UFUNCTION(Client, Reliable)
	void ClientRPCBeginGrabSmash(ASMPlayerCharacter* InstigatorCharacter);

	UFUNCTION(Client, Reliable)
	void ClientRPCPerformGrabSmash(ASMPlayerCharacter* InstigatorCharacter, ASMPlayerCharacter* TargetCharacter);

	UFUNCTION(Client, Reliable)
	void ClientRPCPostGrabSmash(ASMPlayerCharacter* InstigatorCharacter, ASMPlayerCharacter* TargetCharacter);

	UFUNCTION()
	void ResetCharge();

	UFUNCTION()
	void StartGrab(const float ChargedGauge);

	UFUNCTION()
	void CancelGrab();

	bool CanGrab() const;
	ASMPlayerCharacter* FindGrabTarget() const;

	void MoveToTargetCharacterOnTick(const float DeltaTime);
	void MoveToSmashPointOnTick(const float DeltaTime);
	bool MoveToPointOnTick(const FVector& TargetPoint, const float DeltaTime);

protected:
	void GrabCharge();
	void GrabSmash();
	// ~End of Grab Smash Section

	// ~Ranged Attack Section
protected:
	void RangedAttack();

	UFUNCTION(Server, Unreliable)
	void ServerRPCPlayRangedAttackAnimation();

	UFUNCTION(Client, Unreliable)
	void ClientRPCPlayRangedAttackAnimation(const ASMPlayerCharacter* CharacterToAnimation) const;

	void CanRangedAttack();

	virtual void AnimNotify_RangedAttack() override;

	UFUNCTION(Server, Reliable)
	void ServerRPCShootProjectile(ASMPlayerCharacter* NewOwner);

	/** 투사체에 적중당하면 서버측에서 호출됩니다. */
	virtual void HitProjectile() override;

protected:
	uint32 bCanRangedAttack : 1 = true;
	// ~End of Ranged Attack Section

	// ~UI Section
protected:
	UPROPERTY(VisibleAnywhere, Category = "UI")
	TObjectPtr<UWidgetComponent> PostureGauge;

	UPROPERTY()
	TObjectPtr<USMPostureGaugeWidget> PostureGaugeWidget;
	// ~End of UI Section
};
