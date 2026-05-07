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

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

void AMGP_2526Character::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bIsMantling)
	{
		// While mantling, drive the character toward the target and skip detection
		TickMantle(DeltaTime);
	}
	else
	{
		if (MantleCooldownRemaining > 0.f)
		{
			MantleCooldownRemaining -= DeltaTime;
			return;
		}
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
		StartMantle(LowerHit.ImpactPoint);
	}
}


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
		EDrawDebugTrace::None,
		OutHit,
		true                                                     
	);
}

void AMGP_2526Character::StartMantle(const FVector& WallHitLocation)
{
	// Calculates the posistions the character should go to
	const FVector CapsuleOrigin = GetCapsuleComponent()->GetComponentLocation();
	const FVector ForwardVector = GetCapsuleComponent()->GetForwardVector();
	const float   LedgeHeight = CapsuleOrigin.Z + UpperTraceHeightOffset + MantleBuffer;

	// Rises up to hight of the ledge
	MantleRiseTarget = FVector(CapsuleOrigin.X, CapsuleOrigin.Y, LedgeHeight);

	// Steps forward onto the ledge from the risen position
	MantleForwardTarget = FVector(
		WallHitLocation.X + ForwardVector.X * MantleForwardOffset,
		WallHitLocation.Y + ForwardVector.Y * MantleForwardOffset,
		LedgeHeight
	);

	// Starts in the rise phase
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


