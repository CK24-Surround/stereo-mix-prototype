// Fill out your copyright notice in the Description page of Project Settings.


#include "SMPlayerCharacter.h"

#include "Data/SMTeam.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "SMCharacterAssetData.h"
#include "SMSmashComponent.h"
#include "SMTeamComponent.h"
#include "Animation/SMCharacterAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "CharacterStat/SMCharacterStatComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Design/SMPlayerCharacterDesignData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Log/SMLog.h"
#include "Net/UnrealNetwork.h"
#include "Physics/SMCollision.h"
#include "Player/AimPlane.h"
#include "Player/SMPlayerController.h"
#include "Projectile/SMRangedAttackProjectile.h"
#include "UI/SMPostureGaugeWidget.h"

ASMPlayerCharacter::ASMPlayerCharacter()
{
	GetMesh()->SetCollisionProfileName("NoCollision");

	UCharacterMovementComponent* CachedCharacterMovement = GetCharacterMovement();
	CachedCharacterMovement->MaxWalkSpeed = MoveSpeed;
	CachedCharacterMovement->MaxAcceleration = 10000.0f;
	CachedCharacterMovement->BrakingDecelerationWalking = 10000.0f;
	CachedCharacterMovement->BrakingDecelerationFalling = 0.0f; // ~ 100 중 선택
	CachedCharacterMovement->AirControl = 1.0f;

	CachedCharacterMovement->GravityScale = 2.0f;
	CachedCharacterMovement->JumpZVelocity = 700.0f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(GetRootComponent());

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(CameraBoom);

	PostureGauge = CreateDefaultSubobject<UWidgetComponent>(TEXT("PostureGauge"));
	PostureGauge->SetupAttachment(GetRootComponent());
	PostureGauge->SetWidgetSpace(EWidgetSpace::Screen);
	PostureGauge->SetDrawAtDesiredSize(true);
	if (AssetData->PostureGauge)
	{
		PostureGauge->SetWidgetClass(AssetData->PostureGauge);
	}

	TeamComponent = CreateDefaultSubobject<USMTeamComponent>(TEXT("Team"));
	SmashComponent = CreateDefaultSubobject<USMSmashComponent>(TEXT("Smash"));

	InitCamera();

	CurrentState = EPlayerCharacterState::Normal;
	bEnableCollision = true;
	bEnableMovement = true;
	bCanControl = true;
	CollisionProfileName = CP_PLAYER;
	bIsStunned = false;

	MaxChargeGauge = 2.0f;
	ChargeGauge = 0.0f;
	CurrentGrabState = EGrabState::Idle;
}

void ASMPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	StoredSMAnimInstance = CastChecked<USMCharacterAnimInstance>(GetMesh()->GetAnimInstance());

	if (HasAuthority())
	{
		StoredSMAnimInstance->OnSmashEnded.BindUObject(this, &ASMPlayerCharacter::SmashEnded);
		StoredSMAnimInstance->OnStandUpEnded.BindUObject(this, &ASMPlayerCharacter::StandUpEnded);
		StoredSMAnimInstance->OnStunEnded.BindUObject(this, &ASMPlayerCharacter::StunEnded);
		Stat->OnZeroPostureGauge.AddUObject(this, &ASMPlayerCharacter::ApplyStunned);
	}
}

void ASMPlayerCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 무조건 서버에서만 실행되겠지만 만약을 위한 조건입니다.
	if (HasAuthority())
	{
		// 서버에선 OnRep_Controller가 호출되지 않고, 클라이언트에서는 PossessedBy가 호출되지 않기 때문에 서버는 여기서 컨트롤러를 캐싱합니다.
		StoredSMPlayerController = CastChecked<ASMPlayerController>(GetController());
	}
}

void ASMPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(AssetData->MoveAction, ETriggerEvent::Triggered, this,
		                                   &ASMPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(AssetData->JumpAction, ETriggerEvent::Triggered, this,
		                                   &ASMPlayerCharacter::Jump);

		// GrabSmash

		EnhancedInputComponent->BindAction(AssetData->SmashAction, ETriggerEvent::Started, this,
		                                   &ASMPlayerCharacter::GrabCharge);
		EnhancedInputComponent->BindAction(AssetData->SmashAction, ETriggerEvent::Completed, this,
		                                   &ASMPlayerCharacter::GrabSmash);

		// Catch & Smash
		/*
		EnhancedInputComponent->BindAction(AssetData->HoldAction, ETriggerEvent::Started, this,
		                                   &ASMPlayerCharacter::Catch);
		EnhancedInputComponent->BindAction(AssetData->SmashAction, ETriggerEvent::Started, this,
		                                   &ASMPlayerCharacter::Smash);
		*/

		EnhancedInputComponent->BindAction(AssetData->RangedAttackAction, ETriggerEvent::Triggered, this,
		                                   &ASMPlayerCharacter::RangedAttack);

		EnhancedInputComponent->BindAction(AssetData->FutureBaseTeamSelectAction, ETriggerEvent::Started, this,
		                                   &ASMPlayerCharacter::ServerRPCFutureBassTeamSelect);
		EnhancedInputComponent->BindAction(AssetData->RockTeamSelectAction, ETriggerEvent::Started, this,
		                                   &ASMPlayerCharacter::ServerRPCRockTeamSelect);
	}
}

void ASMPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASMPlayerCharacter, CurrentState);
	DOREPLIFETIME(ASMPlayerCharacter, bEnableCollision);
	DOREPLIFETIME(ASMPlayerCharacter, bCanControl);
	DOREPLIFETIME(ASMPlayerCharacter, bEnableMovement);
	DOREPLIFETIME(ASMPlayerCharacter, CaughtCharacter);
	DOREPLIFETIME(ASMPlayerCharacter, CollisionProfileName);
	DOREPLIFETIME(ASMPlayerCharacter, bIsStunned);
	DOREPLIFETIME(ASMPlayerCharacter, ChargeGauge);
	DOREPLIFETIME(ASMPlayerCharacter, CurrentGrabState);
}

void ASMPlayerCharacter::OnRep_Controller()
{
	Super::OnRep_Controller();

	// 로컬 컨트롤러를 캐싱해야하기위한 조건입니다.
	if (IsLocallyControlled())
	{
		// 서버에선 OnRep_Controller가 호출되지 않고, 클라이언트에서는 PossessedBy가 호출되지 않기 때문에 서버는 여기서 컨트롤러를 캐싱합니다.
		StoredSMPlayerController = CastChecked<ASMPlayerController>(GetController());
	}
}

void ASMPlayerCharacter::OnRep_bCanControl()
{
	if (bCanControl)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("컨트롤 활성화"))
		EnableInput(StoredSMPlayerController);
		bUseControllerRotationPitch = true;
		bUseControllerRotationYaw = true;
		bUseControllerRotationRoll = true;
	}
	else
	{
		NET_LOG(LogSMCharacter, Log, TEXT("컨트롤 비활성화"))
		DisableInput(StoredSMPlayerController);
		bUseControllerRotationPitch = false;
		bUseControllerRotationYaw = false;
		bUseControllerRotationRoll = false;
	}
}

void ASMPlayerCharacter::OnRep_CurrentState()
{
	switch (CurrentState)
	{
	case EPlayerCharacterState::Normal:
		{
			NET_LOG(LogSMCharacter, Log, TEXT("현재 캐릭터 상태: Normal"));
			break;
		}
	case EPlayerCharacterState::Stun:
		{
			NET_LOG(LogSMCharacter, Log, TEXT("현재 캐릭터 상태: Stun"));
			break;
		}
	case EPlayerCharacterState::Caught:
		{
			NET_LOG(LogSMCharacter, Log, TEXT("현재 캐릭터 상태: Caught"));
			break;
		}
	case EPlayerCharacterState::Down:
		{
			NET_LOG(LogSMCharacter, Log, TEXT("현재 캐릭터 상태: Down"));
			break;
		}
	case EPlayerCharacterState::Smash:
		{
			NET_LOG(LogSMCharacter, Log, TEXT("현재 캐릭터 상태: Smash"));
			break;
		}
	}
}

void ASMPlayerCharacter::OnRep_bEnableMovement()
{
	if (bEnableMovement)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("움직임 활성화"));
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
	}
	else
	{
		NET_LOG(LogSMCharacter, Log, TEXT("움직임 비활성화"));
		GetCharacterMovement()->SetMovementMode(MOVE_None);
	}
}

void ASMPlayerCharacter::OnRep_bEnableCollision()
{
	NET_LOG(LogSMCharacter, Log, TEXT("충돌 %s"), bEnableCollision ? TEXT("활성화") : TEXT("비활성화"));
	SetActorEnableCollision(bEnableCollision);
}

void ASMPlayerCharacter::OnRep_CollisionProfileName()
{
	NET_LOG(LogSMCharacter, Log, TEXT("콜리전 프로파일: %s"), *CollisionProfileName.ToString())
	GetCapsuleComponent()->SetCollisionProfileName(CollisionProfileName);
}

void ASMPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocallyControlled())
	{
		AimPlane = GetWorld()->SpawnActor<AAimPlane>();
		const FAttachmentTransformRules AttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
		AimPlane->AttachToActor(this, AttachmentTransformRules);
	}

	// 서버는 UI 업데이트가 필요하지 않습니다.
	if (!HasAuthority())
	{
		PostureGaugeWidget = Cast<USMPostureGaugeWidget>(PostureGauge->GetWidget());
		if (PostureGaugeWidget)
		{
			Stat->OnChangedPostureGauge.AddDynamic(PostureGaugeWidget, &USMPostureGaugeWidget::UpdatePostureGauge);
			PostureGaugeWidget->UpdatePostureGauge(Stat->GetCurrentPostureGauge(), Stat->GetBaseStat().MaxPostureGauge);
		}
	}

	InitCharacterControl();

	PullData.TotalTime = DesignData->CatchTime;
}

void ASMPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCanControl)
	{
		UpdateRotateToMousePointer();
	}

	if (PullData.bIsPulling)
	{
		if (HasAuthority())
		{
			UpdatePerformPull();
		}
		else
		{
			UpdateInterpolationPull();
		}
	}

#if WITH_SERVER_CODE
	if (HasAuthority())
	{
		// NET_LOG(LogSMCharacter, Log, TEXT("%f, %d, %s"), ChargeGauge, CurrentGrabState, *GetNameSafe(TargetCharacter));
		if (CurrentGrabState == EGrabState::Charge && ChargeGauge < MaxChargeGauge)
		{
			NET_LOG(LogSMCharacter, Log, TEXT("Charge"));
			ChargeGauge += DeltaSeconds;
			NET_LOG(LogSMCharacter, Log, TEXT("ChargeGauge: %f"), ChargeGauge);
			if (ChargeGauge >= MaxChargeGauge)
			{
				ChargeGauge = MaxChargeGauge;
			}
		}
		else if (CurrentGrabState == EGrabState::Dash && CaughtCharacter)
		{
			NET_LOG(LogSMCharacter, Log, TEXT("Dash"));
			MoveToTargetCharacterOnTick(DeltaSeconds);
		}
		else if (CurrentGrabState == EGrabState::Smash && CaughtCharacter)
		{
			NET_LOG(LogSMCharacter, Log, TEXT("Smash"));
			MoveToSmashPointOnTick(DeltaSeconds);
		}
	}
#endif
	if (!HasAuthority())
	{
	}
}

void ASMPlayerCharacter::InitCamera()
{
	const FRotator CameraRotation(-45.0f, 0.0, 0.0);
	constexpr float CameraDistance = 750.0f;
	constexpr float CameraFOV = 90.0f;

	CameraBoom->SetRelativeRotation(CameraRotation);
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->TargetArmLength = CameraDistance;
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bEnableCameraLag = true;

	Camera->SetFieldOfView(CameraFOV);
}

void ASMPlayerCharacter::InitCharacterControl()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	APlayerController* PlayerController = Cast<APlayerController>(GetController());
	if (PlayerController)
	{
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(
			PlayerController->GetLocalPlayer());
		if (Subsystem)
		{
			Subsystem->ClearAllMappings();
			Subsystem->AddMappingContext(AssetData->DefaultMappingContext, 0);
		}
	}

	FInputModeGameOnly InputModeGameOnly;
	InputModeGameOnly.SetConsumeCaptureMouseDown(false);
	PlayerController->SetInputMode(InputModeGameOnly);
}

FVector ASMPlayerCharacter::GetMousePointingDirection()
{
	FHitResult HitResult;
	const bool Succeed = StoredSMPlayerController->GetHitResultUnderCursor(TC_AIM_PLANE, false, HitResult);
	if (Succeed)
	{
		const FVector MouseLocation = HitResult.Location;
		const FVector MouseDirection = (MouseLocation - GetActorLocation()).GetSafeNormal();
		return MouseDirection;
	}

	return FVector::ZeroVector;
}

void ASMPlayerCharacter::UpdateRotateToMousePointer()
{
	if (IsLocallyControlled())
	{
		const FVector MousePointingDirection = GetMousePointingDirection();
		if (MousePointingDirection == FVector::ZeroVector)
		{
			return;
		}

		const FRotator MousePointingRotation = FRotationMatrix::MakeFromX(MousePointingDirection).Rotator();
		const FRotator NewRotation = FRotator(0.0, MousePointingRotation.Yaw, 0.0);

		StoredSMPlayerController->SetControlRotation(NewRotation);
	}
}

void ASMPlayerCharacter::SetEnableMovement(bool bInEnableMovement)
{
	if (HasAuthority())
	{
		bEnableMovement = bInEnableMovement;
		OnRep_bEnableMovement();
	}
}

void ASMPlayerCharacter::Move(const FInputActionValue& InputActionValue)
{
	const FVector2D InputScalar = InputActionValue.Get<FVector2D>().GetSafeNormal();
	const FRotator CameraYawRotation(0.0, Camera->GetComponentRotation().Yaw, 0.0);
	const FVector ForwardDirection = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(CameraYawRotation).GetUnitAxis(EAxis::Y);
	const FVector MoveVector = (ForwardDirection * InputScalar.X) + (RightDirection * InputScalar.Y);

	const UCharacterMovementComponent* CachedCharacterMovement = GetCharacterMovement();
	if (CachedCharacterMovement->IsFalling())
	{
		const FVector NewMoveVector = MoveVector / 2.0f;
		AddMovementInput(NewMoveVector);
	}
	else
	{
		AddMovementInput(MoveVector);
	}
}

void ASMPlayerCharacter::SetCurrentState(EPlayerCharacterState InState)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentState = InState;
	if (CurrentGrabState != EGrabState::Idle)
	{
		CancelGrab();
	}
	OnRep_CurrentState();
}

void ASMPlayerCharacter::SetEnableCollision(bool bInEnableCollision)
{
	if (!HasAuthority())
	{
		return;
	}

	bEnableCollision = bInEnableCollision;
	OnRep_bEnableCollision();
}

void ASMPlayerCharacter::SetCollisionProfileName(FName InCollisionProfileName)
{
	if (!HasAuthority())
	{
		return;
	}

	CollisionProfileName = InCollisionProfileName;
	OnRep_CollisionProfileName();
}

void ASMPlayerCharacter::ApplyStunned()
{
	NET_LOG(LogSMCharacter, Log, TEXT("기절 상태 적용"));
	SetStunned(true);
	SetCurrentState(EPlayerCharacterState::Stun);

	StoredSMAnimInstance->PlayStun();
	MulticastRPCPlayStunAnimation();
	const float StunEndTime = DesignData->StunTime - StoredSMAnimInstance->GetStunEndLength();
	GetWorldTimerManager().SetTimer(StunTimerHandle, this, &ASMPlayerCharacter::RecoverStunned, StunEndTime, false);
}

void ASMPlayerCharacter::MulticastRPCPlayStunAnimation_Implementation()
{
	if (!HasAuthority())
	{
		StoredSMAnimInstance->PlayStun();
	}
}

void ASMPlayerCharacter::RecoverStunned()
{
	// 만약 기절 상태가 아니면(잡혀있다면) 아래 코드를 수행하지 않아야 합니다. 
	if (CurrentState == EPlayerCharacterState::Stun)
	{
		SetCanControl(true);
		SetEnableMovement(true);
		SetEnableCollision(true);
		StoredSMAnimInstance->PlayStunEnd();
		MulticastRPCPlayStunEndAnimation();
	}
}

void ASMPlayerCharacter::MulticastRPCPlayStunEndAnimation_Implementation()
{
	if (!HasAuthority())
	{
		StoredSMAnimInstance->PlayStunEnd();
	}
}

void ASMPlayerCharacter::StunEnded(UAnimMontage* InAnimMontage, bool bInterrupted)
{
	if (bInterrupted)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("기절 애니메이션 중단"));
		return;
	}

	Stat->ClearPostureGauge();
	SetStunned(false);
	SetCurrentState(EPlayerCharacterState::Normal);
}

void ASMPlayerCharacter::OnRep_bIsStunned()
{
	NET_LOG(LogSMCharacter, Log, TEXT("스턴 상태: %s"), bIsStunned ? TEXT("활성화") : TEXT("비활성화"));
	SetCanControl(!bIsStunned);
	bIsStunned ? SetCollisionProfileName(CP_STUNNED) : SetCollisionProfileName(CP_PLAYER);
}

void ASMPlayerCharacter::SetCanControl(bool bInEnableControl)
{
	if (!HasAuthority())
	{
		return;
	}

	NET_LOG(LogSMCharacter, Log, TEXT("컨트롤 %s"), bInEnableControl ? TEXT("활성화") : TEXT("비활성화"));
	bCanControl = bInEnableControl;
	OnRep_bCanControl();
}

TArray<ASMPlayerCharacter*> ASMPlayerCharacter::GetCharactersExcludingServerAndCaster()
{
	// 해당 로직은 for문 한 번 순회 후 외부에서 배열로 한 번 더 순회하기 떄문에 비효율적입니다. (곽필경)
	TArray<ASMPlayerCharacter*> Result;
	for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		if (!PlayerController || PlayerController->IsLocalController() || PlayerController == GetController())
		{
			continue;
		}

		ASMPlayerCharacter* SMPlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn());
		if (SMPlayerCharacter)
		{
			Result.Add(SMPlayerCharacter);
		}
	}

	return Result;
}

float ASMPlayerCharacter::DistanceHeightFromFloor()
{
	FHitResult HitResult;
	const FVector Start = GetActorLocation() + (-GetActorUpVector() * 90.5f);
	const FVector End = GetActorLocation() + (-GetActorUpVector() * 10000.0f);
	FCollisionObjectQueryParams CollisionObjectQueryParams;
	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(DistanceHeigh), false, this);
	CollisionObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	const bool bSuccess = GetWorld()->LineTraceSingleByObjectType(HitResult, Start, End, CollisionObjectQueryParams,
	                                                              CollisionQueryParams);
	if (bSuccess)
	{
		const float Distance = (HitResult.Location - Start).Size();
		NET_LOG(LogSMCharacter, Log, TEXT("Height: %f"), Distance);

		return Distance;
	}
	return 0.0f;
}

void ASMPlayerCharacter::OnJumped_Implementation()
{
	Super::OnJumped_Implementation();

	NET_LOG(LogSMCharacter, Log, TEXT("Jumped!"));
}

void ASMPlayerCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	NET_LOG(LogSMCharacter, Log, TEXT("Landed! Floor Info: %s"), *Hit.GetComponent()->GetName());
}

void ASMPlayerCharacter::SetCaughtCharacter(ASMPlayerCharacter* InCaughtCharacter)
{
	if (HasAuthority())
	{
		CaughtCharacter = InCaughtCharacter;
	}
}

void ASMPlayerCharacter::Catch()
{
	if (bCanCatch)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("잡기 시전"));
		bCanCatch = false;

		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, this, &ASMPlayerCharacter::CanCatch,
		                                DesignData->CatchCoolDownTime);

		if (!CaughtCharacter)
		{
			StoredSMAnimInstance->PlayCatch();
			ServerRPCPlayCatchAnimation();
		}
	}
}

void ASMPlayerCharacter::CanCatch()
{
	bCanCatch = true;
}

void ASMPlayerCharacter::ServerRPCPlayCatchAnimation_Implementation()
{
	for (const ASMPlayerCharacter* RemoteCharacter : GetCharactersExcludingServerAndCaster())
	{
		RemoteCharacter->ClientRPCPlayCatchAnimation(this);
	}
}

void ASMPlayerCharacter::ClientRPCPlayCatchAnimation_Implementation(
	const ASMPlayerCharacter* InPlayAnimationCharacter) const
{
	InPlayAnimationCharacter->StoredSMAnimInstance->PlayCatch();
}

void ASMPlayerCharacter::AnimNotify_Catch()
{
	if (IsLocallyControlled())
	{
		if (CurrentGrabState != EGrabState::Idle)
		{
			return;
		}

		NET_LOG(LogSMCharacter, Log, TEXT("잡기 시작"));

		// 나중에 기절 종료 애니메이션이 재생되는 중에 잡기가 성사되고, 서버지연으로인해 잡기가 먼저 처리 된 뒤 기절 종료 애니가 재생 마무리 될경우 버그를 방지하기 위해 서버측에서 유효하지 않은 상황이라면 롤백하는 검증 코드가 필요합니다.

		// 충돌 로직
		FHitResult HitResult;
		const FVector Start = GetActorLocation();
		const FVector End = Start + (GetActorForwardVector() * 300.0f);
		FCollisionObjectQueryParams CollisionObjectQueryParams;
		CollisionObjectQueryParams.AddObjectTypesToQuery(OC_STUNNED);
		FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(Catch), false, this);
		bool bSuccess = GetWorld()->SweepSingleByObjectType(HitResult, Start, End, FQuat::Identity,
		                                                    CollisionObjectQueryParams,
		                                                    FCollisionShape::MakeSphere(50.0f), CollisionQueryParams);

		// 충돌 시
		if (bSuccess)
		{
			NET_LOG(LogSMCharacter, Log, TEXT("잡기 적중"));
			ASMPlayerCharacter* HitPlayerCharacter = Cast<ASMPlayerCharacter>(HitResult.GetActor());
			if (HitPlayerCharacter)
			{
				if (!HitPlayerCharacter->GetCaughtCharacter())
				{
					ServerRPCPerformPull(HitPlayerCharacter);
				}
				else
				{
					bSuccess = false;
				}
			}
		}

		// 디버거
		const FVector Center = Start + (End - Start) * 0.5f;
		const FColor DrawColor = bSuccess ? FColor::Green : FColor::Red;
		DrawDebugCapsule(GetWorld(), Center, 150.0f, 50.0f,
		                 FRotationMatrix::MakeFromZ(GetActorForwardVector()).ToQuat(), DrawColor, false, 1.0f);
	}
}

void ASMPlayerCharacter::ServerRPCPerformPull_Implementation(ASMPlayerCharacter* InTargetCharacter)
{
	NET_LOG(LogSMCharacter, Log, TEXT("당기기 시작"));

	// 기절 타이머 초기화
	// 현재는 잡힌 도중 기절에서 탈출할 수 없도록 설계했습니다. 추후 탈출 로직 추가가 필요합니다.
	// InTargetCharacter->GetWorldTimerManager().ClearTimer(InTargetCharacter->StunTimerHandle);

	// 클라이언트 제어권 박탈 및 충돌 판정 비활성화
	InTargetCharacter->SetCanControl(false);
	InTargetCharacter->SetEnableMovement(false);
	InTargetCharacter->SetEnableCollision(false);

	InTargetCharacter->SetCurrentState(EPlayerCharacterState::Caught);

	// 당기기에 필요한 데이터 할당
	InTargetCharacter->PullData.bIsPulling = true;
	InTargetCharacter->PullData.ElapsedTime = 0.0f;
	InTargetCharacter->PullData.Caster = this;
	InTargetCharacter->PullData.StartLocation = InTargetCharacter->GetActorLocation();
	InTargetCharacter->ClientRPCLastTimeCheck();
}

void ASMPlayerCharacter::ClientRPCLastTimeCheck_Implementation()
{
	PullData.LastServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
}

void ASMPlayerCharacter::UpdatePerformPull()
{
	if (!HasAuthority())
	{
		return;
	}

	// 시전자로부터 자신을 향한 방향
	FVector CasterVector = GetActorLocation() - PullData.Caster->GetActorLocation();
	CasterVector.Z = 0.0;
	const FVector CasterDirection = CasterVector.GetSafeNormal();

	// 캡슐 반지름을 통해 EndLocation이 시전자의 위치와 겹치지 않도록 오프셋 지정
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	PullData.EndLocation = PullData.Caster->GetActorLocation() + (CasterDirection * CapsuleRadius * 2);

	// 선형 보간
	PullData.ElapsedTime += GetWorld()->GetDeltaSeconds();
	const float Alpha = FMath::Clamp(PullData.ElapsedTime / PullData.TotalTime, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(PullData.StartLocation, PullData.EndLocation, Alpha);
	SetActorLocation(NewLocation);
	ClientRPCInterpolationPull(NewLocation);

	// 디버거
	DrawDebugLine(GetWorld(), PullData.StartLocation, PullData.EndLocation, FColor::Cyan, false, 0.1f);

	if (Alpha >= 1.0f)
	{
		HandlePullEnd();
	}
}

void ASMPlayerCharacter::ClientRPCInterpolationPull_Implementation(FVector_NetQuantize10 InInterpolationLocation)
{
	if (HasAuthority())
	{
		return;
	}

	PullData.bIsPulling = true;

	PullData.StartLocation = GetActorLocation();
	PullData.EndLocation = InInterpolationLocation;

	PullData.ElapsedTime = 0.0f;

	const float CurrentServerTime = GetWorld()->GetGameState()->GetServerWorldTimeSeconds();
	PullData.TotalTime = CurrentServerTime - PullData.LastServerTime;
	PullData.LastServerTime = CurrentServerTime;
}

void ASMPlayerCharacter::UpdateInterpolationPull()
{
	if (HasAuthority())
	{
		return;
	}

	// 서버와 클라이언트 간의 호출 순서가 보장되지 않기 때문에 어태치 된 후에도 이 로직이 수행될 수 있습니다. 이런 경우를 방지하는 코드입니다.
	if (GetAttachParentActor())
	{
		PullData.bIsPulling = false;
		return;
	}

	PullData.ElapsedTime += GetWorld()->GetDeltaSeconds();
	const float Alpha = FMath::Clamp(PullData.ElapsedTime / PullData.TotalTime, 0.0f, 1.0f);
	const FVector NewLocation = FMath::Lerp(PullData.StartLocation, PullData.EndLocation, Alpha);

	SetActorLocation(NewLocation);

	if (Alpha >= 1.0f)
	{
		PullData.bIsPulling = false;
	}
}

void ASMPlayerCharacter::HandlePullEnd()
{
	if (HasAuthority())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("당기기 종료"))
		PullData.bIsPulling = false;

		if (!PullData.Caster)
		{
			return;
		}

		NET_LOG(LogSMCharacter, Log, TEXT("어태치 시작"))

		GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = true;
		PullData.Caster->SetCaughtCharacter(this);

		AttachToComponent(PullData.Caster->GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale,
		                  TEXT("CatchSocket"));
		StoredSMPlayerController->SetViewTargetWithBlend(PullData.Caster, 0.3f);

		for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
		{
			if (!PlayerController || PlayerController->IsLocalController())
			{
				continue;
			}

			const ASMPlayerCharacter* PlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn());
			if (PlayerCharacter)
			{
				PlayerCharacter->ClientRPCPlayCaughtAnimation(this);
			}
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlayCaughtAnimation_Implementation(ASMPlayerCharacter* InPlayAnimationCharacter) const
{
	if (InPlayAnimationCharacter)
	{
		InPlayAnimationCharacter->StoredSMAnimInstance->PlayCaught();
	}
}

void ASMPlayerCharacter::Smash()
{
	NET_LOG(LogSMCharacter, Log, TEXT("매치기 트리거"));

	if (CaughtCharacter)
	{
		StoredSMAnimInstance->PlaySmash();
		ServerRPCPlaySmashAnimation();
	}
}

void ASMPlayerCharacter::ServerRPCPlaySmashAnimation_Implementation()
{
	SetCurrentState(EPlayerCharacterState::Smash);
	SetCanControl(false);
	StoredSMAnimInstance->PlaySmash();

	for (const ASMPlayerCharacter* CharacterToAnimation : GetCharactersExcludingServerAndCaster())
	{
		if (CharacterToAnimation)
		{
			CharacterToAnimation->ClientRPCPlaySmashAnimation(this);
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlaySmashAnimation_Implementation(
	const ASMPlayerCharacter* InCharacterToAnimation) const
{
	if (InCharacterToAnimation)
	{
		InCharacterToAnimation->StoredSMAnimInstance->PlaySmash();
	}
}

void ASMPlayerCharacter::AnimNotify_Smash()
{
	// 캐스터 클라이언트에서만 실행되야합니다.
	if (!IsLocallyControlled())
	{
		return;
	}

	if (CurrentGrabState != EGrabState::Idle)
	{
		return;
	}

	if (!CaughtCharacter)
	{
		return;
	}

	NET_LOG(LogSMCharacter, Log, TEXT("매치기 처리"));

	CaughtCharacter->StoredSMAnimInstance->PlayKnockDown();
	ServerRPCPlayKnockDownAnimation();

	// 클라이언트에서 먼저 매쳐진 위치로 대상을 이동시킵니다. 이후 서버에도 해당 데이터를 보내 동기화시킵니다.
	// 매쳐지는 위치를 직접 정하는 이유는 애니메이션에만 의존해서 의도된 위치에 매쳐지도록 조정하기 어렵기 때문입니다.
	const FVector DownLocation = GetActorLocation() + (GetActorForwardVector() * 150.0f);
	FRotator DownRotation = FRotationMatrix::MakeFromX(GetActorForwardVector()).Rotator();
	DownRotation.Pitch = 0.0;
	DownRotation.Roll = 0.0;
	ServerRPCDetachToCaster(DownLocation, DownRotation);
}

void ASMPlayerCharacter::ServerRPCPlayKnockDownAnimation_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	if (CaughtCharacter)
	{
		CaughtCharacter->StoredSMAnimInstance->PlayKnockDown();
	}

	for (const auto CharacterToAnimation : GetCharactersExcludingServerAndCaster())
	{
		if (CharacterToAnimation)
		{
			CharacterToAnimation->ClientRPCPlayKnockDownAnimation(CaughtCharacter);
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlayKnockDownAnimation_Implementation(ASMPlayerCharacter* InCharacterToAnimation)
{
	if (InCharacterToAnimation)
	{
		InCharacterToAnimation->StoredSMAnimInstance->PlayKnockDown();
	}
}

void ASMPlayerCharacter::ServerRPCDetachToCaster_Implementation(FVector_NetQuantize10 InLocation, FRotator InRotation)
{
	if (!HasAuthority())
	{
		return;
	}

	if (CaughtCharacter)
	{
		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, CaughtCharacter.Get(),
		                                &ASMPlayerCharacter::MulticastRPCPlayStandUpAnimation, DesignData->StandUpTime);

		NET_LOG(LogSMCharacter, Log, TEXT("디태치"));
		CaughtCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		CaughtCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
		CaughtCharacter->SetEnableMovement(true);
		CaughtCharacter->SetEnableCollision(true);
		CaughtCharacter->SetCollisionProfileName(CP_KNOCK_DOWN);

		// 뷰타겟 설정은 서버에서만 실행해주면 알아서 동기화됩니다.
		CaughtCharacter->StoredSMPlayerController->SetViewTargetWithBlend(CaughtCharacter, 0.3f);

		// 로컬에서 회전을 변경해주는 이유는 컨트롤러 관련은 언리얼에서 클라이언트 우선이기 때문입니다. 여기서 바꾸는 값은 컨트롤러의 회전값입니다.
		CaughtCharacter->SetActorLocation(InLocation);
		CaughtCharacter->ClientRPCSetRotation(InRotation);

		CaughtCharacter->SmashComponent->TriggerTile(TeamComponent->GetCurrentTeam());

		SetCaughtCharacter(nullptr);
	}
}

void ASMPlayerCharacter::SmashEnded(UAnimMontage* PlayAnimMontage, bool bInterrupted)
{
	if (HasAuthority())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("매치기 애니메이션 종료"))
		SetCurrentState(EPlayerCharacterState::Normal);
		SetCanControl(true);
	}
}

void ASMPlayerCharacter::ClientRPCSetRotation_Implementation(FRotator InRotation)
{
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;
	bUseControllerRotationRoll = true;
	StoredSMPlayerController->SetControlRotation(InRotation);
}

void ASMPlayerCharacter::MulticastRPCPlayStandUpAnimation_Implementation()
{
	StoredSMAnimInstance->PlayStandUp();
	if (HasAuthority())
	{
		SetCurrentState(EPlayerCharacterState::Normal);
		SetCanControl(true);
		SetEnableMovement(true);
		SetEnableCollision(true);
	}
}

void ASMPlayerCharacter::StandUpEnded(UAnimMontage* PlayAnimMontage, bool bInterrupted)
{
	if (HasAuthority())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("기상 애니메이션 종료"));
		Stat->ClearPostureGauge();
		SetCurrentState(EPlayerCharacterState::Normal);
		SetCollisionProfileName(CP_PLAYER);
		SetCanControl(true);
	}
}

void ASMPlayerCharacter::OnRep_ChargeGauge() const
{
	NET_LOG(LogSMCharacter, Log, TEXT("OnRep_ChargeGauge: %f"), ChargeGauge);
}

void ASMPlayerCharacter::OnRep_CurrentGrabState() const
{
	NET_LOG(LogSMCharacter, Log, TEXT("OnRep_CurrentGrabState: %d"), CurrentGrabState);
}

void ASMPlayerCharacter::ClientRPCBeginGrabSmash_Implementation(ASMPlayerCharacter* InstigatorCharacter)
{
	NET_LOG(LogSMCharacter, Log, TEXT("ClientRPCBeginGrabSmash"));
	if (IsLocallyControlled())
	{
		// SetRole(ROLE_SimulatedProxy);
		GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Exponential;
	}
	InstigatorCharacter->StoredSMAnimInstance->PlayCatch();
}

void ASMPlayerCharacter::ClientRPCPerformGrabSmash_Implementation(ASMPlayerCharacter* InstigatorCharacter,
                                                                  ASMPlayerCharacter* TargetCharacter)
{
	NET_LOG(LogSMCharacter, Log, TEXT("ClientRPCPerformGrabSmash"));
	InstigatorCharacter->StoredSMAnimInstance->PlaySmash();
	if (TargetCharacter)
	{
		TargetCharacter->StoredSMAnimInstance->PlayCaught();
	}
}

void ASMPlayerCharacter::ClientRPCPostGrabSmash_Implementation(ASMPlayerCharacter* InstigatorCharacter,
                                                               ASMPlayerCharacter* TargetCharacter)
{
	NET_LOG(LogSMCharacter, Log, TEXT("ClientRPCPostGrabSmash"));
	if (IsLocallyControlled())
	{
		// SetRole(ROLE_AutonomousProxy);
		GetCharacterMovement()->NetworkSmoothingMode = ENetworkSmoothingMode::Disabled;
	}
	if (TargetCharacter)
	{
		TargetCharacter->StoredSMAnimInstance->PlayKnockDown();
	}
}

void ASMPlayerCharacter::ResetCharge()
{
	CurrentGrabState = EGrabState::Idle;
	ChargeGauge = 0.0f;
}

void ASMPlayerCharacter::StartGrab(const float ChargedGauge)
{
#if WITH_SERVER_CODE
	if (!GetOwner()->HasAuthority())
	{
		CancelGrab();
		return;
	}

	if (!CanGrab())
	{
		CancelGrab();
		return;
	}

	CurrentGrabState = EGrabState::Dash;
	CaughtCharacter = FindGrabTarget();
	NET_LOG(LogSMCharacter, Log, TEXT("Grab target: %s"), *GetNameSafe(CaughtCharacter));
	if (!CaughtCharacter)
	{
		CancelGrab();
		return;
	}
	CaughtCharacter->SetCanControl(false);
	CaughtCharacter->SetEnableMovement(false);
	CaughtCharacter->SetEnableCollision(false);
	this->LastChargedGauge = ChargedGauge;

	for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		if (ASMPlayerCharacter* SMPlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn()))
		{
			SMPlayerCharacter->ClientRPCBeginGrabSmash(this);
		}
	}


#endif
}

void ASMPlayerCharacter::CancelGrab()
{
	CurrentGrabState = EGrabState::Idle;
	SetCanControl(true);
	SetEnableMovement(true);
	SetEnableCollision(true);
	if (CaughtCharacter)
	{
		CaughtCharacter->ApplyStunned();
		CaughtCharacter = nullptr;
	}
}

bool ASMPlayerCharacter::CanGrab() const
{
	return CurrentState == EPlayerCharacterState::Normal;
}

ASMPlayerCharacter* ASMPlayerCharacter::FindGrabTarget() const
{
	FCollisionObjectQueryParams CollisionObjectQueryParams;
	CollisionObjectQueryParams.AddObjectTypesToQuery(OC_STUNNED);
	const FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(Catch), false, this);

	const FVector ActorLocation = GetActorLocation();
	TArray<FHitResult> HitResults;
	bool bSuccess = GetWorld()->SweepMultiByObjectType(HitResults, ActorLocation, ActorLocation, FQuat::Identity,
	                                                   CollisionObjectQueryParams,
	                                                   FCollisionShape::MakeSphere(300.0f),
	                                                   CollisionQueryParams);

#if WITH_EDITOR
	const FColor DrawColor = bSuccess ? FColor::Green : FColor::Red;
	DrawDebugSphere(GetWorld(), ActorLocation, 300.0f, 16, DrawColor, false, 1.0f);
#endif

	if (bSuccess)
	{
		for (const auto& HitResult : HitResults)
		{
			ASMPlayerCharacter* HitCharacter = Cast<ASMPlayerCharacter>(HitResult.GetActor());
			if (HitCharacter && HitCharacter->GetCurrentState() == EPlayerCharacterState::Stun)
			{
				return HitCharacter;
			}
		}
	}

	return nullptr;
}

void ASMPlayerCharacter::MoveToTargetCharacterOnTick(const float DeltaTime)
{
	if (!CaughtCharacter)
	{
		return;
	}

	if (MoveToPointOnTick(CaughtCharacter->GetActorLocation(), DeltaTime))
	{
		for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
		{
			if (ASMPlayerCharacter* SMPlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn()))
			{
				SMPlayerCharacter->ClientRPCPerformGrabSmash(this, CaughtCharacter);
			}
		}
		CaughtCharacter->SetCurrentState(EPlayerCharacterState::Caught);

		// set SmashPoint to Direction + 100f
		const FVector Direction = GetActorForwardVector();
		// TODO: change smash point to be calculated charge gauge

		float First = MaxChargeGauge / 3.0f;
		float Second = MaxChargeGauge / 3.0f * 2.0f;
		float SmashDistance = 0.0f;
		if (LastChargedGauge < First)
		{
			SmashDistance = 50.0f;
		}
		else if (LastChargedGauge < Second)
		{
			SmashDistance = 250.0f;
		}
		else
		{
			SmashDistance = 500.0f;
		}

		SmashPoint = CaughtCharacter->GetActorLocation() + Direction * SmashDistance;

		FHitResult HitResult;
		const FVector End = GetActorLocation() + (-GetActorUpVector() * 10000.0f);
		FCollisionObjectQueryParams CollisionObjectQueryParams;
		FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(DistanceHeigh), false, this);
		CollisionObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
		const bool bSuccess = GetWorld()->LineTraceSingleByObjectType(HitResult, SmashPoint.GetValue(), End,
		                                                              CollisionObjectQueryParams,
		                                                              CollisionQueryParams);
		if (bSuccess)
		{
			SmashPoint = HitResult.Location;
		}

		CurrentGrabState = EGrabState::Smash;
		CaughtCharacter->AttachToComponent(GetMesh(), FAttachmentTransformRules::SnapToTargetIncludingScale,
		                                   TEXT("CatchSocket"));
	}
}

void ASMPlayerCharacter::MoveToSmashPointOnTick(const float DeltaTime)
{
	if (!SmashPoint.IsSet())
	{
		return;
	}

	if (MoveToPointOnTick(SmashPoint.GetValue(), DeltaTime))
	{
		for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
		{
			ASMPlayerCharacter* SMPlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn());
			if (SMPlayerCharacter)
			{
				SMPlayerCharacter->ClientRPCPostGrabSmash(this, CaughtCharacter);
			}
		}

		FTimerHandle TimerHandle;
		GetWorldTimerManager().SetTimer(TimerHandle, CaughtCharacter.Get(),
		                                &ASMPlayerCharacter::MulticastRPCPlayStandUpAnimation, DesignData->StandUpTime);

		NET_LOG(LogSMCharacter, Log, TEXT("디태치"));
		CaughtCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

		CaughtCharacter->GetCharacterMovement()->bIgnoreClientMovementErrorChecksAndCorrection = false;
		CaughtCharacter->SetCollisionProfileName(CP_KNOCK_DOWN);

		// 뷰타겟 설정은 서버에서만 실행해주면 알아서 동기화됩니다.
		CaughtCharacter->StoredSMPlayerController->SetViewTargetWithBlend(CaughtCharacter, 0.3f);

		// 로컬에서 회전을 변경해주는 이유는 컨트롤러 관련은 언리얼에서 클라이언트 우선이기 때문입니다. 여기서 바꾸는 값은 컨트롤러의 회전값입니다.
		CaughtCharacter->SetActorLocation(SmashPoint.GetValue());
		CaughtCharacter->ClientRPCSetRotation(GetActorForwardVector().Rotation());

		SetCanControl(true);
		SetCaughtCharacter(nullptr);

		CurrentGrabState = EGrabState::Idle;
		SmashPoint.Reset();
	}
}

bool ASMPlayerCharacter::MoveToPointOnTick(const FVector& TargetPoint, const float DeltaTime)
{
	const FVector CurrentLocation = GetActorLocation();
	const FVector Direction = (TargetPoint - CurrentLocation).GetSafeNormal();
	const FVector MoveVector = Direction * MoveSpeed * DeltaTime;
	const float DistanceToTarget = (TargetPoint - CurrentLocation).Size();
	FVector NewLocation;


	if (MoveVector.Size() > DistanceToTarget)
	{
		// Bug 1: 층이 다른 경우 캐릭터가 타겟 위치까지 이동할 수 없으므로 영원히 Dash 상태가 됨.
		// Bug 2: 스매시 위치 층이 다른 경우 위치 체크가 제대로 되지 않아 영원히 Smash 상태가 됨.
		NewLocation = TargetPoint;
		// End move to target position
		return true;
	}
	NewLocation = CurrentLocation + MoveVector;

	const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
	SetActorLocationAndRotation(NewLocation, Rotation);
	return false;
}

void ASMPlayerCharacter::GrabCharge()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	if (CurrentGrabState != EGrabState::Idle)
	{
		return;
	}
	NET_LOG(LogSMCharacter, Log, TEXT("LocalBeginCharge"));
	ServerRPCBeginCharge(GetWorld()->GetGameState()->GetServerWorldTimeSeconds());
}

void ASMPlayerCharacter::GrabSmash()
{
	if (!IsLocallyControlled())
	{
		return;
	}

	NET_LOG(LogSMCharacter, Log, TEXT("Current grab state: %d"), CurrentGrabState);
	ServerRPCBeginGrab(GetWorld()->GetGameState()->GetServerWorldTimeSeconds());
}

void ASMPlayerCharacter::ServerRPCBeginCharge_Implementation(const double LocalBeginTime)
{
	const double Latency = GetWorld()->GetTimeSeconds() - LocalBeginTime;
	const float InitGauge = FMath::Clamp(Latency, 0.0, MaxChargeGauge);
	ChargeGauge = InitGauge;
	CurrentGrabState = EGrabState::Charge;
	NET_LOG(LogSMCharacter, Log, TEXT("Server Begin charge: %f"), ChargeGauge);
}

void ASMPlayerCharacter::ServerRPCBeginGrab_Implementation(const double LocalBeginTime)
{
	const float ChargedGauge = ChargeGauge;
	NET_LOG(LogSMCharacter, Log, TEXT("Last charged gauge: %f"), ChargedGauge);
	SetCanControl(false);
	StartGrab(ChargedGauge);
}

void ASMPlayerCharacter::RangedAttack()
{
	if (CaughtCharacter)
	{
		return;
	}

	if (bCanRangedAttack)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("원거리 공격 시전"));
		bCanRangedAttack = false;

		FTimerHandle TimerHandle;
		const float Rate = 1.0f / DesignData->RangedAttackFiringRate;
		GetWorldTimerManager().SetTimer(TimerHandle, this, &ASMPlayerCharacter::CanRangedAttack, Rate);

		StoredSMAnimInstance->PlayRangedAttack();
		ServerRPCPlayRangedAttackAnimation();
	}
}

void ASMPlayerCharacter::ServerRPCPlayRangedAttackAnimation_Implementation()
{
	for (const ASMPlayerCharacter* CharacterToAnimation : GetCharactersExcludingServerAndCaster())
	{
		if (CharacterToAnimation)
		{
			CharacterToAnimation->ClientRPCPlayRangedAttackAnimation(this);
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlayRangedAttackAnimation_Implementation(
	const ASMPlayerCharacter* CharacterToAnimation) const
{
	if (CharacterToAnimation)
	{
		CharacterToAnimation->StoredSMAnimInstance->PlayRangedAttack();
	}
}

void ASMPlayerCharacter::CanRangedAttack()
{
	bCanRangedAttack = true;
}

void ASMPlayerCharacter::AnimNotify_RangedAttack()
{
	if (IsLocallyControlled())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("원거리 공격 시전"))

		ServerRPCShootProjectile(this);
	}
}

void ASMPlayerCharacter::ServerRPCShootProjectile_Implementation(ASMPlayerCharacter* NewOwner)
{
	const FVector SpawnLocation = GetActorLocation() + (GetActorForwardVector() * GetCapsuleComponent()->
		GetScaledCapsuleRadius() * 2);
	const FRotator SpawnRotation = GetActorRotation();
	const FTransform SpawnTransform = FTransform(SpawnRotation, SpawnLocation);
	ASMRangedAttackProjectile* CachedProjectile = GetWorld()->SpawnActorDeferred<ASMRangedAttackProjectile>(
		AssetData->RangedAttackProjectileClass, SpawnTransform);
	if (CachedProjectile)
	{
		CachedProjectile->SetOwningPawn(NewOwner);
	}

	UGameplayStatics::FinishSpawningActor(CachedProjectile, SpawnTransform);
}

void ASMPlayerCharacter::HitProjectile()
{
	NET_LOG(LogSMCharacter, Log, TEXT("\"%s\"가 투사체에 적중 당했습니다."), *GetName())

	if (CurrentState != EPlayerCharacterState::Smash)
	{
		Stat->AddCurrentPostureGauge(25.0f);
		NET_LOG(LogSMCharacter, Log, TEXT("현재 체간 게이지 %f / %f"), Stat->GetCurrentPostureGauge(),
		        Stat->GetBaseStat().MaxPostureGauge);
	}
}

void ASMPlayerCharacter::ResetTeamMaterial()
{
	const int32 TotalMaterialCount = GetMesh()->GetNumMaterials();

	switch (TeamComponent->GetCurrentTeam())
	{
	case ESMTeam::None:
		{
			break;
		}
	case ESMTeam::FutureBass:
		{
			for (int32 i = 0; i < TotalMaterialCount; ++i)
			{
				GetMesh()->SetMaterial(i, AssetData->FutureBassTeamMaterial);
			}

			break;
		}
	case ESMTeam::Rock:
		{
			for (int32 i = 0; i < TotalMaterialCount; ++i)
			{
				GetMesh()->SetMaterial(i, AssetData->RockTeamMaterial);
			}

			break;
		}
	}
}

void ASMPlayerCharacter::ServerRPCFutureBassTeamSelect_Implementation()
{
	TeamComponent->SetTeam(ESMTeam::FutureBass);
}

void ASMPlayerCharacter::ServerRPCRockTeamSelect_Implementation()
{
	TeamComponent->SetTeam(ESMTeam::Rock);
}
