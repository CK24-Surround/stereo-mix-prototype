// Fill out your copyright notice in the Description page of Project Settings.


#include "Character/SMSmashComponent.h"

#include "Interface/SMTileInterface.h"
#include "Log/SMLog.h"
#include "Physics/SMCollision.h"

USMSmashComponent::USMSmashComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void USMSmashComponent::BeginPlay()
{
	Super::BeginPlay();
}

void USMSmashComponent::TriggerTile(ESMTeam InTeam)
{
	FHitResult HitResult;
	const FVector Start = GetOwner()->GetActorLocation() + GetOwner()->GetActorUpVector() * 100.0f;
	const FVector End = Start + -GetOwner()->GetActorUpVector() * 10000.0f;
	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(Tile), false, GetOwner());
	const bool bSuccess = GetWorld()->SweepSingleByChannel(HitResult, Start, End, FQuat::Identity, TC_TILE, FCollisionShape::MakeSphere(10.0f), CollisionQueryParams);
	if (bSuccess)
	{
		ISMTileInterface* TileInterface = Cast<ISMTileInterface>(HitResult.GetActor());
		if (TileInterface)
		{
			ServerRPCTriggerTile_Implementation(HitResult.GetActor(), InTeam);
		}
	}
	
	const FColor Color = bSuccess ? FColor::Green : FColor::Red;
	DrawDebugLine(GetWorld(), Start, End, Color, false, 2.0f);
}

void USMSmashComponent::ServerRPCTriggerTile_Implementation(AActor* InTarget, ESMTeam InTeam)
{
	ISMTileInterface* TileInterface = Cast<ISMTileInterface>(InTarget);
	if (TileInterface)
	{
		TileInterface->TriggerTile(InTeam);
	}
}
