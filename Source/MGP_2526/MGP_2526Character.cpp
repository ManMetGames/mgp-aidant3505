// Copyright Epic Games, Inc. All Rights Reserved.

#include "MGP_2526Character.h"
#include "Engine/LocalPlayer.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/Controller.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "Kismet/KismetSystemLibrary.h"
#include "MGP_2526.h"

AMGP_2526Character::AMGP_2526Character()
{
	PrimaryActorTick.bCanEverTick = true;
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);
		
	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f);

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 500.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;
	GetCharacterMovement()->BrakingDecelerationFalling = 1500.0f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f;
	CameraBoom->bUsePawnControlRotation = true;

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void AMGP_2526Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMantling)
	{
		// While mantling is true, move the character toward the target positions
		TickMantle(DeltaTime);
	}
	else
	{
		// Cooldown between mantles
		if (MantleCooldownRemaining > 0.f)
		{
			MantleCooldownRemaining -= DeltaTime;
			return;
		}
		// Makes it so player can only mantle when on the ground
		if (GetCharacterMovement()->IsMovingOnGround())
		{
			DoMantleDetection();
		}
	}

}


void AMGP_2526Character::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent)) {
		
		// Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);

		// Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Move);
		EnhancedInputComponent->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Look);

		// Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AMGP_2526Character::Look);

		// Sprinting
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Started, this, &AMGP_2526Character::SprintStart);
		EnhancedInputComponent->BindAction(SprintAction, ETriggerEvent::Completed, this, &AMGP_2526Character::SprintStop);
	}
	else
	{
		UE_LOG(LogMGP_2526, Error, TEXT("'%s' Failed to find an Enhanced Input component! This template is built to use the Enhanced Input system. If you intend to use the legacy system, then you will need to update this C++ file."), *GetNameSafe(this));
	}
}

void AMGP_2526Character::Move(const FInputActionValue& Value)
{
	// Input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	// Route the input
	DoMove(MovementVector.X, MovementVector.Y);
}

void AMGP_2526Character::Look(const FInputActionValue& Value)
{
	// Input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	// Route the input
	DoLook(LookAxisVector.X, LookAxisVector.Y);
}

void AMGP_2526Character::DoMove(float Right, float Forward)
{
	// Disables movement if the player is mantling
	if (bIsMantling) return;

	if (GetController() != nullptr)
	{
		// Find out which way is forward
		const FRotator Rotation = GetController()->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// Get forward vector
		const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);

		// Get right vector 
		const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

		// Add movement 
		AddMovementInput(ForwardDirection, Forward);
		AddMovementInput(RightDirection, Right);
	}
}

void AMGP_2526Character::DoLook(float Yaw, float Pitch)
{
	if (GetController() != nullptr)
	{
		// Add yaw and pitch input to controller
		AddControllerYawInput(Yaw);
		AddControllerPitchInput(Pitch);
	}
}

void AMGP_2526Character::DoJumpStart()
{
	// Signal the character to jump
	Jump();
}

void AMGP_2526Character::DoJumpEnd()
{
	// Signal the character to stop jumping
	StopJumping();
}

void AMGP_2526Character::SprintStart()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = SprintSpeed;
	}
	bIsSprinting = true;
}

void AMGP_2526Character::SprintStop()
{
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
	}
	bIsSprinting = false;
}

// Mantle detection
void AMGP_2526Character::DoMantleDetection()
{
	UCapsuleComponent* Capsule = GetCapsuleComponent();
	if (!Capsule) return;

	const FVector CapsuleOrigin = Capsule->GetComponentLocation();
	const FVector ForwardVector = Capsule->GetForwardVector();

	// Lower trace  
	const FVector LowerStart = CapsuleOrigin + FVector(0.f, 0.f, LowerTraceHeightOffset);
	const FVector LowerEnd = LowerStart + ForwardVector * TraceDistance;

	FHitResult LowerHit;
	const bool bLowerHit = FireMantleTrace(LowerStart, LowerEnd, LowerHit);

	// Upper trace
	const FVector UpperStart = CapsuleOrigin + FVector(0.f, 0.f, UpperTraceHeightOffset);
	const FVector UpperEnd = UpperStart + ForwardVector * TraceDistance;

	FHitResult UpperHit;
	const bool bUpperHit = FireMantleTrace(UpperStart, UpperEnd, UpperHit);

	// Checks if player has met the conditions to be able to mantle
	if (bLowerHit && !bUpperHit && bIsSprinting)
	{
		// Fires a downward trace infront of the player and above the wall to find ledge surface Z (height)
		const FVector DownStart = FVector(
			LowerHit.ImpactPoint.X + ForwardVector.X * 5.f,   
			LowerHit.ImpactPoint.Y + ForwardVector.Y * 5.f,
			CapsuleOrigin.Z + DownTraceStartHeight
		);
		const FVector DownEnd = FVector(DownStart.X, DownStart.Y, CapsuleOrigin.Z - 100.f);

		FHitResult LedgeHit;
		if (FireMantleTrace(DownStart, DownEnd, LedgeHit))
		{
			StartMantle(LowerHit.ImpactPoint, LedgeHit.ImpactPoint.Z);
		}
	}
}

// Function to shoot out traces from player 
bool AMGP_2526Character::FireMantleTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(const_cast<AMGP_2526Character*>(this));

	return UKismetSystemLibrary::LineTraceSingle(
		const_cast<AMGP_2526Character*>(this),
		Start,
		End,
		UEngineTypes::ConvertToTraceType(ECC_Visibility),
		false,                                                    
		ActorsToIgnore,
		EDrawDebugTrace::ForOneFrame,
		OutHit,
		true
	);
}

void AMGP_2526Character::StartMantle(const FVector& WallHitLocation, float LedgeZ)
{
	// Calculates the posistions the character should go to
	const FVector CapsuleOrigin = GetCapsuleComponent()->GetComponentLocation();
	const FVector ForwardVector = GetCapsuleComponent()->GetForwardVector();
	const float CapsuleHalfHeight = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float TargetZ = LedgeZ + CapsuleHalfHeight + MantleBuffer;

	// Sets the height pos the player should rise to based off ledge height
	MantleRiseTarget = FVector(CapsuleOrigin.X, CapsuleOrigin.Y, TargetZ);

	// Sets the forward pos the player should move to based off forward offset
	MantleForwardTarget = FVector(
		WallHitLocation.X + ForwardVector.X * MantleForwardOffset,
		WallHitLocation.Y + ForwardVector.Y * MantleForwardOffset,
		TargetZ
	);

	// Starts in the rise player phase
	MantleTargetLocation = MantleRiseTarget;
	bMantleRising = true;
	bIsMantling = true;

	// Switch to Flying so gravity doesn't affect the player
	GetCharacterMovement()->SetMovementMode(MOVE_Flying);
	GetCharacterMovement()->Velocity = FVector::ZeroVector;

	// Plays mantle animation
	if (MantleMontage)
	{
		PlayAnimMontage(MantleMontage);
	}
}

void AMGP_2526Character::TickMantle(float DeltaTime)
{
	const FVector CurrentLocation = GetActorLocation();
	const FVector NewPos = FMath::VInterpTo(CurrentLocation, MantleTargetLocation, DeltaTime, MantleInterpSpeed);

	SetActorLocation(NewPos, false);

	// Checks if the pos of the player is within range of current mantle target pos 
	if (FVector::Dist(NewPos, MantleTargetLocation) < MantleCompletionRadius)
	{
		if (bMantleRising)
		{
			// Rise phase done, Start moving player forward
			bMantleRising = false;
			MantleTargetLocation = MantleForwardTarget;
		}
		else
		{
			// Forward phase done
			SetActorLocation(MantleForwardTarget);
			bIsMantling = false;
			MantleCooldownRemaining = MantleCooldownDuration;
			GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		}
	}
}


