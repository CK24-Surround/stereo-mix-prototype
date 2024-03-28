// Fill out your copyright notice in the Description page of Project Settings.


#include "SMGrabSmashAbilityComponent.h"

#include "Animation/SMCharacterAnimationAssetData.h"
#include "Animation/SMCharacterAnimInstance.h"

USMGrabSmashAbilityComponent::USMGrabSmashAbilityComponent()
{
	DefaultCooldown = 1.0f;
	PerformDelay = 0.0f;
	bIsMovableAction = false;

	GrabRadius = 100.0f;
}

void USMGrabSmashAbilityComponent::InitializeComponent()
{
	Super::InitializeComponent();

	ActionMontage = GetOwnerCharacter()->GetStoredSMAnimInstance()->GetAssetData()->GrabSmash_DashMontage;
	SmashMontage = GetOwnerCharacter()->GetStoredSMAnimInstance()->GetAssetData()->GrabSmash_SmashMontage;
}

void USMGrabSmashAbilityComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USMGrabSmashAbilityComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

void USMGrabSmashAbilityComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void USMGrabSmashAbilityComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
}

bool USMGrabSmashAbilityComponent::CanAction() const
{
	const EPlayerCharacterState CurrentState = GetOwnerCharacter()->GetCurrentState();
	if (CurrentState != EPlayerCharacterState::Normal)
	{
		return false;
	}

	return Super::CanAction();
}

void USMGrabSmashAbilityComponent::OnBeginAbility(const double ClientDelay)
{
	Super::OnBeginAbility(ClientDelay);

	CurrentCooldown = DefaultCooldown;
}

void USMGrabSmashAbilityComponent::OnPerformAbility()
{
	Super::OnPerformAbility();
}

void USMGrabSmashAbilityComponent::OnClientPlayVisualEffectsOnBegin() const
{
	Super::OnClientPlayVisualEffectsOnBegin();
}

void USMGrabSmashAbilityComponent::OnClientPlayVisualEffectsOnPerform() const
{
	Super::OnClientPlayVisualEffectsOnPerform();
}
