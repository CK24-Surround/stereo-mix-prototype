// Fill out your copyright notice in the Description page of Project Settings.


#include "Action/SMCharacterActionComponent.h"

#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

// Sets default values for this component's properties
USMCharacterActionComponent::USMCharacterActionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
}

void USMCharacterActionComponent::InitializeComponent()
{
	Super::InitializeComponent();
	OwnerCharacter = CastChecked<ASMPlayerCharacter>(GetOwner());
}

// Called when the game starts
void USMCharacterActionComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USMCharacterActionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);

	GetWorld()->GetTimerManager().ClearAllTimersForObject(this);
}

// Called every frame
void USMCharacterActionComponent::TickComponent(const float DeltaTime, const ELevelTick TickType,
                                                FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USMCharacterActionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USMCharacterActionComponent, OwnerCharacter);
	DOREPLIFETIME_CONDITION(USMCharacterActionComponent, CurrentCooldown, COND_OwnerOnly);
}

void USMCharacterActionComponent::OnRep_IsActivate()
{
}

void USMCharacterActionComponent::OnRep_CurrentCooldown()
{
}

void USMCharacterActionComponent::UseAction()
{
	if (!OwnerCharacter->IsLocallyControlled())
	{
		return;
	}
	if (!CanAction())
	{
		return;
	}
	ClientPlayVisualEffectsOnBegin();
	ServerBeginAction(GetWorld()->GetGameState()->GetServerWorldTimeSeconds());
}

bool USMCharacterActionComponent::ServerBeginAction_Validate(const double Timestamp)
{
	// 딜레이가 음수가 나온다는 것은 있을 수 없는 일입니다.
	if (GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - Timestamp < 0.0f)
	{
		return false;
	}

	return true;
}

void USMCharacterActionComponent::ServerBeginAction_Implementation(const double Timestamp)
{
	OnBeginAction(GetWorld()->GetGameState()->GetServerWorldTimeSeconds() - Timestamp);
}

bool USMCharacterActionComponent::CanAction() const
{
	return true;
}

void USMCharacterActionComponent::OnBeginAction(const double ClientDelay)
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
	ClientPlayVisualEffectsOnBegin();

	FTimerManager& TimerManager = GetWorld()->GetTimerManager();
	if (PerformActionTimerHandle.IsValid())
	{
		TimerManager.ClearTimer(PerformActionTimerHandle);
	}
	const float Delay = FMath::Max(0.0f, PerformDelay - ClientDelay);
	TimerManager.SetTimer(PerformActionTimerHandle, this, &USMCharacterActionComponent::OnPerformAction, Delay, false);
}

void USMCharacterActionComponent::OnPerformAction()
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

void USMCharacterActionComponent::ClientPlayVisualEffectsOnBegin_Implementation()
{
}

void USMCharacterActionComponent::ClientPlayVisualEffectsOnPerform_Implementation()
{
}
