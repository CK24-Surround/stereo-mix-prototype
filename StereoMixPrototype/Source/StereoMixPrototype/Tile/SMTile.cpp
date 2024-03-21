// Fill out your copyright notice in the Description page of Project Settings.


#include "SMTile.h"

#include "SMTileAssetData.h"
#include "Data/AssetPath.h"
#include "Log/SMLog.h"
#include "Physics/SMCollision.h"

ASMTile::ASMTile()
{
	PrimaryActorTick.bCanEverTick = false;

	static ConstructorHelpers::FObjectFinder<USMTileAssetData> DA_TileAsset(TILE_DATA_ASSET_PATH);
	if (DA_TileAsset.Succeeded())
	{
		AssetData = DA_TileAsset.Object;
	}

	check(AssetData);

	BottomMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BottomMesh"));
	SetRootComponent(BottomMesh);
	BottomMesh->SetCollisionProfileName(CP_FLOOR);

	FrameMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FrameMesh"));
	FrameMesh->SetupAttachment(BottomMesh);
	FrameMesh->SetCollisionProfileName(CP_FLOOR);

	TileMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TileMesh"));
	TileMesh->SetupAttachment(FrameMesh);
	TileMesh->SetCollisionProfileName(CP_TILE);

	bReplicates = true;
}

void ASMTile::BeginPlay()
{
	Super::BeginPlay();
}

void ASMTile::TriggerTile(ESMTeam InTeam)
{
	NET_LOG(LogSMTile, Log, TEXT("트리거 된 타일: %s"), *GetName());
	TArray<ASMTile*> SelectedTiles = SelectAdjacentTiles();

	for (const auto& SelectedTile : SelectedTiles)
	{
		SelectedTile->TileChange(InTeam);
	}
}

void ASMTile::TileChange(ESMTeam InTeam)
{
	MulticastRPCTileColorChange(InTeam);
	//TODO: 점수 처리 로직 필요
}

void ASMTile::MulticastRPCTileColorChange_Implementation(ESMTeam InTeam)
{
	switch (InTeam)
	{
		case ESMTeam::None:
			break;
		case ESMTeam::FutureBass:
		{
			TileMesh->SetMaterial(0, AssetData->FutureBassTeamMaterial);
			break;
		}
		case ESMTeam::Rock:
		{
			TileMesh->SetMaterial(0, AssetData->RockTeamMaterial);
			break;
		}
	}
}

TArray<ASMTile*> ASMTile::SelectAdjacentTiles()
{
	auto test = FCollisionQueryParams::DefaultQueryParam;
	TArray<FOverlapResult> HitResults;
	const FVector Center = GetActorLocation() + FVector(0.0, 0.0, 100.0);
	const FVector Box(300.0f);
	const bool bSuccess = GetWorld()->OverlapMultiByChannel(HitResults, Center, FQuat::Identity, TC_TILE, FCollisionShape::MakeBox(Box));

	TArray<ASMTile*> SelectedTiles;
	if (bSuccess)
	{
		for (const auto& HitResult : HitResults)
		{
			ASMTile* SelectedTile = Cast<ASMTile>(HitResult.GetActor());
			if (SelectedTile)
			{
				SelectedTiles.Add(SelectedTile);
			}
		}
	}

	DrawDebugBox(GetWorld(), Center, Box, FColor::Cyan, false, 2.0f);
	return SelectedTiles;
}
