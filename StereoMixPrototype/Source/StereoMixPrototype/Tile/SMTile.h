// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Data/SMTeam.h"
#include "GameFramework/Actor.h"
#include "Interface/SMTileInterface.h"
#include "SMTile.generated.h"

class USMTileAssetData;

DECLARE_LOG_CATEGORY_CLASS(LogSMTile, Log, All);

UCLASS()
class STEREOMIXPROTOTYPE_API ASMTile : public AActor,
                                       public ISMTileInterface
{
	GENERATED_BODY()

public:
	ASMTile();

protected:
	virtual void BeginPlay() override;

public:
	virtual void TriggerTile(ESMTeam InTeam) override;
	
	void TileChange(ESMTeam InTeam);

	UFUNCTION(NetMulticast, Reliable)
	void MulticastRPCTileColorChange(ESMTeam InTeam);

	TArray<ASMTile*> SelectAdjacentTiles();

protected:
	UPROPERTY()
	TObjectPtr<USMTileAssetData> AssetData;

protected:
	UPROPERTY(VisibleAnywhere, Category = "Bottom")
	TObjectPtr<UStaticMeshComponent> BottomMesh;

	UPROPERTY(VisibleAnywhere, Category = "Frame")
	TObjectPtr<UStaticMeshComponent> FrameMesh;

	UPROPERTY(VisibleAnywhere, Category = "Tile")
	TObjectPtr<UStaticMeshComponent> TileMesh;
};
