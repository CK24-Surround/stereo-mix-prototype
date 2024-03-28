// Fill out your copyright notice in the Description page of Project Settings.


#include "SMCharacterAbilityManagerComponent.h"

// Sets default values for this component's properties
USMCharacterAbilityManagerComponent::USMCharacterAbilityManagerComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}

void USMCharacterAbilityManagerComponent::InitializeComponent()
{
	Super::InitializeComponent();
}

// Called when the game starts
void USMCharacterAbilityManagerComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}

void USMCharacterAbilityManagerComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
}

// Called every frame
void USMCharacterAbilityManagerComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                        FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
