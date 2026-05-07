// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Logging/LogMacros.h"
#include "MGP_2526Character.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputAction;
struct FInputActionValue;

DECLARE_LOG_CATEGORY_EXTERN(LogTemplateCharacter, Log, All);

/**
 *  A simple player-controllable third person character
 *  Implements a controllable orbiting camera
 */
UCLASS(abstract)
class AMGP_2526Character : public ACharacter
{
	GENERATED_BODY()

	/** Camera boom positioning the camera behind the character */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	USpringArmComponent* CameraBoom;

	/** Follow camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta = (AllowPrivateAccess = "true"))
	UCameraComponent* FollowCamera;
	
protected:

	/** Jump Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* JumpAction;

	/** Move Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MoveAction;

	/** Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* LookAction;

	/** Mouse Look Input Action */
	UPROPERTY(EditAnywhere, Category="Input")
	UInputAction* MouseLookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	UInputAction* SprintAction;

	/** Movement Speeds */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float WalkSpeed = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Movement")
	float SprintSpeed = 600.f;

	/** Mantle Traces */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Trace")
	float TraceDistance = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Trace")
	float LowerTraceHeightOffset = -20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Trace")
	float UpperTraceHeightOffset = 10.f;

	/** Mantle Movement */
	// How fast the character moves to the mantle target
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Movement")
	float MantleInterpSpeed = 15.f;

	// How far forward past the edge of the ledge the character is placed
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Movement")
	float MantleForwardOffset = 30.f;

	// Distance threshold at which the mantle is considered complete 
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Movement")
	float MantleCompletionRadius = 5.f;

	// How long after a mantle completes before detection re-enables (seconds)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Movement")
	float MantleCooldownDuration = 1.f;
	float MantleCooldownRemaining = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Movement")
	float MantleBuffer = 80.f;

	/** Mantle Animation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mantle|Animation")
	UAnimMontage* MantleMontage = nullptr;


public:

	/** Constructor */
	AMGP_2526Character();	

	/** Called every frame */
	virtual void Tick(float DeltaTime) override;


protected:

	/** Initialize input action bindings */
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

protected:

	/** Called for movement input */
	void Move(const FInputActionValue& Value);

	/** Called for looking input */
	void Look(const FInputActionValue& Value);

public:

	/** Handles move inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoMove(float Right, float Forward);

	/** Handles look inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoLook(float Yaw, float Pitch);

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpStart();

	/** Handles jump pressed inputs from either controls or UI interfaces */
	UFUNCTION(BlueprintCallable, Category="Input")
	virtual void DoJumpEnd();

	/** Handles sprint input */
	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void SprintStart();

	UFUNCTION(BlueprintCallable, Category = "Input")
	virtual void SprintStop();

	/** Returns true while the character is mid-mantle. Used for animation */
	UFUNCTION(BlueprintPure, Category = "Mantle")
	bool IsMantling() const { return bIsMantling; }


public:

	/** Returns CameraBoom subobject */
	FORCEINLINE class USpringArmComponent* GetCameraBoom() const { return CameraBoom; }

	/** Returns FollowCamera subobject */
	FORCEINLINE class UCameraComponent* GetFollowCamera() const { return FollowCamera; }

private:

	/**Fires the lower and upper traces each tick */
	void DoMantleDetection();

	/** Fires a single line trace and returns true on a hit */
	bool FireMantleTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	/** Called once when a mantleable ledge is first detected */
	void StartMantle(const FVector& WallHitLocation);

	/** Called every tick while bIsMantling is true */
	void TickMantle(float DeltaTime);

	// Runtime mantle states
	bool bMantleRising = false;
	bool bIsMantling = false;

	// Runtime sprint state
	bool bIsSprinting = false;

	// Mantle target positions
	FVector MantleTargetLocation = FVector::ZeroVector;
	FVector MantleRiseTarget = FVector::ZeroVector;
	FVector MantleForwardTarget = FVector::ZeroVector;
};

