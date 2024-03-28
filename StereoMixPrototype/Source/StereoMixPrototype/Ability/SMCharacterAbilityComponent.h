// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Character/SMPlayerCharacter.h"
#include "Components/ActorComponent.h"
#include "SMCharacterAbilityComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class STEREOMIXPROTOTYPE_API USMCharacterAbilityComponent : public UActorComponent
{
	GENERATED_BODY()

	FTimerHandle PerformActionTimerHandle;

	/**
	 * 액션의 소유자 캐릭터
	 */
	UPROPERTY(Replicated, meta = (AllowPrivateAccess = "true"))
	TObjectPtr<ASMPlayerCharacter> OwnerCharacter;

	/**
	 * 액션이 현재 실행 중인지 여부를 나타냅니다.
	 */
	UPROPERTY(Transient, ReplicatedUsing=OnRep_IsActivate)
	uint8 bIsActivate : 1;

protected:
	/**
	 * 액션의 쿨타임
	 */
	UPROPERTY(EditDefaultsOnly, Category="Action", meta = (AllowPrivateAccess = "true"))
	float DefaultCooldown;

	/**
	 * 액션의 실행 딜레이
	 */
	UPROPERTY(EditDefaultsOnly, Category="Action", meta = (AllowPrivateAccess = "true"))
	float PerformDelay;

	/**
	 * 액션이 이동 중에 실행 가능한지 여부를 나타냅니다.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Action", meta = (AllowPrivateAccess = "true"))
	bool bIsMovableAction;

	/**
	 * 현재 쿨타임
	 */
	UPROPERTY(Transient, ReplicatedUsing=OnRep_CurrentCooldown, meta = (AllowPrivateAccess = "true"))
	float CurrentCooldown;

	UPROPERTY(EditDefaultsOnly, Category = "Action")
	TObjectPtr<UAnimMontage> ActionMontage;

public:
	// Sets default values for this component's properties
	USMCharacterAbilityComponent();

	virtual void InitializeComponent() override;

	// Called when the game starts
	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	FORCEINLINE ASMPlayerCharacter* GetOwnerCharacter() const { return OwnerCharacter; }

	FORCEINLINE bool IsActivate() const { return bIsActivate; }

	FORCEINLINE float GetDefaultCooldown() const { return DefaultCooldown; }

	FORCEINLINE float GetPerformDelay() const { return PerformDelay; }

	FORCEINLINE bool IsMovableAction() const { return bIsMovableAction; }

	FORCEINLINE float GetCurrentCooldown() const { return CurrentCooldown; }

	FORCEINLINE const TObjectPtr<UAnimMontage>& GetActionMontage() const { return ActionMontage; }

	/**
	 * 클라이언트에서 인풋 입력을 받았을 때 호출되는 함수입니다.
	 */
	virtual void UseAbility();

	/**
	 * 서버에서 호출되는 액션 실행 RPC입니다.
	 */
	UFUNCTION(Server, Reliable, WithValidation)
	void ServerBeginAbility(const double Timestamp);

protected:
	UFUNCTION()
	virtual void OnRep_IsActivate();

	UFUNCTION()
	virtual void OnRep_CurrentCooldown();

	/**
	 * 액션을 실행할 수 있는지 여부를 반환합니다.
	 * 클라이언트, 서버 모두 액션 실행 전 호출되며 클라이언트에서 성공하더라도 서버에서 실패할 수 있습니다.
	 * @return 액션 실행 가능 여부
	 */
	UFUNCTION()
	virtual bool CanAction() const;

	UFUNCTION()
	virtual void OnBeginAbility(const double ClientDelay);

	UFUNCTION()
	virtual void OnPerformAbility();

	UFUNCTION()
	virtual void OnEndAbility();

	/**
	 * 액션 시작 시 클라이언트에서 호출되는 RPC입니다.
	 * 몽타쥬 실행, 이펙트 재생 화면에 보이는 요소들을 처리합니다.
	 * Autonomous Proxy인 경우 액션 시작 시 바로 로컬에서 호출되며 Simulated Proxy는 서버에 의해 호출됩니다.
	 * 로컬에서는 액션이 실행됐지만 서버에서 실행되지 않은 경우 Simulated Proxy에서는 호출되지 않습니다.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientPlayVisualEffectsOnBegin();

	UFUNCTION()
	virtual void OnClientPlayVisualEffectsOnBegin() const;

	/**
	 * 액션 실행 시 서버에 의해 클라이언트에서 호출되는 RPC입니다.
	 * 몽타쥬 실행, 이펙트 재생 화면에 보이는 요소들을 처리합니다.
	 * 무조건 서버에 의해 클라이언트에서 호출되며 실제로 액션이 실행되었을 때 호출됩니다.
	 */
	UFUNCTION(Client, Unreliable)
	void ClientPlayVisualEffectsOnPerform();

	UFUNCTION()
	virtual void OnClientPlayVisualEffectsOnPerform() const;

private:
	void OnActionMontageEnded(UAnimMontage* Montage, bool bInterrupted);
};
