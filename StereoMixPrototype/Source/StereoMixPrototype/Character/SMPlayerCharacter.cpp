// Fill out your copyright notice in the Description page of Project Settings.


#include "SMPlayerCharacter.h"

#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "SMCharacterAssetData.h"
#include "Animation/SMCharacterAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Log/SMLog.h"
#include "Net/UnrealNetwork.h"
#include "Physics/SMCollision.h"
#include "Player/AimPlane.h"
#include "Player/SMPlayerController.h"

ASMPlayerCharacter::ASMPlayerCharacter()
{
	bUseControllerRotationRoll = true;
	bUseControllerRotationPitch = true;
	bUseControllerRotationYaw = true;

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

	InitCamera();

	CurrentState = EPlayerCharacterState::Normal;
	bEnableCollision = true;
	bEnableMovement = true;
	bCanControl = true;

	// Design
	CatchTime = 0.25f;
	StandUpTime = 3.0f;
}

void ASMPlayerCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	StoredSMAnimInstance = CastChecked<USMCharacterAnimInstance>(GetMesh()->GetAnimInstance());
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

void ASMPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (IsLocallyControlled())
	{
		AimPlane = GetWorld()->SpawnActor<AAimPlane>();
		const FAttachmentTransformRules AttachmentTransformRules(EAttachmentRule::SnapToTarget, true);
		AimPlane->AttachToActor(this, AttachmentTransformRules);
	}

	InitCharacterControl();

	InitDesignData();
}

void ASMPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (EnhancedInputComponent)
	{
		EnhancedInputComponent->BindAction(AssetData->MoveAction, ETriggerEvent::Triggered, this, &ASMPlayerCharacter::Move);
		EnhancedInputComponent->BindAction(AssetData->JumpAction, ETriggerEvent::Triggered, this, &ASMPlayerCharacter::Jump);
		EnhancedInputComponent->BindAction(AssetData->HoldAction, ETriggerEvent::Started, this, &ASMPlayerCharacter::Catch);
		EnhancedInputComponent->BindAction(AssetData->SmashAction, ETriggerEvent::Started, this, &ASMPlayerCharacter::Smash);
	}
}

void ASMPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bCanControl)
	{
		UpdateRotateToMousePointer();
	}

	if (HasAuthority())
	{
		if (PullData.bIsPulling)
		{
			UpdatePerformPull(DeltaSeconds);
		}
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
	DOREPLIFETIME(ASMPlayerCharacter, TempStandUpCastCharacter);
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

void ASMPlayerCharacter::InitDesignData()
{
	PullData.TotalTime = CatchTime;
}

void ASMPlayerCharacter::InitCamera()
{
	const FRotator CameraRotation(-45.0f, 0.0, 0.0);
	const float CameraDistance = 750.0f;
	const float CameraFOV = 90.0f;

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
		UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer());
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

void ASMPlayerCharacter::SetCurrentState(EPlayerCharacterState InState)
{
	if (!HasAuthority())
	{
		return;
	}

	CurrentState = InState;
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

void ASMPlayerCharacter::SetCanControl(bool bInEnableControl)
{
	if (!HasAuthority())
	{
		return;
	}

	bCanControl = bInEnableControl;
	OnRep_bCanControl();
}

void ASMPlayerCharacter::OnRep_bCanControl()
{
	if (bCanControl)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("컨트롤 활성화"))
		EnableInput(StoredSMPlayerController);
		bUseControllerRotationYaw = true;
	}
	else
	{
		NET_LOG(LogSMCharacter, Log, TEXT("컨트롤 비활성화"))
		DisableInput(StoredSMPlayerController);
		bUseControllerRotationYaw = false;
	}
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

float ASMPlayerCharacter::DistanceHeightFromFloor()
{
	FHitResult HitResult;
	const FVector Start = GetActorLocation() + (-GetActorUpVector() * 90.5f);
	const FVector End = GetActorLocation() + (-GetActorUpVector() * 10000.0f);
	FCollisionObjectQueryParams CollisionObjectQueryParams;
	FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(DistanceHeigh), false, this);
	CollisionObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);
	const bool bSuccess = GetWorld()->LineTraceSingleByObjectType(HitResult, Start, End, CollisionObjectQueryParams, CollisionQueryParams);
	if (bSuccess)
	{
		const float Distance = (HitResult.Location - Start).Size();
		NET_LOG(LogSMCharacter, Warning, TEXT("Height: %f"), Distance);

		return Distance;
	}
	else
	{
		return 0.0f;
	}
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
	if (!CaughtCharacter)
	{
		StoredSMAnimInstance->PlayCatch();
		ServerRPCPlayCatchAnimation();
	}
}

void ASMPlayerCharacter::HandleCatch()
{
	if (IsLocallyControlled())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("잡기 시전"));

		// 충돌 로직
		FHitResult HitResult;
		const FVector Start = GetActorLocation();
		const FVector End = Start + (GetActorForwardVector() * 300.0f);
		FCollisionObjectQueryParams CollisionObjectQueryParams;
		CollisionObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);
		FCollisionQueryParams CollisionQueryParams(SCENE_QUERY_STAT(Hold), false, this);
		const bool bSuccess = GetWorld()->SweepSingleByObjectType(HitResult, Start, End, FQuat::Identity, CollisionObjectQueryParams, FCollisionShape::MakeSphere(50.0f), CollisionQueryParams);

		// 충돌 시
		if (bSuccess)
		{
			NET_LOG(LogSMCharacter, Log, TEXT("잡기 적중"));
			ASMPlayerCharacter* HitPlayerCharacter = Cast<ASMPlayerCharacter>(HitResult.GetActor());
			if (HitPlayerCharacter)
			{
				ServerRPCPerformPull(HitPlayerCharacter);
			}
		}

		// 디버거
		const FVector Center = Start + (End - Start) * 0.5f;
		const FColor DrawColor = bSuccess ? FColor::Green : FColor::Red;
		DrawDebugCapsule(GetWorld(), Center, 150.0f, 50.0f, FRotationMatrix::MakeFromZ(GetActorForwardVector()).ToQuat(), DrawColor, false, 1.0f);
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
	}
}

void ASMPlayerCharacter::OnRep_bEnableCollision()
{
	NET_LOG(LogSMCharacter, Log, TEXT("충돌 %s"), bEnableCollision ? TEXT("활성화") : TEXT("비활성화"));
	SetActorEnableCollision(bEnableCollision);
}

void ASMPlayerCharacter::ServerRPCPerformPull_Implementation(ASMPlayerCharacter* InTargetCharacter)
{
	NET_LOG(LogSMCharacter, Log, TEXT("당기기 시작"));

	// 클라이언트 제어권 박탈 및 충돌 판정 비활성화
	InTargetCharacter->SetAutonomousProxy(false);
	InTargetCharacter->SetCanControl(false);
	InTargetCharacter->SetEnableMovement(false);
	InTargetCharacter->SetEnableCollision(false);

	// 당기기에 필요한 데이터 할당
	InTargetCharacter->PullData.bIsPulling = true;
	InTargetCharacter->PullData.ElapsedTime = 0.0f;
	InTargetCharacter->PullData.Caster = this;
	InTargetCharacter->PullData.StartLocation = InTargetCharacter->GetActorLocation();
}

void ASMPlayerCharacter::UpdatePerformPull(float DeltaSeconds)
{
	if (HasAuthority())
	{
		// 시전자로부터 자신을 향한 방향
		FVector CasterVector = GetActorLocation() - PullData.Caster->GetActorLocation();
		CasterVector.Z = 0.0;
		const FVector CasterDirection = CasterVector.GetSafeNormal();

		// 캡슐 반지름을 통해 EndLocation이 시전자의 위치와 겹치지 않도록 오프셋 지정
		const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
		PullData.EndLocation = PullData.Caster->GetActorLocation() + (CasterDirection * CapsuleRadius * 2);

		// 선형 보간
		PullData.ElapsedTime += DeltaSeconds;
		const float Alpha = FMath::Clamp(PullData.ElapsedTime / PullData.TotalTime, 0.0f, 1.0f);
		const FVector NewLocation = FMath::Lerp(PullData.StartLocation, PullData.EndLocation, Alpha);
		SetActorLocation(NewLocation);

		// 디버거
		DrawDebugLine(GetWorld(), PullData.StartLocation, PullData.EndLocation, FColor::Cyan, false, 0.1f);

		if (Alpha >= 1.0f)
		{
			HandlePullEnd();
		}
	}
}

void ASMPlayerCharacter::HandlePullEnd()
{
	if (HasAuthority())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("당기기 종료"))
		PullData.bIsPulling = false;

		NET_LOG(LogSMCharacter, Log, TEXT("어태치 시작"))
		MulticastRPCAttachToCaster(PullData.Caster, this);
		StoredSMPlayerController->SetViewTargetWithBlend(PullData.Caster, 0.1f);
	}
}

void ASMPlayerCharacter::MulticastRPCAttachToCaster_Implementation(ASMPlayerCharacter* InCaster, ASMPlayerCharacter* InTarget)
{
	const FAttachmentTransformRules AttachmentTransformRules(EAttachmentRule::SnapToTarget, false);
	if (InCaster && InTarget)
	{
		// 플레이어의 상태를 잡힘으로 변경합니다.
		if (HasAuthority())
		{
			InCaster->SetCaughtCharacter(this);
		}

		InTarget->SetCurrentState(EPlayerCharacterState::Caught);
		InTarget->AttachToComponent(InCaster->GetMesh(), AttachmentTransformRules, TEXT("CatchSocket"));
		if (!HasAuthority())
		{
			InTarget->StoredSMAnimInstance->PlayCaught();
		}
	}
}

void ASMPlayerCharacter::ServerRPCPlayCatchAnimation_Implementation() const
{
	for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		if (!PlayerController->IsLocalController())
		{
			if (PlayerController != GetController())
			{
				const ASMPlayerCharacter* SMPlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn());
				SMPlayerCharacter->ClientRPCPlayCatchAnimation(this);
			}
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlayCatchAnimation_Implementation(const ASMPlayerCharacter* InPlayAnimationCharacter) const
{
	InPlayAnimationCharacter->StoredSMAnimInstance->PlayCatch();
}

void ASMPlayerCharacter::Smash()
{
	if (CaughtCharacter)
	{
		StoredSMAnimInstance->PlaySmash();
		ServerRPCPlaySmashAnimation();
	}
}

void ASMPlayerCharacter::ServerRPCPlaySmashAnimation_Implementation()
{
	SetCanControl(false);

	for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		if (!PlayerController)
		{
			continue;
		}

		if (!PlayerController->IsLocalController())
		{
			if (PlayerController != GetController())
			{
				const ASMPlayerCharacter* SMPlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn());
				SMPlayerCharacter->ClientRPCPlaySmashAnimation(this);
			}
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlaySmashAnimation_Implementation(const ASMPlayerCharacter* InPlayAnimationCharacter) const
{
	if (InPlayAnimationCharacter)
	{
		InPlayAnimationCharacter->StoredSMAnimInstance->PlaySmash();
	}
}

void ASMPlayerCharacter::HandleSmash()
{
	if (IsLocallyControlled())
	{
		NET_LOG(LogSMCharacter, Log, TEXT("스매시"));

		if (CaughtCharacter)
		{
			CaughtCharacter->StoredSMAnimInstance->PlayDownStart();

			// 서버에서 해당 액터의 위치를 받아오기전에 미리 이동시켜두고 나중에 받은 데이터로는 검증을 수행합니다.
			// 여기선 딜레이를 줄이기 위해 작성했습니다. 아니면 아예 앞으로 이런 로직은 시전자 클라에서 계산을 담당하고 서버로는 좌표값만 전송하고 반영하는 것도 염두 중입니다.
			const FVector DownLocation = GetActorLocation() + (GetActorForwardVector() * 150.0f);
			FRotator DownRotation = FRotationMatrix::MakeFromX(GetActorForwardVector()).Rotator();
			DownRotation.Pitch = 0.0;
			DownRotation.Roll = 0.0;
			CaughtCharacter->SetActorLocationAndRotation(DownLocation, DownRotation);

			ServerRPCDetachToCaster();
		}
	}
}

void ASMPlayerCharacter::ServerRPCDetachToCaster_Implementation()
{
	if (!HasAuthority())
	{
		return;
	}

	SetCanControl(true);
	// TempStandUpCastCharacter 버그:
	// 대상이 기상하기 전에 다른 타겟을 매치면 포인터가 옮겨지는 버그 있음
	TempStandUpCastCharacter = CaughtCharacter;
	MulticastRPCDetachToCaster(this, CaughtCharacter);
	
	FTimerHandle TimerHandle;
	GetWorldTimerManager().SetTimer(TimerHandle, this, &ASMPlayerCharacter::MulticastRPCHandleStandUp, StandUpTime);

	for (const APlayerController* PlayerController : TActorRange<APlayerController>(GetWorld()))
	{
		if (!PlayerController)
		{
			continue;
		}

		if (!PlayerController->IsLocalController())
		{
			if (PlayerController != GetController())
			{
				const ASMPlayerCharacter* PlayerCharacter = Cast<ASMPlayerCharacter>(PlayerController->GetPawn());
				if (PlayerCharacter)
				{
					PlayerCharacter->ClientRPCPlayDownStartAnimation(TempStandUpCastCharacter);
				}
			}
		}
	}
}

void ASMPlayerCharacter::MulticastRPCDetachToCaster_Implementation(ASMPlayerCharacter* InCaster, ASMPlayerCharacter* InTarget)
{
	if (InCaster && InTarget)
	{
		NET_LOG(LogSMCharacter, Log, TEXT("디태치 성공"));
		const FDetachmentTransformRules DetachmentTransformRules(EDetachmentRule::KeepWorld, false);
		InTarget->DetachFromActor(DetachmentTransformRules);

		if (HasAuthority())
		{
			SetCaughtCharacter(nullptr);

			InTarget->SetEnableCollision(true);
			InTarget->SetCurrentState(EPlayerCharacterState::Normal);
			InTarget->StoredSMPlayerController->SetViewTargetWithBlend(InTarget, 0.3f);

			const FVector DownLocation = InCaster->GetActorLocation() + (InCaster->GetActorForwardVector() * 150.0f);
			FRotator DownRotation = FRotationMatrix::MakeFromX(InCaster->GetActorForwardVector()).Rotator();
			DownRotation.Pitch = 0.0;
			DownRotation.Roll = 0.0;
			InTarget->SetAutonomousProxy(true);
			InTarget->SetActorLocationAndRotation(DownLocation, DownRotation);
			// InTarget->ClientRPCSetActorRotation(DownRotation);
			InTarget->GetController()->SetControlRotation(DownRotation);
			NET_LOG(LogSMCharacter, Log, TEXT("%s"), *DownRotation.ToString());
		}
	}
}

void ASMPlayerCharacter::ClientRPCPlayDownStartAnimation_Implementation(const ASMPlayerCharacter* InAnimationPlayCharacter) const
{
	if (InAnimationPlayCharacter)
	{
		InAnimationPlayCharacter->StoredSMAnimInstance->PlayDownStart();
	}
}

void ASMPlayerCharacter::MulticastRPCHandleStandUp_Implementation()
{
	if (!HasAuthority())
	{
		if (TempStandUpCastCharacter)
		{
			TempStandUpCastCharacter->StoredSMAnimInstance->PlayDownEnd();
		}
	}

	if (HasAuthority())
	{
		// TempStandUpCastCharacter->SetAutonomousProxy(true);
	}
}

void ASMPlayerCharacter::OnStandUpAnimationEnded()
{
	if (IsLocallyControlled())
	{
		ServerRPCOnStandUpEnd();
	}
}

void ASMPlayerCharacter::ClientRPCSetActorRotation_Implementation(FRotator InRotation)
{
	SetActorRotation(InRotation);
}

void ASMPlayerCharacter::ServerRPCOnStandUpEnd_Implementation()
{
	NET_LOG(LogSMCharacter, Warning, TEXT("Begin"));
	SetCanControl(true);
	SetEnableMovement(true);
}
