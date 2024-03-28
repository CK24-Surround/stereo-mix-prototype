// Fill out your copyright notice in the Description page of Project Settings.


#include "SMCharacterAbilityComponent.h"

#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
USMCharacterAbilityComponent::USMCharacterAbilityComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
}

void USMCharacterAbilityComponent::InitializeComponent()
{
	Super::InitializeComponent();
	OwnerCharacter = CastChecked<ASMPlayerCharacter>(GetOwner());
	OwnerCharacter->GetRootMotionAnimMontageInstance()->OnMontageEnded.BindUObject(
		this, &USMCharacterAbilityComponent::OnActionMontageEnded);
}

// Called when the game starts
void USMCharacterAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USMCharacterAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
}

// Called every frame
void USMCharacterAbilityComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USMCharacterAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USMCharacterAbilityComponent, OwnerCharacter);
	DOREPLIFETIME_CONDITION(USMCharacterAbilityComponent, CurrentCooldown, COND_OwnerOnly);
}

void USMCharacterAbilityComponent::OnRep_IsActivate()
{
}

void USMCharacterAbilityComponent::OnRep_CurrentCooldown()
{
}

void USMCharacterAbilityComponent::UseAbility()
{
	if (!OwnerCharacter->IsLocallyControlled())
	{
		return;
	}
	if (!CanAction())
	{
		return;
	}

	if (ActionMontage)
	{
		ClientPlayVisualEffectsOnBegin();
		const float PlaySeconds = OwnerCharacter->PlayAnimMontage(ActionMontage);
		if (PlaySeconds == 0.0f)
		{
			return;
		}
	}
	ServerBeginAbility(GetWorld()->GetGameState()->GetServerWorldTimeSeconds());
}

bool USMCharacterAbilityComponent::ServerBeginAbility_Validate(const double Timestamp)
{
	// 딜레이가 음수가 나온다는 것은 있을 수 없는 일입니다.
	if (GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - Timestamp < 0.0f)
	{
		return false;
	}

	return true;
}

void USMCharacterAbilityComponent::ServerBeginAbility_Implementation(const double Timestamp)
{
	if (!OwnerCharacter->HasAuthority())
	{
		return;
	}

	// 추후 서버 되감기 수행하는 코드 추가
	if (!CanAction())
	{
		return;
	}
	OnBeginAbility(GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - Timestamp);
}

bool USMCharacterAbilityComponent::CanAction() const
{
	if (CurrentCooldown > 0.0f)
	{
		return false;
	}
	return true;
}

void USMCharacterAbilityComponent::OnBeginAbility(const double ClientDelay)
{
	if (!OwnerCharacter->HasAuthority())
	{
		return;
	}
	for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		// play vfx & call rpc
	}

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (PerformActionTimerHandle.IsValid())
	{
		TimerManager.ClearTimer(PerformActionTimerHandle);
	}
	const float Delay = FMath::Max(0.0f, PerformDelay - ClientDelay);
	TimerManager.SetTimer(PerformActionTimerHandle, this, &USMCharacterAbilityComponent::OnPerformAbility, Delay,
	                      false);
}

void USMCharacterAbilityComponent::OnPerformAbility()
{
#if WITH_SERVER_CODE
	if (!OwnerCharacter->HasAuthority())
	{
		return;
	}

	ClientPlayVisualEffectsOnPerform();

	// ...
#endif
}

void USMCharacterAbilityComponent::OnEndAbility()
{
}

void USMCharacterAbilityComponent::ClientPlayVisualEffectsOnBegin_Implementation()
{
	OnClientPlayVisualEffectsOnBegin();
}

void USMCharacterAbilityComponent::OnClientPlayVisualEffectsOnBegin() const
{
}

void USMCharacterAbilityComponent::ClientPlayVisualEffectsOnPerform_Implementation()
{
	OnClientPlayVisualEffectsOnPerform();
}

void USMCharacterAbilityComponent::OnClientPlayVisualEffectsOnPerform() const
{
}

// ReSharper disable once CppParameterMayBeConstPtrOrRef
void USMCharacterAbilityComponent::OnActionMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
	if (Montage != ActionMontage.Get())
	{
		return;
	}
	if (bInterrupted)
	{
		// interrupted일 때 다른 콜백 함수를 호출할건지?
	}
	OnEndAbility();
}
