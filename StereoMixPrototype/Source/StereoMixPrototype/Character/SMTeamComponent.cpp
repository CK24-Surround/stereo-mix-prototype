// Fill out your copyright notice in the Description page of Project Settings.


#include "SMTeamComponent.h"

#include "Interface/SMTeamComponentInterface.h"
#include "Log/SMLog.h"
#include "Net/UnrealNetwork.h"

USMTeamComponent::USMTeamComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);

	bWantsInitializeComponent = true;
}

void USMTeamComponent::InitializeComponent()
{
	Super::InitializeComponent();

	TeamComponentInterface = Cast<ISMTeamComponentInterface>(GetOwner());
}


void USMTeamComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USMTeamComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USMTeamComponent, CurrentTeam);
}

void USMTeamComponent::SetTeam(ESMTeam InTeam)
{
	if (GetOwnerRole() == ROLE_Authority)
	{
		if (CurrentTeam != InTeam)
		{
			CurrentTeam = InTeam;
			OnRep_CurrentTeam();
		}
	}
}

void USMTeamComponent::OnRep_CurrentTeam()
{
	const FString TeamName = UEnum::GetValueAsString(TEXT("StereoMixPrototype.ESMTeam"), CurrentTeam);
	NET_COMP_LOG(LogSMTeamComponent, Warning, TEXT("%s로 팀 변경"), *TeamName);

	if (GetOwnerRole() != ROLE_Authority)
	{
		if (TeamComponentInterface)
		{
			TeamComponentInterface->ResetTeamMaterial();
		}
	}
}
