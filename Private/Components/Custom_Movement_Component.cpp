// Fill out your copyright notice in the Description page of Project Settings.


#include "Components/Custom_Movement_Component.h"
#include "Debug/DebugHelper.h"
#include "Components/CapsuleComponent.h"
#include "Character/Technical_Animator_Character.h"
#include "Kismet/KismetMathLibrary.h"
#include "MotionWarpingComponent.h"
#include "Engine/World.h"
//#include "Gameplay_Tags/Gameplay_Tags.h"
#include "Native_Gameplay_Tags/Native_Gameplay_Tags.h"
#include "DrawDebugHelpers.h"
#include "Character_Direction/Character_Direction_Arrow.h"
#include "Data_Asset/Parkour_Action_Data.h"
#include "Kismet/GameplayStatics.h"
#include "Player_Controller/Player_Controller.h"
#include "World_Actors/Wall_Pipe_Actor.h"
#include "Net/UnrealNetwork.h"
#include "World_Actors/Balance_Traversal_Actor.h"
#include "World_Actors/Wall_Vault_Actor.h"
#include "World_Actors/Tic_Tac_Actor.h"

UCustom_Movement_Component::UCustom_Movement_Component()
{
	//Set this component to replicate for network compatibility.
	SetIsReplicatedByDefault(true);
}

void UCustom_Movement_Component::BeginPlay()
{
	Super::BeginPlay();
	
	Owning_Player_Animation_Instance = CharacterOwner->GetMesh()->GetAnimInstance();

	if(Owning_Player_Animation_Instance)
	{
		Owning_Player_Animation_Instance->OnMontageEnded.AddDynamic(this, &UCustom_Movement_Component::On_Climbing_Montage_Ended);
		Owning_Player_Animation_Instance->OnMontageBlendingOut.AddDynamic(this, &UCustom_Movement_Component::On_Climbing_Montage_Ended);

		Owning_Player_Animation_Instance->OnMontageEnded.AddDynamic(this, &UCustom_Movement_Component::On_Take_Cover_Montage_Ended);
		Owning_Player_Animation_Instance->OnMontageBlendingOut.AddDynamic(this, &UCustom_Movement_Component::On_Take_Cover_Montage_Ended);
	}

	Owning_Player_Character = Cast<ATechnical_Animator_Character>(CharacterOwner);

}

void UCustom_Movement_Component::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const FVector Unrotated_Last_Input_Vector = 
	UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), GetLastInputVector());

	//Debug::Print(Unrotated_Last_Input_Vector.GetSafeNormal().ToCompactString(), FColor:: Cyan, 9);

	Parkour_Call_In_Tick();
}

void UCustom_Movement_Component::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);

	if(Is_Climbing())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
		On_Enter_Climb_State_Delegate.ExecuteIfBound();
	}

	else if(PreviousMovementMode == MOVE_Custom && PreviousCustomMode == E_Custom_Movement_Mode::MOVE_Climb)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator Dirty_Rotation = UpdatedComponent->GetComponentRotation();
		const FRotator Clean_Stand_Rotation = FRotator(0.f, Dirty_Rotation.Yaw, 0.f);
		UpdatedComponent->SetRelativeRotation(Clean_Stand_Rotation);

		StopMovementImmediately();

		On_Exit_Climb_State_Delegate.ExecuteIfBound();
	}

	if(Is_Taking_Cover())
	{
		bOrientRotationToMovement = false;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
		CharacterOwner->bIsCrouched = true;
		On_Enter_Take_Cover_State_Delegate.ExecuteIfBound();
	}

	else if(PreviousMovementMode == MOVE_Custom && PreviousCustomMode == E_Custom_Movement_Mode::MOVE_Take_Cover)
	{
		bOrientRotationToMovement = true;
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(96.f);

		const FRotator Dirty_Rotation = UpdatedComponent->GetComponentRotation();
		const FRotator Clean_Stand_Rotation = FRotator(0.f, Dirty_Rotation.Yaw, 0.f);
		UpdatedComponent->SetRelativeRotation(Clean_Stand_Rotation);

		StopMovementImmediately();

		On_Exit_Take_Cover_State_Delegate.ExecuteIfBound();
	}

}

void UCustom_Movement_Component::PhysCustom(float deltaTime, int32 Iterations)
{
	Super::PhysCustom(deltaTime, Iterations);

	if(Is_Climbing())
	{
		Physics_Climb(deltaTime, Iterations);
	}
	
	if(Is_Taking_Cover())
	{
		Physics_Take_Cover(deltaTime, Iterations);
	}
}

float UCustom_Movement_Component::GetMaxSpeed() const
{
    if(Is_Climbing())
	{
		return Max_Climb_Speed;
	}

	else
	{
		return Super:: GetMaxSpeed();
	}
	
	if(Is_Taking_Cover())
	{
		return Max_Take_Cover_Speed;
	}

	else
	{
		return Super:: GetMaxSpeed();
	}
}

float UCustom_Movement_Component::GetMaxAcceleration() const
{
    if(Is_Climbing())
	{
		return Max_Climb_Acceleration;
	}

	else if(!Is_Climbing())
	{
		return Super:: GetMaxAcceleration();
	}

	if(Is_Taking_Cover())
	{
		return Max_Take_Cover_Acceleration;
	}

	else
	{
		return Super:: GetMaxAcceleration();
	}


	/*if(Is_Climbing() || Is_Taking_Cover())
	{
		if(Is_Climbing())
		{
			return Max_Climb_Acceleration;
		}
		else if(Is_Taking_Cover())
		{
			return Max_Take_Cover_Acceleration;
		}	
	}
	else
	{	
		return Super:: GetMaxAcceleration();
	}*/
}

FVector UCustom_Movement_Component::ConstrainAnimRootMotionVelocity(const FVector &RootMotionVelocity, const FVector &CurrentVelocity) const
{	
	const bool bIs_Playing_RM_Montage =
    IsFalling() && Owning_Player_Animation_Instance && Owning_Player_Animation_Instance->IsAnyMontagePlaying();

	if(bIs_Playing_RM_Montage)
	{
		return RootMotionVelocity;	
	}

	else
	{
		return Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity,CurrentVelocity);
	}
}


#pragma region Climb_Region


#pragma region Climb_Traces

TArray<FHitResult> UCustom_Movement_Component::DoCapsuleTraceMultiByObject(const FVector& Start, const FVector& End, bool B_Show_Debug_Shape, bool bDrawPersistantShapes)
{
	TArray<FHitResult> OutCapsuleTraceHitResults;

	EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;

	if(B_Show_Debug_Shape)
	{
		DebugTraceType = EDrawDebugTrace::ForOneFrame;

		if(bDrawPersistantShapes)
		{
			DebugTraceType = EDrawDebugTrace::Persistent;
		}
	}
	
	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		Start,
		End,
		Climb_Capsule_Trace_Radius,
		Climb_Capsule_Trace_Half_Height,
		Climable_Surface_Trace_Types,
		false,
		TArray<AActor*>(),
		DebugTraceType,
		OutCapsuleTraceHitResults,
		false
	);

	return OutCapsuleTraceHitResults;
}

FHitResult UCustom_Movement_Component::Do_Line_Trace_Single_By_Object(const FVector& Start, const FVector& End, bool B_Show_Debug_Shape, bool bDrawPersistantShapes)
{
	FHitResult OutHit;

	EDrawDebugTrace::Type DebugTraceType = EDrawDebugTrace::None;

	if(B_Show_Debug_Shape)
	{
		DebugTraceType = EDrawDebugTrace::ForOneFrame;

		if(bDrawPersistantShapes)
		{
			DebugTraceType = EDrawDebugTrace::Persistent;
		}
	}
	
	UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		Start,
		End,
		Climable_Surface_Trace_Types,
		false,
		TArray<AActor*>(),
		DebugTraceType,
		OutHit,
		false
	);

	return OutHit;
}
#pragma endregion

#pragma region Climb_Core

void UCustom_Movement_Component::Toggle_Climbing(bool B_Eneble_Climb)
{
	if(B_Eneble_Climb)
	{
		if(Can_Start_Climbing())
		{
			//Enter Climb The State
			Debug::Print(TEXT("Can Start Climbing"));
			Play_Climbing_Montage(Idle_To_Climb_Montage);
		}

		else if (Can_Climb_Down_Ledge())
		{
			Play_Climbing_Montage(Climb_Down_Ledge_Montage);
			Debug::Print(TEXT("Can Climb Down"), FColor::Cyan, 4);
		}

		else
		{
			Try_Start_Vaulting();
			Debug::Print(TEXT("Can't Climb Down"), FColor::Green, 8);
		}

	}

	if(!B_Eneble_Climb)
	{
		//Stop Climbing
		Stop_Climbing();
	}

}

bool UCustom_Movement_Component::Can_Start_Climbing()
{
    if(IsFalling()) return false;
	if(!Trace_Climbable_Surfaces()) return false;
	if(!Trace_From_Eye_Height(100.f).bBlockingHit) return false;

	return true;
}

bool UCustom_Movement_Component::Can_Climb_Down_Ledge()
{
    if(IsFalling()) return false;

	const FVector Component_Loation = UpdatedComponent->GetComponentLocation();
	const FVector Component_Forward_Vector = UpdatedComponent->GetForwardVector();
	const FVector Component_Down_Vector = -UpdatedComponent->GetUpVector();

	const FVector Walkable_Surface_Trace_Start = Component_Loation + Component_Forward_Vector * Climb_Down_Walkable_Surface_Trace_Offset;
	const FVector Walkable_Surface_Trace_End = Walkable_Surface_Trace_Start + Component_Down_Vector * 100.f;

	FHitResult Walkable_Surface_Hit = Do_Line_Trace_Single_By_Object(Walkable_Surface_Trace_Start, Walkable_Surface_Trace_End, 1, 0);

	const FVector Ledge_Trace_Start = Walkable_Surface_Hit.TraceStart + Component_Forward_Vector * Climb_Down_Ledge_Trace_Offset;
	const FVector Ledge_Trace_End = Ledge_Trace_Start + Component_Down_Vector * 230.f;

	FHitResult Ledge_Trace_Hit = Do_Line_Trace_Single_By_Object(Ledge_Trace_Start, Ledge_Trace_End, 1, 0);

	if(Walkable_Surface_Hit.bBlockingHit && !Ledge_Trace_Hit.bBlockingHit)
	{
		return true;
	}

	/*if(!Walkable_Surface_Hit.bBlockingHit && !Ledge_Trace_Hit.bBlockingHit)
	{
		return true;
	}*/

	return false;
}

void UCustom_Movement_Component::Start_Climbing()
{
	SetMovementMode(MOVE_Custom, E_Custom_Movement_Mode::MOVE_Climb);
}

void UCustom_Movement_Component::Stop_Climbing()
{
	SetMovementMode(MOVE_Falling);
}

void UCustom_Movement_Component::Physics_Climb(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	/*Process all climable surfaces information*/

	Trace_Climbable_Surfaces();

	Process_Climbable_Surface_Info();

	if(Check_Has_Reached_Floor())
	{
		Debug::Print(TEXT("Has Reached Floor"), FColor::Yellow, 5);
	}

	else
	{
		Debug::Print(TEXT("Floor Not Reached"), FColor::Green, 7);
	}

	/*Check if we should stop climbing*/
	if(Check_Should_Stop_Climbing() || Check_Has_Reached_Floor())
	{
		Stop_Climbing();
	}


	RestorePreAdditiveRootMotionVelocity();

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{	/*Define the max climb speed and acceleration*/
		CalcVelocity(deltaTime, 0.f, true, Max_Climb_Speed);
	}

	ApplyRootMotionToVelocity(deltaTime);


	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	
	/*Handle the climbing rotation*/
	SafeMoveUpdatedComponent(Adjusted, Get_Climb_Rotation(deltaTime), true, Hit);

	if (Hit.Time < 1.f)
	{
		//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	if(!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	/*Snap movement to climable surfaces*/
	Snap_Movement_To_Climbable_Surfaces(deltaTime);

	if(Check_Has_Has_Reached_Ledge())
	{	
		Play_Climbing_Montage(Climb_To_Top_Montage);
		Debug::Print(TEXT("Top of Surface Reached"), FColor::Yellow, 3);
	}
	else
	{
		Debug::Print(TEXT("Top of Surface Not Reached"), FColor::Emerald, 4);
	}
}

void UCustom_Movement_Component::Process_Climbable_Surface_Info()
{
	Current_Climbable_Surface_Location = FVector::ZeroVector;
	Current_Climbable_Surface_Normal = FVector::ZeroVector;

	if(Climable_Surfaces_Traced_Results.IsEmpty()) return;

	for(const FHitResult& TracedHitResult:Climable_Surfaces_Traced_Results)
	{
		Current_Climbable_Surface_Location += TracedHitResult.ImpactPoint;
		Current_Climbable_Surface_Normal += TracedHitResult.ImpactNormal;
	}

	Current_Climbable_Surface_Location /= Climable_Surfaces_Traced_Results.Num();
	Current_Climbable_Surface_Normal = Current_Climbable_Surface_Normal.GetSafeNormal();

	Debug::Print(TEXT("Climbable_Surface_Location:") + Current_Climbable_Surface_Location.ToCompactString(), FColor::Cyan, 1);
	Debug::Print(TEXT("Climbable_Surface_Normal:") + Current_Climbable_Surface_Normal.ToCompactString(), FColor::Red, 2);
}

bool UCustom_Movement_Component::Check_Should_Stop_Climbing()
{
    if(Climable_Surfaces_Traced_Results.IsEmpty()) return true;

	const float Dot_Result = FVector::DotProduct(Current_Climbable_Surface_Normal, FVector::UpVector);
	const float Degree_Difference = FMath::RadiansToDegrees(FMath::Acos(Dot_Result));

	if(Degree_Difference <= 59.f)
	{
		return true;
	}

	Debug::Print(TEXT("Degree Difference:") + FString::SanitizeFloat(Degree_Difference), FColor::Cyan, 1);

	return false;
}

bool UCustom_Movement_Component::Check_Has_Reached_Floor()
{
    const FVector Down_Vector = -UpdatedComponent->GetUpVector();
	const FVector Start_Offset = Down_Vector * 50.f;

	const FVector Start = UpdatedComponent->GetComponentLocation() + Start_Offset;
	const FVector End = Start + Down_Vector;

	TArray<FHitResult> Possible_Floor_Hits = DoCapsuleTraceMultiByObject(Start, End, 0, 0);

	if(Possible_Floor_Hits.IsEmpty()) return false;

	for(const FHitResult& Possible_Floor_Hit:Possible_Floor_Hits)
	{
		const bool bFloorReached = 
		FVector::Parallel(-Possible_Floor_Hit.ImpactNormal, FVector::UpVector) &&
		Get_Unrotated_Climb_Velocity().Z<-10.f;

		if(bFloorReached)
		{
			return true;
		}
	}

	return false;
}

FQuat UCustom_Movement_Component::Get_Climb_Rotation(float DeltaTime)
{
    const FQuat Current_Quat = UpdatedComponent->GetComponentQuat();
	
	if(HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return Current_Quat;
	}
	
	const FQuat Target_Quat = FRotationMatrix::MakeFromX(-Current_Climbable_Surface_Normal).ToQuat();

	return FMath::QInterpTo(Current_Quat, Target_Quat, DeltaTime, 5.f);
	
}

void UCustom_Movement_Component::Snap_Movement_To_Climbable_Surfaces(float DeltaTime)
{
	const FVector Component_Forward = UpdatedComponent->GetForwardVector();
	const FVector Component_Location = UpdatedComponent->GetComponentLocation();

	const FVector Projected_Character_To_Surface = 
	(Current_Climbable_Surface_Location - Component_Location).ProjectOnTo(Component_Forward);

	const FVector Snap_Vector = -Current_Climbable_Surface_Normal * Projected_Character_To_Surface.Length();

	UpdatedComponent->MoveComponent(
	Snap_Vector * DeltaTime * Max_Climb_Speed,
	UpdatedComponent->GetComponentQuat(),
	true);
}

bool UCustom_Movement_Component::Check_Has_Has_Reached_Ledge()
{
	FHitResult Ledge_Hit_Result = Trace_From_Eye_Height(100.f, 50.f);

	if(!Ledge_Hit_Result.bBlockingHit)
	{
		const FVector Walkable_Surface_Trace_Start = Ledge_Hit_Result.TraceEnd;
		
		const FVector Down_Vector = -UpdatedComponent->GetUpVector();

		const FVector Walkable_Surface_Trace_End = Walkable_Surface_Trace_Start + Down_Vector * 100.f;
		
		FHitResult Walkable_Surface_Hit_Result =
		Do_Line_Trace_Single_By_Object(Walkable_Surface_Trace_Start, Walkable_Surface_Trace_End, 1, 0);

		if(Walkable_Surface_Hit_Result.bBlockingHit && Get_Unrotated_Climb_Velocity().Z > 10.f)
		{
			return true;
			//Use dot product to check the surface normal and or check if it's parallel with up vector.
		}
	}
	return false;
}

void UCustom_Movement_Component::Try_Start_Vaulting()
{
	FVector Vault_Start_Position;
	FVector Vault_End_Position;
	if(Can_Start_Vaulting(Vault_Start_Position, Vault_End_Position))
	{
		//Start Vaulting
		Set_Motion_Warping_Target(FName("Vault_Start_Position"), Vault_Start_Position);
		Set_Motion_Warping_Target(FName("Vault_End_Position"), Vault_End_Position);
		Debug::Print(TEXT("Vault Start position:") + Vault_Start_Position.ToCompactString());
		Debug::Print(TEXT("ault_End_Position:") + Vault_End_Position.ToCompactString());

		Start_Climbing();
		Play_Climbing_Montage(Vaulting_Montage);
	}

	else
	{
		Debug::Print(TEXT("Unable To Vault"));
	}
}

bool UCustom_Movement_Component::Can_Start_Vaulting(FVector& Out_Vault_Start_Position, FVector& Out_Vault_Land_Position)
{
    if(IsFalling()) return false;
	Out_Vault_Start_Position = FVector::ZeroVector;
	Out_Vault_Land_Position = FVector::ZeroVector;
	const FVector Component_Location = UpdatedComponent->GetComponentLocation();
	const FVector Component_Forward_Vector = UpdatedComponent->GetForwardVector();
	const FVector Component_Up_Vector = UpdatedComponent->GetUpVector();
	const FVector Component_Down_Vector = -UpdatedComponent->GetUpVector();

	for(int32 i = 0; i < 5; i++)
	{
		const FVector Start = Component_Location + Component_Up_Vector * 100.f + 
		Component_Forward_Vector * 100.f * (i+1);

		const FVector End = Start + Component_Down_Vector * 100.f * (i+1);

		FHitResult Vault_Trace_Hit = Do_Line_Trace_Single_By_Object(Start, End, 1, 0);

		if(i == 0 && Vault_Trace_Hit.bBlockingHit)
		{
			Out_Vault_Start_Position = Vault_Trace_Hit.ImpactPoint;
		}

		if(i == 4)
		{
			Out_Vault_Land_Position = Vault_Trace_Hit.ImpactPoint;
		}
	}

	if(Out_Vault_Start_Position != FVector::ZeroVector && Out_Vault_Land_Position != FVector::ZeroVector)
	{
		return true;
	}

	else
	{
		return false;
	}
}

bool UCustom_Movement_Component::Is_Climbing() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == E_Custom_Movement_Mode::MOVE_Climb;
}

//Trace for climable surfaces return true if there are valid surfaces, otherwise return false.
bool UCustom_Movement_Component::Trace_Climbable_Surfaces()
{
	const FVector Start_Offset = UpdatedComponent->GetForwardVector() * 30.f;
	
	const FVector Start = UpdatedComponent->GetComponentLocation() + Start_Offset;
	
	const FVector End = Start + UpdatedComponent->GetForwardVector();
	
	Climable_Surfaces_Traced_Results = DoCapsuleTraceMultiByObject(Start, End, 0);

	return !Climable_Surfaces_Traced_Results.IsEmpty();
}

FHitResult UCustom_Movement_Component::Trace_From_Eye_Height(float Trace_Distance, float Trace_Start_Offset, bool B_Show_Debug_Shape, bool bDrawPersistantShapes)
{
	const FVector Component_Location = UpdatedComponent->GetComponentLocation();
	
	const FVector Eye_Height_Offset = UpdatedComponent->GetUpVector() * (CharacterOwner->BaseEyeHeight + Trace_Start_Offset);
	
	const FVector Start = Component_Location + Eye_Height_Offset;
	
	const FVector End = Start + UpdatedComponent->GetForwardVector() * Trace_Distance;

	return Do_Line_Trace_Single_By_Object(Start, End, B_Show_Debug_Shape, bDrawPersistantShapes);
}

void UCustom_Movement_Component::Play_Climbing_Montage(UAnimMontage *MontageToPlay)
{
	if(!MontageToPlay) return;
	if(!Owning_Player_Animation_Instance) return;
	if(Owning_Player_Animation_Instance->IsAnyMontagePlaying()) return;

	Owning_Player_Animation_Instance->Montage_Play(MontageToPlay);
}

void UCustom_Movement_Component::On_Climbing_Montage_Ended(UAnimMontage* Montage, bool bInterrupted)
{
	if(Montage == Idle_To_Climb_Montage || Montage == Climb_Down_Ledge_Montage)
	{
		Start_Climbing();
		StopMovementImmediately();
	}

	if (Montage == Climb_To_Top_Montage || Montage == Vaulting_Montage)
	{
		SetMovementMode(MOVE_Walking);
	}
	
	return Debug::Print(TEXT("Montage is Working"));
}

void UCustom_Movement_Component::Request_Hopping()
{
	const FVector Unrotated_Last_Input_Vector = 
	UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), GetLastInputVector());

	const float Dot_Product_Result = 
	FVector::DotProduct(Unrotated_Last_Input_Vector.GetSafeNormal(), FVector::UpVector);

	const FVector Component_Velocity = UpdatedComponent->GetComponentVelocity();
	const FVector Right_Vector = UpdatedComponent->GetRightVector();
	 
	const float Dot_Product_Result_2 = FVector::DotProduct(Velocity.GetSafeNormal(), Right_Vector); 

	Debug::Print(TEXT("Dot Product Result") + FString::SanitizeFloat(Dot_Product_Result));

	if(Dot_Product_Result > 0.9f)
	{
		Handle_Hop_Hop();
		Debug::Print(TEXT("Hopping Up"));
	}

	else if(Dot_Product_Result <= -0.9f)
	{
		Handle_Hop_Down();
		Debug::Print(TEXT("Hopping Down"));
	}

	if(Dot_Product_Result_2 > 0.9f)
	{
		Handle_Hop_Right();
		Debug::Print(TEXT("Hopping Right"));
	}

	else if(Dot_Product_Result_2 <= -0.9f)
	{
		Handle_Hop_Left();
		Debug::Print(TEXT("Hopping Left"));
	}

	else
	{
		Debug::Print(TEXT("Desired Directional Hopping Has Yet To Be Implemented"));
	}

}

void UCustom_Movement_Component::Set_Motion_Warping_Target(const FName &In_Warping_Target_Name, const FVector &In_Target_Position)
{
	if(!Owning_Player_Character) return;

	Owning_Player_Character->Get_Motion_Warping_Component()->AddOrUpdateWarpTargetFromLocation(
		In_Warping_Target_Name,
		In_Target_Position
	);
}

void UCustom_Movement_Component::Handle_Hop_Hop()
{
	FVector Hop_Up_Target_Point;
	if(bCheck_Can_Hop_Up(Hop_Up_Target_Point))
	{
		Set_Motion_Warping_Target(FName("Hop_Up_Target_Point"), Hop_Up_Target_Point);
		
		Play_Climbing_Montage(Hop_Up_Montage);
		
		Debug::Print(TEXT("Hopping Up"));
	}
	
}

bool UCustom_Movement_Component::bCheck_Can_Hop_Up(FVector& Out_Hop_Up_Target_Position)
{
    FHitResult Hop_Up_Hit = Trace_From_Eye_Height(100.f, -15.f, true, true);
	FHitResult Safety_Ledge_Hit = Trace_From_Eye_Height(100.f, 150.f, true, true);

	if(Hop_Up_Hit.bBlockingHit && Safety_Ledge_Hit.bBlockingHit)
	{
		Out_Hop_Up_Target_Position = Hop_Up_Hit.ImpactPoint;
		return true;
	}

	return false;
}

void UCustom_Movement_Component::Handle_Hop_Down()
{
	FVector Hop_Down_Target_Point;
	if(bCheck_Can_Hop_Down(Hop_Down_Target_Point))
	{
		Set_Motion_Warping_Target(FName("Hop_Down_Target_Point"), Hop_Down_Target_Point);


		Play_Climbing_Montage(Hop_Down_Montage);
	}

	else
	{
		Debug::Print(TEXT("Can't Hop Down"));
	}
}

bool UCustom_Movement_Component::bCheck_Can_Hop_Down(FVector &Out_Hop_Down_Target_Point)
{	
	float Offset = 300.f;
	FVector Location = UpdatedComponent->GetComponentLocation();
	FVector Down_Vector = -UpdatedComponent->GetUpVector();
	FVector Forward_Vector = UpdatedComponent->GetForwardVector();
	
	FVector Start = Location + Forward_Vector * 2.f;
	FVector End = Start + Down_Vector * Offset;

	FHitResult Ground_Safety_Check = Do_Line_Trace_Single_By_Object(Start, End, 1, 1);
	FHitResult Hop_Down_Wall_Hit = Trace_From_Eye_Height(100.f, -300.f, 1, 1);
	
	if(Ground_Safety_Check.bBlockingHit)
	{
		return false;
	}

	else if (!Ground_Safety_Check.bBlockingHit && Hop_Down_Wall_Hit.bBlockingHit)
	{
		Out_Hop_Down_Target_Point = Hop_Down_Wall_Hit.ImpactPoint;
		return true;
	}

	return false;
	
	/*FHitResult Hop_Down_Wall_Hit = Trace_From_Eye_Height(100.f, -300.f);

	if(Hop_Down_Wall_Hit.bBlockingHit)
	{
		Out_Hop_Down_Target_Point = Hop_Down_Wall_Hit.ImpactPoint;
		return true;
	}
	return false; */	
}

void UCustom_Movement_Component::Handle_Hop_Left()
{
	FVector Hop_Left_Target_Point;
	if(bCheck_Can_Hop_Left(Hop_Left_Target_Point))
	{
		Set_Motion_Warping_Target(FName("Hop_Left_Target_Point"), Hop_Left_Target_Point);

		Play_Climbing_Montage(Hop_Left_Montage);
	}

	else
	{
		Debug::Print(TEXT("Can't Hop Left"));
	}

}

bool UCustom_Movement_Component::bCheck_Can_Hop_Left(FVector &Out_Hop_Left_Target_Point)
{
	float Offset = 100.f;
    FVector Location = UpdatedComponent->GetComponentLocation();
	FVector Forward_Vector = UpdatedComponent->GetForwardVector();
	FVector Component_Left_Vector = -UpdatedComponent->GetRightVector();
	FVector Component_Down_Vector = -UpdatedComponent->GetUpVector();
	FVector Down_Point = Location + Component_Down_Vector * Offset;
	FVector Left_Point = Down_Point + Component_Left_Vector * Climbing_Hop_Trace_Offset;
	FVector Wall_Point = Left_Point + Forward_Vector * Offset;
	
	FHitResult Wall_Safety_Check = Do_Line_Trace_Single_By_Object(Left_Point, Wall_Point, true, true);

	if(Wall_Safety_Check.bBlockingHit)
	{
		Out_Hop_Left_Target_Point = Wall_Safety_Check.ImpactPoint;
		return true;
	}

	return false;
}

void UCustom_Movement_Component::Handle_Hop_Right()
{
	FVector Hop_Right_Target_Point;
	if(bCheck_Can_Hop_Right(Hop_Right_Target_Point))
	{
		Set_Motion_Warping_Target(FName("Hop_Right_Target_Point"), Hop_Right_Target_Point);

		Play_Climbing_Montage(Hop_Right_Montage);
	}

	else
	{
		Debug::Print(TEXT("Can't Hop Right"));
	}
}

bool UCustom_Movement_Component::bCheck_Can_Hop_Right(FVector &Out_Hop_Right_Target_Point)
{
    float Offset = 100.f;
	FVector Location = UpdatedComponent->GetComponentLocation();
	FVector Forward_Vector = UpdatedComponent->GetForwardVector();
	FVector Right_Vector = UpdatedComponent->GetRightVector();
	FVector Down_Vector = -UpdatedComponent->GetUpVector();
	FVector Down_Point = Location + Down_Vector * Offset;
	FVector Right_Point = Down_Point + Right_Vector * Climbing_Hop_Trace_Offset;
	FVector Wall_Point = Right_Point + Forward_Vector * Offset;

	FHitResult Wall_Safety_Check = Do_Line_Trace_Single_By_Object(Right_Point, Wall_Point, true, true);

	if(Wall_Safety_Check.bBlockingHit)
	{
		Out_Hop_Right_Target_Point = Wall_Safety_Check.ImpactPoint;
		return true;
	}

	return false;
}

FVector UCustom_Movement_Component::Get_Unrotated_Climb_Velocity() const
{
    return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), Velocity);
}

#pragma endregion


#pragma endregion



#pragma region Take_Cover_Region


#pragma region Take_Cover_Traces

TArray<FHitResult> UCustom_Movement_Component::Do_Capsule_Trace_Multi_By_Object_Take_Cover(const FVector& Start, const FVector& End, bool B_Show_Debug_Shape, bool B_Draw_Persistant_Shapes)
{
	TArray<FHitResult> Out_Capsule_Trace_Hit_Results;

	EDrawDebugTrace::Type Debug_Trace_Type = EDrawDebugTrace::None;

	if(B_Show_Debug_Shape)
	{
		Debug_Trace_Type = EDrawDebugTrace::ForOneFrame;

		if(B_Draw_Persistant_Shapes)
		{
			Debug_Trace_Type = EDrawDebugTrace::Persistent;
		}
	}

	UKismetSystemLibrary::CapsuleTraceMultiForObjects(
		this,
		Start,
		End,
		Take_Cover_Capsule_Trace_Radius,
		Take_Cover_Capsule_Trace_Half_Height,
		Take_Cover_Surface_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Trace_Type,
		Out_Capsule_Trace_Hit_Results,
		false
	);
	
	return Out_Capsule_Trace_Hit_Results;
}

FHitResult UCustom_Movement_Component::Do_Line_Trace_Single_By_Object_Take_Cover(const FVector& Start, const FVector& End, bool B_Show_Debug_Shape, bool B_Draw_Persistant_Shapes)
{
	FHitResult Out_Hit;
	
	EDrawDebugTrace::Type Debug_Trace_Type = EDrawDebugTrace::None;

	if(B_Show_Debug_Shape)
	{
		Debug_Trace_Type = EDrawDebugTrace::ForOneFrame;

		if(B_Draw_Persistant_Shapes)
		{
			Debug_Trace_Type = EDrawDebugTrace::Persistent;
		}
	}

	/*GetWorld()->LineTraceSingleByChannel(
		Out_Hit,
		Start,
		End,
		ECollisionChannel::ECC_GameTraceChannel1
	);*/

	UKismetSystemLibrary::LineTraceSingleForObjects(
		this,
		Start,
		End,
		Take_Cover_Surface_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Trace_Type,
		Out_Hit,
		false
	);

	return Out_Hit;
}

FHitResult UCustom_Movement_Component::Do_Sphere_Trace_For_Objects(const FVector& Start, const FVector& End, bool B_Show_Debug_Shape, bool B_Draw_Persistent_Shapes)
{
	FHitResult Out_Hit;
	
	EDrawDebugTrace::Type Debug_Trace_Type = EDrawDebugTrace::None;

	if(B_Show_Debug_Shape)
	{
		Debug_Trace_Type = EDrawDebugTrace::ForOneFrame;

		if(B_Draw_Persistent_Shapes)
		{
			Debug_Trace_Type = EDrawDebugTrace::Persistent;
		}
	}

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		Take_Cover_Sphere_Trace_Radius,
		Take_Cover_Surface_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Trace_Type,
		Out_Hit,
		false
		);
	
	return Out_Hit;

}

#pragma endregion

#pragma region Take_Cover_Traces_Implemented

//Trace for surfaces which player can take cover. Return true if there are valid surfaces, otherwise returns false.
bool UCustom_Movement_Component::Capsule_Trace_Take_Cover_Surfaces()
{
	const FVector Start_Offset = UpdatedComponent->GetForwardVector() * 55.f;
	const FVector Start = UpdatedComponent->GetComponentLocation() + Start_Offset;
	const FVector End = Start + UpdatedComponent->GetForwardVector();

	Take_Cover_Surfaces_Traced_Results = Do_Capsule_Trace_Multi_By_Object_Take_Cover(Start, End, true /*,true */);

	return !Take_Cover_Surfaces_Traced_Results.IsEmpty();
}

bool UCustom_Movement_Component::Capsule_Trace_Ground_Surface()
{
	const FVector Start_Offset = -UpdatedComponent->GetUpVector() * 30.f;
	const FVector Start = UpdatedComponent->GetComponentLocation() + Start_Offset;
	const FVector End = Start + -UpdatedComponent->GetForwardVector();

	Take_Cover_Ground_Surface_Traced_Results = Do_Capsule_Trace_Multi_By_Object_Take_Cover(Start, End, true /*, true*/);

	return !Take_Cover_Ground_Surface_Traced_Results.IsEmpty();
}

FHitResult UCustom_Movement_Component::Sphere_Trace_Trace_Take_Cover()
{
	const FVector Forward = UpdatedComponent->GetForwardVector();

	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + Forward * 200.f;
	
	 return Sphere_Trace_Hit_Result = Do_Sphere_Trace_For_Objects(Start, End, true, false);
}

FHitResult UCustom_Movement_Component::Line_Trace_Check_Cover_Right(float Trace_Distance, float Trace_Start_Offset)
{
	const FVector Component_Location = UpdatedComponent->GetComponentLocation();
	const FVector Right_Offset = UpdatedComponent->GetRightVector() * Trace_Start_Offset;
	
	const FVector Start = Component_Location + Right_Offset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * Trace_Distance;

 	return Do_Line_Trace_Single_By_Object_Take_Cover(Start, End /*, true, true*/);
}

FHitResult UCustom_Movement_Component::Line_Trace_Check_Cover_Left(float Trace_Distance, float Trace_Start_Offset)
{
	const FVector Component_Location = UpdatedComponent->GetComponentLocation();
	const FVector Left_Offset = -UpdatedComponent->GetRightVector() * Trace_Start_Offset;
	
	const FVector Start = Component_Location + Left_Offset;
	const FVector End = Start + UpdatedComponent->GetForwardVector() * Trace_Distance;

 	return Do_Line_Trace_Single_By_Object_Take_Cover(Start, End /*, true, true*/);
}

#pragma endregion

#pragma region Take_Cover_Core

void UCustom_Movement_Component::Toggle_Take_Cover(bool bEneble_Take_Cover)
{
	if(bEneble_Take_Cover)
	{
		FVector Take_Cover_End_Position;
		
		if(Can_Take_Cover(Take_Cover_End_Position))
		{
			//Enter Take Cover State
			Debug::Print(TEXT("Can Take Cover!"));
			Debug::Print(TEXT("Take Cover End Position: ") + Take_Cover_End_Position.ToCompactString());
			Set_Motion_Warping_Target(FName("Take_Cover_Crouch"), Take_Cover_End_Position);
			
			Play_Take_Cover_Montage(Idle_To_Take_Cover_Montage);
		}

		else
		{
			Debug::Print(TEXT("Can't Take Cover!"));
		}
	}

	else
	{
		//Stop Taking Cover
		Play_Take_Cover_Montage(Exit_Cover_To_Stand);
	}
}

bool UCustom_Movement_Component::Can_Take_Cover(FVector& Out_Take_Cover_End_Position)
{
	Sphere_Trace_Trace_Take_Cover();

	Out_Take_Cover_End_Position = FVector::ZeroVector;

	if(IsFalling() || !Sphere_Trace_Hit_Result.bBlockingHit) return false;
	if(!Capsule_Trace_Take_Cover_Surfaces() || !Capsule_Trace_Ground_Surface()) return false;
	if(!Line_Trace_Check_Cover_Right(Take_Cover_Check_Cover_Edge).bBlockingHit || !Line_Trace_Check_Cover_Right(Take_Cover_Check_Cover_Edge).bBlockingHit) return false;

	if(Sphere_Trace_Hit_Result.bBlockingHit)
	{
		Out_Take_Cover_End_Position = Sphere_Trace_Hit_Result.ImpactPoint;
	}

	if(Out_Take_Cover_End_Position != FVector::ZeroVector)
	{
		return true;
	}

	else
	{
		return false;
	}
}

void UCustom_Movement_Component::Start_Take_Cover()
{
	SetMovementMode(MOVE_Custom, E_Custom_Movement_Mode::MOVE_Take_Cover);
}

void UCustom_Movement_Component::Stop_Take_Cover()
{
	SetMovementMode(MOVE_Walking);
}

void UCustom_Movement_Component::Physics_Take_Cover(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	//Process all take cover surfaces information

	Capsule_Trace_Take_Cover_Surfaces();

	Capsule_Trace_Ground_Surface();

	Process_Take_Cover_Surface_Info();

	Process_Take_Cover_Ground_Surface_Info();

	/* Check if we should exit taking cover*/
	if(Check_Should_Exit_Take_Cover())
	{
		Stop_Take_Cover();
	}

	RestorePreAdditiveRootMotionVelocity();

	if( !HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{	
		/*Define the max take cover speed and acceleration*/
		CalcVelocity(deltaTime, 0.f, true, Max_Brake_Take_Cover_Deceleration);
	}

	ApplyRootMotionToVelocity(deltaTime);


	FVector OldLocation = UpdatedComponent->GetComponentLocation();
	const FVector Adjusted = Velocity * deltaTime;
	FHitResult Hit(1.f);
	
	/*Handle the take cover rotation*/
	SafeMoveUpdatedComponent(Adjusted, Get_Take_Cover_Rotation(deltaTime), true, Hit);

	if (Hit.Time < 1.f)
	{
		//adjust and try again
			HandleImpact(Hit, deltaTime, Adjusted);
			SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}

	if(!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() )
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	/*Snap movement to take cover surfaces*/
	Snap_Movement_To_Take_Cover_Surfaces(deltaTime);
	Take_Cover_Snap_Movement_To_Ground(deltaTime);
}

void UCustom_Movement_Component::Process_Take_Cover_Surface_Info()
{
	Current_Take_Cover_Surface_Location = FVector::ZeroVector;
	Current_Take_Cover_Surface_Normal = FVector::ZeroVector;

	if(Take_Cover_Surfaces_Traced_Results.IsEmpty()) return;

	for(const FHitResult& Traced_Hit_Result : Take_Cover_Surfaces_Traced_Results)
	{
		Current_Take_Cover_Surface_Location += Traced_Hit_Result.ImpactPoint;
		Current_Take_Cover_Surface_Normal += Traced_Hit_Result.ImpactNormal;
	}

	Current_Take_Cover_Surface_Location /= Take_Cover_Surfaces_Traced_Results.Num();
	Current_Take_Cover_Surface_Normal = Current_Take_Cover_Surface_Normal.GetSafeNormal();
}

void UCustom_Movement_Component::Process_Take_Cover_Ground_Surface_Info()
{
	if(Take_Cover_Ground_Surface_Traced_Results.IsEmpty()) return;

	for(const FHitResult& Traced_Hit_Result : Take_Cover_Ground_Surface_Traced_Results)
	{
		Current_Take_Cover_Ground_Surface_Location += Traced_Hit_Result.ImpactPoint;
		Current_Take_Cover_Ground_Surface_Normal += Traced_Hit_Result.ImpactNormal;
	}

	Current_Take_Cover_Ground_Surface_Location /= Take_Cover_Ground_Surface_Traced_Results.Num();
	Current_Take_Cover_Ground_Surface_Normal = Current_Take_Cover_Ground_Surface_Normal.GetSafeNormal();

	Debug::Print(TEXT("Take Cover Ground Surface Location: ") + Current_Take_Cover_Ground_Surface_Location.ToCompactString(), FColor::Green, 1);
	Debug::Print(TEXT("Take Cover Ground Surface Normal: ") + Current_Take_Cover_Ground_Surface_Normal.ToCompactString(), FColor::Yellow, 2);
}

bool UCustom_Movement_Component::Check_Should_Exit_Take_Cover()
{
	if(Take_Cover_Surfaces_Traced_Results.IsEmpty()) return true;

	const float Dot_Result = FVector::DotProduct(Current_Take_Cover_Surface_Normal, FVector::UpVector);
	const float Degree_Difference = FMath::RadiansToDegrees(FMath::Acos(Dot_Result));

	if(Degree_Difference <= 87.f)
	{
		return true;
	}

	Debug::Print(TEXT("Take Cover Edge Degree Difference") + FString::SanitizeFloat(Degree_Difference), FColor::Yellow, 1);

	return false;
}

FQuat UCustom_Movement_Component::Get_Take_Cover_Rotation(float DeltaTime)
{
	const FQuat Current_Quat = UpdatedComponent->GetComponentQuat();

	if(HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return Current_Quat;
	}

	const FQuat Target_Quat = FRotationMatrix::MakeFromX(-Current_Take_Cover_Surface_Normal).ToQuat();
	
	return FMath::QInterpTo(Current_Quat, Target_Quat, DeltaTime, 5.f);
}

void UCustom_Movement_Component::Snap_Movement_To_Take_Cover_Surfaces(float DeltaTime)
{
	const FVector Component_Forward = UpdatedComponent->GetForwardVector();
	const FVector Component_Locataion = UpdatedComponent->GetComponentLocation();

	const FVector Projected_Character_To_Surface = 
	(Current_Take_Cover_Surface_Location - Component_Locataion).ProjectOnTo(Component_Forward);

	const FVector Snap_Vector = -Current_Take_Cover_Surface_Normal * Projected_Character_To_Surface.Length();

	UpdatedComponent->MoveComponent
	(Snap_Vector * DeltaTime * Max_Take_Cover_Speed,
	UpdatedComponent->GetComponentQuat(),
	true
	);
}

void UCustom_Movement_Component::Take_Cover_Snap_Movement_To_Ground(float DeltaTime)
{
	const FVector Component_Down = -UpdatedComponent->GetUpVector();
	const FVector Component_Locataion = UpdatedComponent->GetComponentLocation();

	const FVector Projected_Character_To_Surface = 
	(Current_Take_Cover_Ground_Surface_Location - Component_Locataion).ProjectOnTo(Component_Down);

	const FVector Snap_Vector = -Current_Take_Cover_Ground_Surface_Normal * Projected_Character_To_Surface.Length();

	UpdatedComponent->MoveComponent
	(Snap_Vector * DeltaTime * Max_Take_Cover_Speed,
	UpdatedComponent->GetComponentQuat(),
	true
	);
}

void UCustom_Movement_Component::Play_Take_Cover_Montage(UAnimMontage* Montage_To_Play)
{
	if(!Montage_To_Play) return;
	if(!Owning_Player_Animation_Instance) return;
	if(Owning_Player_Animation_Instance->IsAnyMontagePlaying()) return;

	Owning_Player_Animation_Instance->Montage_Play(Montage_To_Play);
}

void UCustom_Movement_Component::On_Take_Cover_Montage_Ended(UAnimMontage* Montage, bool bInterrupted)
{
	if(Montage == Idle_To_Take_Cover_Montage)
	{
		Debug::Print(TEXT("Take Cover Montage Ended!"));
		
		Start_Take_Cover();
	}

	if(Montage == Exit_Cover_To_Stand)
	{
		Debug::Print(TEXT("Exiting Take Cover Montage"));

		Stop_Take_Cover();
	}
}

FVector UCustom_Movement_Component::Get_Unrotated_Take_Cover_Velocity() const
{
	return UKismetMathLibrary::Quat_UnrotateVector(UpdatedComponent->GetComponentQuat(), Velocity);
}

bool UCustom_Movement_Component::Is_Taking_Cover() const
{
	return MovementMode == MOVE_Custom && CustomMovementMode == E_Custom_Movement_Mode::MOVE_Take_Cover;
}

#pragma endregion


#pragma endregion



#pragma region Parkour_Region

#pragma region Initialize_Parkour

void UCustom_Movement_Component::Attach_Arrow_Actor_To_Character(ATechnical_Animator_Character* Character)
{
	//Use the character pointer passed in at "&UCustom_Movement_Component::Initialize_Parkour_Pointers" by (ATechnical_Animator_Character* Character) to 
	//"GetActorTransform()" and initialize input paramater 1. (for "GetWorld()->SpawnActor") "FTransform Location".
	FTransform Location{Character->GetActorTransform()};
	
	//Initialize Input parameter 2. (for "GetWorld()->SpawnActor") "FActorSpawnParameters Spawn_Info"
	FActorSpawnParameters Spawn_Info{};

	//Spawn the arrow component which is within "&ACharacter_Direction_Arrow" using "GetWorld()->SpawnActor". Use the two input parameters initialized above (Location, Spawn_Info).
	Character_Direction_Arrow = GetWorld()->SpawnActor<ACharacter_Direction_Arrow>(ACharacter_Direction_Arrow::StaticClass(), Location, Spawn_Info);
	
	//After spawning the arrow and attach it to the character using the character pointer passed in by (ATechnical_Animator_Character* Character). Snap it to the target.
	Character_Direction_Arrow->AttachToActor(Character, FAttachmentTransformRules::SnapToTargetIncludingScale);
	
	//Offset the location of the arrow with "SetActorRelativeLocation()" so it is just above the capsule component using its pointer which is initialized above "ACharacter_Direction_Arrow* Character_Direction_Arrow". 
	FVector Character_Direction_Arrow_Relative_Location{FVector(0.f, 0.f, 100.f)};
	Character_Direction_Arrow->SetActorRelativeLocation(Character_Direction_Arrow_Relative_Location);
}

void UCustom_Movement_Component::Get_Pointer_To_Parkour_Locomotion_Interface_Class()
{
	//This cast will return the object that uses the interface. The interface is being used in the Animation Instance, therefore
	//casting "Anim_Instance" to "IParkour_Locomotion_Interface" is nessacary. When Parkour_Interface is used to call the
	//generated function (at compile) "Execute_..." the input argument of the function being called will requre a pointer to the object 
	//which is using the interface. "Anim_Instance" will be passed in as the pointer to this object followed by the other inut parameter(s) 
	//which needs to be filled in.
	Parkour_Interface = Cast<IParkour_Locomotion_Interface>(Anim_Instance);
	if(Parkour_Interface) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Parkour_Interface INITIALIZATION SUCCEEDED"));
	}

	else UE_LOG(LogTemp, Warning, TEXT("Parkour_Interface INITIALIZATION FAILED"));
}

/*void UCustom_Movement_Component::Get_Pointer_To_Parkour_Action_Data_Class()
{
	//Default_Parkour_Data_Asset_Pointer was declared and initialized in the character Blueprint with an empty Data Asset of the type UParkour_Action_Data.
	//This is so an object of the type UParkour_Action_Data* could be created, enabling the calling of functions from within the class UParkour_Action_Data.
	//The Data Asset assigned to the "Default_Parkour_Data_Asset_Pointer" slot within the character Blueprint has no use other use.
	if(Default_Parkour_Data_Asset_Pointer)
	//Parkour_Data_Asset is the pointer which will be used throughout the Parkour System. It holds the address of the class "UParkour_Action_Data" which is set within
	//the constructor of said class followed by the transfer of value which happens within "&UParkour_Action_Data::Get_Pointer_To_This_Class()".
	Parkour_Data_Asset = Default_Parkour_Data_Asset_Pointer->Get_Pointer_To_This_Class();

	if(Parkour_Data_Asset) 
	{
		UE_LOG(LogTemp, Warning, TEXT("Parkour_Data_Asset INITIALIZATION SUCCEEDED"));
	}

	else UE_LOG(LogTemp, Warning, TEXT("Parkour_Data_Asset INITIALIZATION FAILED"));
}*/

void UCustom_Movement_Component::Initialize_Parkour(ATechnical_Animator_Character* Character, UMotionWarpingComponent* Motion_Warping, UCameraComponent* Camera)
{
	//"GetCharacterMovement" using the character pointer passed in by (ATechnical_Animator_Character* Character) and initialize "UCharacterMovementComponent* Character_Movement".
	Character_Movement = Character->GetCharacterMovement();
	//Get the mesh using the character pointer passed in by (ATechnical_Animator_Character* Character) and initialize "USkeletalMeshComponent* Mesh".
	Mesh = Character->GetMesh(); 
	//Get the CapsuleComponent by using the character pointer passed in by (ATechnical_Animator_Character* Character) and initialize "UCapsuleComponent* Capsule_Component".
	Capsule_Component = Character->GetCapsuleComponent();
	//Use the "USkeletalMeshComponent* Mesh" which is initialized by the character pointer passed in by (ATechnical_Animator_Character* Character), to get the GetAnimInstance. Initialize "UAnimInstance* Anim_Instance".
	Anim_Instance = Mesh->GetAnimInstance();
	//Initialize "UMotionWarpingComponent* Motion_Warping_Component" with the "UMotionWarpingComponent* Motion_Warping" which is passed in by "&ATechnical_Animator_Character". 
	Motion_Warping_Component = Motion_Warping;
	//Initialize "UCameraComponent* Camera_Component" with the "UCameraComponent* Camera" that is passed in by "&ATechnical_Animator_Character".
	Camera_Component = Camera;
	
	//Initialize " APlayer_Controller Player_Controller*" by casting "(UGameplayStatics::GetPlayerController(GetWorld(), 0)" from type "APlayerController".
	Player_Controller = Cast<APlayer_Controller>(UGameplayStatics::GetPlayerController(GetWorld(), 0));

	/*Getting a pointer to the Parkour_Locomotion_Interface_Class.*/
	Get_Pointer_To_Parkour_Locomotion_Interface_Class();

	/*Getting a pointer to the Parkour_Action_Data_Class*/
	//Get_Pointer_To_Parkour_Action_Data_Class();
	
	/*Attach Arrow from "&ACharacter_Direction_Arrow" to the Character*/
	Attach_Arrow_Actor_To_Character(Character);

	Initialize_Parkour_Data_Assets_Arrays();

	Initialize_Actors_To_Ignore_Arrays();
	
}

void UCustom_Movement_Component::Initialize_Parkour_Data_Assets_Arrays()
{
	#pragma region Data_Assets_TArrays
	
	Braced_And_Ledge_Shimmy_180_Shimmy_Array.Emplace(Climb_Shimmy_To_Shimmy_180_Vault);
	Braced_And_Ledge_Shimmy_180_Shimmy_Array.Emplace(Ledge_Turn_L_Vault);
	Braced_And_Ledge_Shimmy_180_Shimmy_Array.Emplace(Ledge_Turn_R_Vault);


	Hanging_Shimmy_180_Shimmy_Array.Emplace(Hanging_180_L);
	Hanging_Shimmy_180_Shimmy_Array.Emplace(Hanging_180_R);


	Ledge_Climb_Up_Array.Emplace(Ledge_Climb_Up_Reverse); 
	Ledge_Climb_Up_Array.Emplace(Ledge_Climb_Up_TwoHand_L); 
	Ledge_Climb_Up_Array.Emplace(Ledge_Climb_Up_TwoHand_R);
	Ledge_Climb_Up_Array.Emplace(Ledge_Climb_Up_Monkey);
	Ledge_Climb_Up_Array.Emplace(Climb_Up_The_Ledge);
			

	Hanging_Climb_Up_Array.Emplace(Hanging_Climb_Up);
	Hanging_Climb_Up_Array.Emplace(Free_Hang_Climb_Up);
			

	Hop_Up_Array.Emplace(Braced_Hang_Hop_Up);
	Hop_Up_Array.Emplace(Ledge_Jump_Up_Power);								  
	Hop_Up_Array.Emplace(Ledge_Jump_Up); 	
	Hop_Up_Array.Emplace(Climb_Shimmy_Long_L_Up);
	Hop_Up_Array.Emplace(Climb_Shimmy_Long_R_Up);


	Braced_And_Ledge_Hop_Left_Array.Emplace(Braced_Hang_Hop_Left);
	Braced_And_Ledge_Hop_Left_Array.Emplace(Ledge_Jump_L_Short);
	Braced_And_Ledge_Hop_Left_Array.Emplace(Ledge_Jump_L);
	Braced_And_Ledge_Hop_Left_Array.Emplace(Climb_Shimmy_Long_L_Left);


	Braced_And_Ledge_Hop_Right_Array.Emplace(Braced_Hang_Hop_Right);
	Braced_And_Ledge_Hop_Right_Array.Emplace(Ledge_Jump_R_Short);
	Braced_And_Ledge_Hop_Right_Array.Emplace(Ledge_Jump_R); 
	Braced_And_Ledge_Hop_Right_Array.Emplace(Climb_Shimmy_Long_R_Right);


	//Braced_And_Adventure_Hop_Up_Left_Array.Emplace(Braced_Hang_Hop_Left_Up);
	Braced_And_Adventure_Hop_Up_Left_Array.Emplace(Climb_Shimmy_Long_L_Up_Left);



	//Braced_And_Adventure_Hop_Up_Right_Array.Emplace(Braced_Hang_Hop_Right_Up);
	Braced_And_Adventure_Hop_Up_Right_Array.Emplace(Climb_Shimmy_Long_R_Up_Right);


	Braced_And_Ledge_Hop_Down_Array.Emplace(Braced_Hang_Hop_Down);
	Braced_And_Ledge_Hop_Down_Array.Emplace(Ledge_Jump_Down);
	Braced_And_Ledge_Hop_Down_Array.Emplace(Climb_Leap_Down_To_Ledge);

	
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Monkey_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Reverse_L_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Reverse_R_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Safety_L_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Safety_R_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Thief_L_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_Thief_R_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_TwoHand_L_Vault);
	Exit_Ledge_Jump_Forward_Array.Emplace(Ledge_Climb_Up_TwoHand_R_Vault);

		
	Exit_Ledge_Jump_Backward_Array.Emplace(Exit_Ledge_Jump_Backward_L);
	Exit_Ledge_Jump_Backward_Array.Emplace(Exit_Ledge_Jump_Backward_R);



	Drop_Ledge_Array.Emplace(Accelerating_Drop_Ledge_L);
	Drop_Ledge_Array.Emplace(Accelerating_Drop_Ledge_R);
	Drop_Ledge_Array.Emplace(Accelerating_Drop_Slide_Ledge_L);
	Drop_Ledge_Array.Emplace(Accelerating_Drop_Slide_Ledge_R);



	Drop_Hanging_Array.Emplace(Accelerating_Drop_Hanging_L);
	Drop_Hanging_Array.Emplace(Accelerating_Drop_Hanging_R);



	Landing_Down_Front_Array.Emplace(Landing_Front_L);
	Landing_Down_Front_Array.Emplace(Landing_Front_R);



	Landing_Down_Roll_Array.Emplace(Landing_Roll_A_L);
	Landing_Down_Roll_Array.Emplace(Landing_Roll_A_R);
	Landing_Down_Roll_Array.Emplace(Landing_Roll_B_L);
	Landing_Down_Roll_Array.Emplace(Landing_Roll_B_R);


	Free_Roam_Accelerating_Drop_Array.Emplace(Dash_Drop);
	//Free_Roam_Accelerating_Drop_Array.Emplace(Jump_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Monkey_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Reverse_L_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Reverse_R_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Speed_L_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Speed_R_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Two_Hand_L_Drop);
	Free_Roam_Accelerating_Drop_Array.Emplace(Two_Hand_R_Drop);


	Balance_Walk_Automatic_Hop_Array.Emplace(Balance_Walk_Jump_Front);
	Balance_Walk_Automatic_Hop_Array.Emplace(Jump_One_L);
	Balance_Walk_Automatic_Hop_Array.Emplace(Jump_One_R);


	Running_Wall_Vault_On_Array.Emplace(Wall_Monkey_On);
	Running_Wall_Vault_On_Array.Emplace(Wall_Two_Hand_L_On);
	Running_Wall_Vault_On_Array.Emplace(Wall_Two_Hand_R_On);


	Running_Wall_Vault_Over_Array.Emplace(Wall_Monkey_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Reverse_L_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Reverse_R_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Safety_L_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Safety_R_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Thief_L_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Thief_R_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Two_Hand_L_Vault);
	Running_Wall_Vault_Over_Array.Emplace(Wall_Two_Hand_R_Vault);


	Running_Wall_Vault_Over_180_Shimmy_Array.Emplace(Wall_L_Turn);												   
	Running_Wall_Vault_Over_180_Shimmy_Array.Emplace(Wall_R_Turn);


	Tic_Tac_L_Over_Front_Wall_Array.Emplace(Tic_Tac_L_Jump_Over);
	Tic_Tac_L_Over_Front_Wall_Array.Emplace(Tic_Tac_L_Reverse_Over);
	Tic_Tac_L_Over_Front_Wall_Array.Emplace(Tic_Tac_L_Speed_Over);

																	 
	Tic_Tac_R_Over_Front_Wall_Array.Emplace(Tic_Tac_R_Jump_Over);
	Tic_Tac_R_Over_Front_Wall_Array.Emplace(Tic_Tac_R_Reverse_Over);
	Tic_Tac_R_Over_Front_Wall_Array.Emplace(Tic_Tac_R_Speed_Over);


	Tic_Tac_L_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_L_Jump_On);
	Tic_Tac_L_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_L_Vault_On);


	Tic_Tac_R_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_R_Jump_On);
	Tic_Tac_R_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_R_Vault_On);




	#pragma endregion
	
}

void UCustom_Movement_Component::Initialize_Actors_To_Ignore_Arrays()
{
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWall_Pipe_Actor::StaticClass(), Wall_Pipe_Actors_To_Ignore);

	if(Wall_Pipe_Actors_To_Ignore.Num() == 4)
	{
		Debug::Print("Wall_Pipe_Actors_Ignored_For_Parkour_Detect_Wall_Trace", FColor::Green, 77);
		Debug::Print("Wall_Pipe_Actors_Ignored_For_Validate_bIs_On_Ground_Trace", FColor::Green, 78);
	}

	else
	{
		Debug::Print("Wall_Pipe_Actors_Not_Ignored", FColor::Red, 77);
	}


	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AWall_Vault_Actor::StaticClass(), Wall_Vault_Actors_To_Ignore);

	if(Wall_Vault_Actors_To_Ignore.Num() == 4)
	{
		Debug::Print("Wall_Vault_Actors_Ignored_For_Parkour_Detect_Wall_Trace", FColor::Green, 79);
	}

	else
	{
		Debug::Print("Wall_Vault_Actors_Not_Ignored", FColor::Red, 79);
	}

}

#pragma endregion

#pragma region Parkour_Helper

FVector UCustom_Movement_Component::Move_Vector_Up(const FVector& Initial_Location, const float& Move_Value) const
{
	const FVector Move_Direction{UpdatedComponent->GetUpVector()};
	const FVector Destination{Initial_Location + (Move_Direction * Move_Value)};
	
	return Destination;
}

FVector UCustom_Movement_Component::Move_Vector_Down(const FVector& Initial_Location, const float& Move_Value) const
{
	const FVector Move_Direction{UpdatedComponent->GetUpVector()};
	const FVector Destination{Initial_Location + (-Move_Direction * Move_Value)};

	return Destination;
}

FVector UCustom_Movement_Component::Move_Vector_Left(const FVector& Initial_Location, const FRotator& Rotation, const float& Move_Value) const
{
	const FVector Move_Direction{UKismetMathLibrary::GetRightVector(Rotation)};
	const FVector Destination{Initial_Location + (-Move_Direction * Move_Value)};

	return Destination;
}

FVector UCustom_Movement_Component::Move_Vector_Right(const FVector& Initial_Location, const FRotator& Rotation, const float& Move_Value) const
{
	const FVector Move_Direction{UKismetMathLibrary::GetRightVector(Rotation)};
	const FVector Destination{Initial_Location + (Move_Direction * Move_Value)};

	return Destination;
}

FVector UCustom_Movement_Component::Move_Vector_Forward(const FVector& Initial_Location, const FRotator& Rotation, const float& Move_Value) const
{
	const FVector Move_Direction{UKismetMathLibrary::GetForwardVector(Rotation)};
	const FVector Destination{Initial_Location + (Move_Direction * Move_Value)};

	return Destination;
}

FVector UCustom_Movement_Component::Move_Vector_Backward(const FVector& Initial_Location, const FRotator& Rotation, const float& Move_Value) const
{
	const FVector Move_Direction{UKismetMathLibrary::GetForwardVector(Rotation)};
	const FVector Destination{Initial_Location + (-Move_Direction * Move_Value)};

	return Destination;
}

FRotator UCustom_Movement_Component::Add_Rotator(const FRotator& Initial_Rotation, const float& Add_To_Rotation) const
{
	const FRotator New_Rotation_Roll{Initial_Rotation.Roll + Add_To_Rotation};
	const FRotator New_Rotation_Pitch{Initial_Rotation.Pitch + Add_To_Rotation};
	const FRotator New_Rotation_Yaw{Initial_Rotation.Yaw + Add_To_Rotation};
	const FRotator New_Rotation{New_Rotation_Roll + New_Rotation_Pitch + New_Rotation_Yaw};

	return New_Rotation;
}

FRotator UCustom_Movement_Component::Reverse_Wall_Normal_Rotation_Z(const FVector& Initial_Wall_Normal) const
{
	                                              //The direction of the wall normal is its X axis (pointing forward).
	const FRotator Wall_Normal{UKismetMathLibrary::MakeRotFromX(Initial_Wall_Normal)};
	const FRotator Reverse_Wall_Rotation_On_Z_Axis_180{Wall_Normal - FRotator(0, 180, 0)};
	const FRotator Delta_Rotaton{Reverse_Wall_Rotation_On_Z_Axis_180};
	
	return  Delta_Rotaton;
}

void UCustom_Movement_Component::Draw_Debug_Sphere(const FVector& Location, const float& Radius, const FColor& Color, const float& Duration, const bool& bDraw_Debug_Shape_Persistent, const float& Lifetime) const
{
	UWorld* World = GetWorld();

	DrawDebugSphere(
		World,
		Location,
		Radius,
		12,
		Color,
		bDraw_Debug_Shape_Persistent,
		Lifetime
	);
}

#pragma endregion

#pragma region Parkour_Traces

void UCustom_Movement_Component::Parkour_Detect_Wall()
{	
	//Initialize the last index to be used in the for loop.
	int For_Loop_Last_Index{};
	//Depending on the status of the character one of these integers will be used to set the last index for the for loop.
	const int Is_Falling{7};
	const int Is_Not_Falling{14};
	
	//Set the last index for the for loop.
	//If the character is falling there should be less ray casts performed to decrease the "height" of all the sphere traces combined together.
	if(Character_Movement)
	{
		if(Character_Movement->IsFalling())
		For_Loop_Last_Index = Is_Falling;

		else //if(!Character_Movement->IsFalling())
		For_Loop_Last_Index = Is_Not_Falling;
	}

	int Index{};
	for(Index; Index <= For_Loop_Last_Index; Index++)
	{
		//Get the location of the character.
		const FVector Component_Location{UpdatedComponent->GetComponentLocation()};
		//Set the vector which the sphere trace will use to be at ground level. This for loop will create new sphere traces from this location. Each sphere trace will stack ontop of the previous forming a "tower".
		const FVector Set_Vector_At_Ground_Level{Move_Vector_Down(Component_Location, 70.f)};
		//With each iteration of the for loop move the vector up 17 units by multiplying the index by 17.
		const FVector Move_Vector_Up_With_Each_Iteration_Of_Loop{Move_Vector_Up(Set_Vector_At_Ground_Level, Index * 17.f)};
		//Move the vector backwards 20 units so that it starts right behind the character.
		const FVector Start{Move_Vector_Backward(Move_Vector_Up_With_Each_Iteration_Of_Loop, UpdatedComponent->GetComponentRotation(), 20)};
		//Move the vector forward 100 - 140 units so that it ends a good distance away from the character.
		FVector End{};

		if(bIs_On_Ground)
		{
			End = Move_Vector_Forward(Start, UpdatedComponent->GetComponentRotation(), 100);
		}

		else
		{
			End = Move_Vector_Forward(Start, UpdatedComponent->GetComponentRotation(), 140);
		}


		//Develop a "SphereTraceSingleForObjects()". The objects will be set in the character blueprint.
		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			10.f,
			Parkour_Detect_Wall_Trace_Types,
			false,
			Wall_Pipe_Actors_To_Ignore,
			Debug_Action,
			Initial_Front_Wall_Hit_Result,
			false
			);

		//If there is a blocking hit and there is no initial overlap break out of the for loop early.
		if(Initial_Front_Wall_Hit_Result.bBlockingHit && !Initial_Front_Wall_Hit_Result.bStartPenetrating)
		break;
	}
	
	//Drawing debug sphere so the "EDrawDebugTrace" can be set to none on the "SphereTraceSingleForObjects()".
	//Draw_Debug_Sphere(Initial_Front_Wall_Hit_Result.ImpactPoint, 5.f, FColor::Blue, 1.f, false, 7.f);
} 

void UCustom_Movement_Component::Grid_Scan_For_Hit_Results(const FVector& Previous_Trace_Location, const FRotator& Previous_Trace_Rotation, const int& Scan_Width_Value, const int& Scan_Height_Value)
{
	/*The input parameter "Previous_Trace_Rotation" is the Impact Normal for the "SphereTraceSingleForObjects()" found in "&UCustom_Movement_Component::Parkour_Detect_Wall()". 
	This normal will be reversed when this function is called so that the ray cast performed in this function will face towards the wall of the impact point passed into "Previous_Trace_Location".
	This impact point is also foundin "&UCustom_Movement_Component::Parkour_Detect_Wall() "Scan_Width_Value" and "Scan_Height_Value" will are variables which may be set in C++ or in the character Blueprint*/
	
	//The "LineTraceSingleForObjects()" performed in the for loop will fill this FHitResult with data.
	FHitResult Out_Hit{};

	//In this case it's good practice to empty the arrays at the beginnning of the function so that garagbage data from the previous function call can be wiped out.
	Grid_Scan_Hit_Traces.Empty();
	
	//Develop a for loop which will set the "Width" of the line traces performed. The goal is to have the line traces casted form a grid which have has a width of 5 line traces.
	//"Scan_Width_Value" will be filled with the the value 5.
	for(int Index_1{}; Index_1 <= Scan_Width_Value; Index_1++)
	{	
		//Multiply "Index_1" by 20 on each iteration of the for loop. This value will be subtracted from to generate the vector location of each of the line traces which make up the width of "grid"
		const int Index_1_Multiplier{Index_1 * 20};
		
		//Multiply the "Scan_Width_Value" by 10 during each iteration of the for loop. This value will be subtracted from "Index_1_Multiplier" during each iteration of the loop to position the vector 
		//which will serve as the location for respective the ray cast. 
		const int Scan_Width_Value_Multiplier{Scan_Width_Value * 10};
		
		//Subtract the value in "Scan_Width_Value_Multiplier" from the value in "Index_1_Multiplier" to position the vector accordingly.
		const int Set_Scan_Width_Trace_Location_Value{Index_1_Multiplier - Scan_Width_Value_Multiplier};
		
		//With the calculation (subtraction) above, on the first iteration of this for loop, the vector will start off at the left of the input parameter "Previous_Trace_Location" which is 
		//the impact point of the "SphereTraceSingleForObjects()" found in "&UCustom_Movement_Component::Parkour_Detect_Wall()". This is because the value will start of as a negative number 
		//on loop 0 and in result instead of "Move_Vector_Right" the vector will move to the left (because of the negative value). WIth each loop iteration, the vector will move to the right 
		//as the value increases. When "Index_1" is 2 the value of "Set_Scan_Width_Trace_Location_Value" will be 0 and in result the vector will be at the same location as the input parameter 
		//"Previous_Trace_Location". This will be followed by two more loops which will move the vector to the right of the "SphereTraceSingleForObjects()" found in "&UCustom_Movement_Component::Parkour_Detect_Wall()"  
		//(input parameter "Previous_Trace_Location"). In result there will be five raycast formed, hence forming the "width" of the grid scan.
		const FVector Set_Scan_Width_Trace_Location{Move_Vector_Right(Previous_Trace_Location, Previous_Trace_Rotation, Set_Scan_Width_Trace_Location_Value)};

		//Develop a nested for loop to handle the height of the grid scan.
		//"Scan_Height_Value" will be filled with the the value 30.
		for(int Index_2{}; Index_2 <= Scan_Height_Value; Index_2++)
		{
			//Multiply "Index_2" by 8 on each iteration of the for loop. This value will be used to generate the vector location of each of the line traces which make up the height of "grid" 
			const int Index_2_Multiplier{Index_2 * 8};
			
			//With each iteration of this nested loop the vector will have a starting point of the "Set_Scan_Width_Trace_Location" which happens to be at ground level. Therefore, to increase the height of the vector
			//consistently, making sure the position of the vector is above the position of the previous vector a multiplication of (Index_2 * 8) found in "Index_2_Multiplier" has to be done during each loop. 
			const FVector Set_Scan_Height_Trace_Location{Move_Vector_Up(Set_Scan_Width_Trace_Location, Index_2_Multiplier)};
			
			//Move the vector backwards so that it starts right in front of the character.
			const FVector Start{Move_Vector_Backward(Set_Scan_Height_Trace_Location, Previous_Trace_Rotation, 50.f)};
			
			//Move the vector forwards so that the line traces end a good distance away from the character
			const FVector End{Move_Vector_Forward(Start, Previous_Trace_Rotation, 80.f)};

			//Develop the line traces
			UKismetSystemLibrary::LineTraceSingleForObjects(
				this,
				Start,
				End,
				Parkour_Grid_Scan_For_Hit_Results_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Out_Hit,
				true
			);

			//Add the "Out_Hit" generated by each for loop iteration to the array "Grid_Scan_Hit_Traces".
			Grid_Scan_Hit_Traces.Add(Out_Hit);
		}
	}
}

void UCustom_Movement_Component::Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()
{
	//In this case it's good practice to empty the arrays at the beginnning of the function so that garagbage data from the previous function call can be wiped out.
	Front_Wall_Top_Edge_Traces.Empty();

	//Before moving on to perform the following for loop the loop index has to be greater than 0 before anything can be done. This is because there is a need for at least two indexes (current and previous)
	//to perform the calculations needed. The goal of the following for loop is to get the distances (from Trace Start to Trace End if there is no impact point and from Trace Start to Impact Point if there is a blocking hit) 
	//of the line traces which were generated in the previous for loop. Once the distances of line traces are calculated for both the current (Index_3) and previous (Index_3 - 1) loop iteration, there is a check to see wheahter there is a blocking hit or not. 
	//Depending on this answer the corresponding value will be assighned to the global variables "Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration" and "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
	//After this there is another check to see if the difference between the two is greater than 7 units. If this is true this means the previous line trace is the ray cast that is is right under the top edge of the wall.
	//In result, said line trace from the previous loop iteration is added to the TArray "Front_Wall_Top_Edge_Traces" and the for loop "continues".
		
	for(int Index{}; Index != Grid_Scan_Hit_Traces.Num(); Index++)
	{
		/*The reason why this "continue" happens at the beginning of this array index based for loop (which handles the calculations for the current and previous index's line trace) is because there needs to be at least two array elements (Index = 1)
		loaded into this for loop for it to work correctly.*/
		if(Index == 0) continue;

		/*Getting the trace distance from the current loop iteration*/
		//Checking to see if the current line trace has a blocking hit or not. Depending on this answer the appropriate 
		//distance value will be assigned to the global variable "Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration".
			
		//If there is a blocking hit this, is the distance from the line trace start to the impact point.
		const float Current_Iteration_Distance_If_Blocking_Hit{Grid_Scan_Hit_Traces[Index].Distance};
				
		//If there is no impact point get the distance between the line trace start and its end.
		const FVector Current_Iteration_Line_Trace_Start{Grid_Scan_Hit_Traces[Index].TraceStart};
		const FVector Current_Iteration_Line_Trace_End{Grid_Scan_Hit_Traces[Index].TraceEnd};
		const double Current_Iteration_Distance_If_No_Blocking_Hit{UKismetMathLibrary::Vector_Distance(Current_Iteration_Line_Trace_Start, Current_Iteration_Line_Trace_End)};

		//Depending on whether  there is a impact point, assign the corresponding value to "Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration".
		if(Grid_Scan_Hit_Traces[Index].bBlockingHit) Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration = Current_Iteration_Distance_If_Blocking_Hit;
		else if(!Grid_Scan_Hit_Traces[Index].bBlockingHit) Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration = Current_Iteration_Distance_If_No_Blocking_Hit;


		/*Getting the trace distance in the previous loop iteration*/
		//Checking to see if the previous line trace has a blocking or not. Depending on this answer the appropriate 
		//distance value will be assigned to the global variable "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
		int Previous_Index{Index -1};

		//Getting a reference to the previous element (in respect to the current iteration of this nested for loop) in the TArray "Grid_Line_Hit_Trace".
		FHitResult& Previous_Iteration_Line_Trace_Reference{Grid_Scan_Hit_Traces[Previous_Index]};

		/*Getting the trace distance in previous loop iteration*/
		//Checking to see if the previous line trace has a blocking hit or not. Depending on this answer the appropriate 
		//distance value will be assigned to the global variable "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
			
		//If there is a blocking hit this, is the distance from the line trace start to the impact point.
		const float Previous_Iteration_Distance_If_Blocking_Hit{Previous_Iteration_Line_Trace_Reference.Distance};

		//If there is no impact point get the distance between the line trace start and its end.
		const FVector Previous_Iteration_Line_Trace_Start{Previous_Iteration_Line_Trace_Reference.TraceStart};
		const FVector Previous_Iteration_Line_Trace_End{Previous_Iteration_Line_Trace_Reference.TraceEnd};
		const double Previous_Iteration_Distance_If_No_Blocking_Hit{UKismetMathLibrary::Vector_Distance(Previous_Iteration_Line_Trace_Start, Previous_Iteration_Line_Trace_End)};

		//Depending on whether  there is a impact point, assign the corresponding value to "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
		if(Previous_Iteration_Line_Trace_Reference.bBlockingHit) Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration = Previous_Iteration_Distance_If_Blocking_Hit;
		else if(!Previous_Iteration_Line_Trace_Reference.bBlockingHit) Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration = Previous_Iteration_Distance_If_No_Blocking_Hit;
			
		//Get the difference between the assigned distances (whether there is a blocking hit or not) of current and the previous line traces.
		const double Distance_Between_Current_And_Previous_Line_Trace{Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration - Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration};

		//If the difference between the assigned distances of current and the previous line traces is greater than 7 units this means the previous line trace is the line is right under the top edge of the wall.
		//In result, said line trace from the previous loop iteration is added to the TArray "Front_Wall_Top_Edge_Traces" and the nested for loop "continues"
		if(Distance_Between_Current_And_Previous_Line_Trace > 7)
		{
			Front_Wall_Top_Edge_Traces.Add(Previous_Iteration_Line_Trace_Reference);
			Debug::Print(FString(TEXT("Differences in Front_Wall_Top_Edge_Traces: ") + FString::FromInt(Distance_Between_Current_And_Previous_Line_Trace)), FColor::Blue, 5);
			//Draw_Debug_Sphere(Previous_Iteration_Line_Trace_Reference.ImpactPoint, 5.f, FColor::Black, 10.f, false, 1);
			continue;
		}

		else continue;
	}

}

void UCustom_Movement_Component::Realize_Front_Wall_Top_Edge_Best_Hit()
{
	//During the first iteration of the loop (Index == 0), assign the first element of the array in "Front_Wall_Top_Edge_Traces" into the global variable "Front_Wall_Top_Edge_Best_Hit".
	//This will only happen one time during this for loop. The global FHitResult variable "Front_Wall_Top_Edge_Best_Hit" needs to have a FHitResult assigned to it so said FHitResult
	//can be compared to the other FHitResults in said array. During each for loop iteration, the FHitResult found in the current element of the array (the current Index) will be compared 
	//to the FHitResult which was assigned to the global FHitResult variable "Front_Wall_Top_Edge_Best_Hit" (during Index == 0). When compared to the current location of the character, 
	//if the FHitResult in the element of the array (the current Index) has a smaller delta than that of the FHitResult stored in the global FHitResult variable 
	//"Front_Wall_Top_Edge_Best_Hit" then the FHitResult in the element of the array (the current Index) will replace the current FHitResult stored in the global FHitResult variable
	//"Front_Wall_Top_Edge_Best_Hit". At the end of the for loop the FHitResult which has the lowest delta when compared to the current location of the character will be stored in the 
	//global FHitResult variable "Front_Wall_Top_Edge_Best_Hit".

	const FVector Current_Copmponent_Location{UpdatedComponent->GetComponentLocation()};
	
	for(int Index{}; Index != Front_Wall_Top_Edge_Traces.Num(); Index++)
	{
		//Initialize the global FHitResult variable with the first element of the array. This will only happen once.
		if(Index == 0) Front_Wall_Top_Edge_Best_Hit = Front_Wall_Top_Edge_Traces[Index];
		
		else
		{	
			//Obtain the locatation of the impact points for the FHitResult stored in the global variable "Front_Wall_Top_Edge_Best_Hit" and the FHitResult which is
			//at the same element of that as the current loop iteration (Index).
			const FVector Current_Front_Wall_Top_Edge_Best_Hit_Location{Front_Wall_Top_Edge_Best_Hit.ImpactPoint};
			const FVector Current_Iteration_Trace_Location{Front_Wall_Top_Edge_Traces[Index].ImpactPoint};
			
			//Obtain the delta of the impact points for the FHitResult stored in the global variable "Front_Wall_Top_Edge_Best_Hit" and the FHitResult which is
			//at the same element of that as the current loop iteration (Index), when compared to the current location of the character.
			const double Delta_Between_Current_Iteration_Trace_Location_And_Component_Location
			{UKismetMathLibrary::Vector_Distance(Current_Iteration_Trace_Location, Current_Copmponent_Location)};
			
			const double Delta_Between_Current_Front_Wall_Top_Edge_Best_Hit_And_Component_Location
			{UKismetMathLibrary::Vector_Distance(Current_Front_Wall_Top_Edge_Best_Hit_Location, Current_Copmponent_Location)};

			//If the FHitResult in the element of the array (the current Index) has a smaller delta than that of the FHitResult stored in the global FHitResult variable 
			//"Front_Wall_Top_Edge_Best_Hit" when compared to the current location of the character, then the FHitResult in the element of the array (the current Index) 
			//will replace the current FHitResult stored in the global FHitResult variable "Front_Wall_Top_Edge_Best_Hit"
			if(Delta_Between_Current_Iteration_Trace_Location_And_Component_Location <= Delta_Between_Current_Front_Wall_Top_Edge_Best_Hit_And_Component_Location)
			{
				Front_Wall_Top_Edge_Best_Hit = Front_Wall_Top_Edge_Traces[Index];
				Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Front_Wall_Top_Edge_Best_Hit.ImpactNormal);
			}
			

			//If the FHitResult in the element of the array (the current Index) does not have a smaller delta than that of the FHitResult stored in the global FHitResult variable 
			//"Front_Wall_Top_Edge_Best_Hit" when compared to the current location of the character, then no change happens regarding which FHitResult is stored in the global 
			//FHitResult variable "Front_Wall_Top_Edge_Best_Hit" and the for loop "continues"
			else continue;
		}	
	}

	//Draw_Debug_Sphere(Front_Wall_Top_Edge_Best_Hit.ImpactPoint, 10.f, FColor::Cyan, 5.f, false, 5.f);
}

void UCustom_Movement_Component::Analyze_Wall_Top_Surface()
{
	//Check to see if the Parkour State is set to climb. If this is the case call the function "Calculate_Wall_Top_Surface()".
	//Said function will generate the traces for the top of the wall and the depth of the wall.

	//If the Parkour State is not set to climb then the character is not climbing and is either falling or on the ground. Therefore, before 
	//calling the function "Calculate_Wall_Top_Surface()" the front wall which is being analyzed must have "its normal reversed on the Z axis" 
	//via the FHitResult "Front_Wall_Top_Edge_Best_Hit" using the helper function created "Reverse_Wall_Normal_Rotation_Z()". 
	//This is how the character will know which direction to perform the parkour action.
	
	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		Calculate_Wall_Top_Surface();
	}
	
	else if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Front_Wall_Top_Edge_Best_Hit.ImpactNormal);
		Calculate_Wall_Top_Surface();
	}
}

void UCustom_Movement_Component::Calculate_Wall_Top_Surface()
{
	FHitResult Out_Hit_1{};
	FHitResult Out_Hit_2{};
	
	FHitResult Top_Wall_Last_Hit{};

	//This for loop throws sphere traces downwards, starting from an elevated offset location which originates from the global FHitResult "Front_Wall_Top_Edge_Best_Hit".
	//the starting location of each sphare trace will be moved forward 30 units from the previous sphere trace. There will be a maximum of nine sphere traces generated.
	for(int Index{}; Index <= 8; Index++)
	{
		//Multiplier used to move each sphere trace forward 30 units from the sphere trace which was generated during the previous for loop iteration (considering the Index is not 0).
		const int Index_Multiplier{Index * 30};
		
		//Move the vector forward from its staring location which is the FHitResult "Front_Wall_Top_Edge_Best_Hit" impact point using the value in "Index_Multiplier". To make the vector move on the charater's forward direction
		//"the normal of the wall must be reversed". To do this the global FRotator variable "Reversed_Front_Wall_Normal_Z" is used. This variable is filled with the current wall that is being 
		//analyzed reversed normal on the Z axis. This happens in the function "&UCustom_Movement_Component::Analyze_Wall_Top_Surface()".
		const FVector Move_Vector_Forward_With_Each_Iteration_Of_Loop{Move_Vector_Forward(
			Front_Wall_Top_Edge_Best_Hit.ImpactPoint, 
			Reversed_Front_Wall_Normal_Z, 
			Index_Multiplier)};
		
		const FVector Start{Move_Vector_Up(Move_Vector_Forward_With_Each_Iteration_Of_Loop, 15.f)};
		const FVector End{Move_Vector_Down(Start, 20.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Analyzing_Top_Wall_Surface_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Out_Hit_1,
			false
			);
		
		//If there is no blocing hit on any of the sphere traces break out of the for loop. This is because the top surface of the wall which is being analyzed had dropped below the threshold which is desired (Sphere trace "End").
		if(!Out_Hit_1.bBlockingHit)
		break;

		//If the index is 0 and there is a blocking hit on the Out_Hit of the current loop iteration (Index = 0), as well as "!bStartPenetrating", assign said FHitResult to the global FHitResult variable "Wall_Top_Result". 
		//This is the first and closest FHitResult to the charater on the top surface of the wall which is being analyzed.
		else if(Index == 0 && (Out_Hit_1.bBlockingHit && !Out_Hit_1.bStartPenetrating))
		{
			Wall_Top_Result = Out_Hit_1;
			//Draw_Debug_Sphere(Wall_Top_Result.ImpactPoint, 15.f, FColor::Emerald, 7.f, false, 7.f);
		}
		
		//If the current loop iteration is not 0 then assign every other hit result to the local FHitResult variable "Top_Wall_Last_Hit". With each for loop iteration the data in this variable will be overitten and in result by the end of the 
		//for loop the last hit will be stored in said local variable. The FHitResult stored in this local variable will be used to calculate the global FHitResult "Wall_Depth_Result".
		else if(Index != 0 && Out_Hit_1.bBlockingHit)
		{
			Top_Wall_Last_Hit = Out_Hit_1;
			//Draw_Debug_Sphere(Top_Wall_Last_Hit.ImpactPoint, 10.f, FColor::Magenta, 7.f, false, 7.f);
		}
		
	}

	//If the "Parkour_State" is set to "Parkour.State.Free.Roam" then the character is on the ground. Therefore, a calculation must be made to determine what data to store in the global
	//FHitResult variable "Wall_Depth_Result". The data stored in said global variable will be used to determine the data to store in the other global varable "Wall_Vault_Result".
	//This only needs to be done when the "Parkour_State" is set to "Parkour.State.Free.Roam" because the FHitResults stored in the two global variables "Wall_Depth_Result" and "Wall_Vault_Result"
	//are only useful for determining which vault action to perform. Vault actions can only be performed when the "Parkour_State" is set to "Parkour.State.Free.Roam" (the character is on the ground).
	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	{
		//The goal with this sphere trace is to execute a ray cast which starts at an offset position from the FHitResult "Top_Wall_Last_Hit". This offset should be alligned with the normal stored in the global variable
		//"Reversed_Front_Wall_Normal_Z". So visually, it should start in the same "trajectory" which the sphere traces in the previous for loop were being generated in. This sphere trace should end where the local FHitResult 
		//"Top_Wall_Last_Hit" impact point is. With a radius just big enough, it should collide with the wall which is being analyzed, on the opposite side from where the character is (this sphere trace should be launched towards 
		//the character from the other side of the wall being analyzed).
		const FVector Start{Move_Vector_Forward(Top_Wall_Last_Hit.ImpactPoint, Reversed_Front_Wall_Normal_Z, 30.f)};
		const FVector End{Top_Wall_Last_Hit.ImpactPoint};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			10.f,
			Parkour_Wall_Depth_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Out_Hit_2,
			false
		);

		//Store the sphere trace performed in the global FHitResult variable "Wall_Depth_Result"
		Wall_Depth_Result = Out_Hit_2;
		//Draw_Debug_Sphere(Wall_Depth_Result.ImpactPoint, 10.f, FColor::Purple, 10.f, false, 10.f);
	}
}

void UCustom_Movement_Component::Calculate_Wall_Vault_Location()
{
	//This function calculates the vault location in which the character will land on.
	//The starting position of this sphere trace is an offset from the impact point of the FHitResult 
	//which is stored in the global variable "Wall_Depth_Result". The normal stored in "Reversed_Front_Wall_Normal_Z"
	//is used to set the vector normal be the same as that of the front wall's reversed normal on the Z axis.
	//This vector is moved forward 70 units (away from the character since its normal on the z axis is reversed using 
	//"Reversed_Front_Wall_Normal_Z". This offset is the trace start. From this location the vector is sent downwards 200 units (to make sure it hit a surface).
	//This location is the trace end location.
	
	FHitResult Out_Hit{};

	const FVector Start{Move_Vector_Forward(Wall_Depth_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 70.f)};
	const FVector End{Move_Vector_Down(Start, 200.f)};
	
	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		Parkour_Vault_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
	);

	//Store the FHitResult generated by the sphere trace into the global FHitResult variable "Wall_Vault_Result".
	Wall_Vault_Result = Out_Hit;

	//Draw_Debug_Sphere(Wall_Vault_Result.ImpactPoint, 10.f, FColor::Silver, 10.f, false, 10.f);
}

double UCustom_Movement_Component::Calculate_Wall_Height()
{
	if(Wall_Top_Result.bBlockingHit)
	{
		const FVector Character_Root_Bone_Location{Mesh->GetSocketLocation(FName(TEXT("root")))};
		const FVector Wall_Top_Result_Trace_Location{Wall_Top_Result.ImpactPoint};
		const double  Height_Delta_Between_Character_Root_Bone_Location_And_Wall_Top_Result_Trace_Location
		{Wall_Top_Result_Trace_Location.Z - Character_Root_Bone_Location.Z};
		
		Debug::Print(FString("Wall_Height: ") + FString::SanitizeFloat(Wall_Height), FColor::MakeRandomColor(), 34);
		
		return Wall_Height = Height_Delta_Between_Character_Root_Bone_Location_And_Wall_Top_Result_Trace_Location;
		
	}

	else
	{
		Debug::Print(FString("Wall_Height: ") + FString::SanitizeFloat(Wall_Height), FColor::MakeRandomColor(), 34);

		return Wall_Height = 0.f;
	}
}

double UCustom_Movement_Component::Calculate_Wall_Depth()
{
	if(Wall_Depth_Result.bBlockingHit)
	{
		const FVector Wall_Depth_Result_Trace_Location{Wall_Depth_Result.ImpactPoint};
		const FVector Wall_Top_Result_Trace_Location{Wall_Top_Result.ImpactPoint};
		const double  Distance_Delta_Between_Wall_Depth_Result_Trace_Location_And_Wall_Top_Result_Trace_Location
		{UKismetMathLibrary::Vector_Distance(Wall_Depth_Result_Trace_Location, Wall_Top_Result_Trace_Location)};

		Debug::Print(FString("Wall_Depth: ") + FString::SanitizeFloat(Wall_Depth), FColor::MakeRandomColor(), 35);

		return Wall_Depth = Distance_Delta_Between_Wall_Depth_Result_Trace_Location_And_Wall_Top_Result_Trace_Location;
	}
		
	else
	{
		Debug::Print(FString("Wall_Depth: ") + FString::SanitizeFloat(Wall_Depth), FColor::MakeRandomColor(), 35);
		
		return Wall_Depth = 0;
	} 
}

double UCustom_Movement_Component::Calculate_Vault_Height()
{
	if(Wall_Vault_Result.bBlockingHit)
	{
		const FVector Wall_Vault_Result_Trace_Location{Wall_Vault_Result.ImpactPoint};
		const FVector Wall_Depth_Result_Trace_Location{Wall_Depth_Result.ImpactPoint};
		const double  Height_Delta_Between_Wall_Depth_Result_Trace_Location_And_Wall_Vault_Result_Trace_Location
		{Wall_Depth_Result_Trace_Location.Z - Wall_Vault_Result_Trace_Location.Z};

		Debug::Print(FString("Vault_Height: ") + FString::SanitizeFloat(Vault_Height), FColor::MakeRandomColor(), 37);
		
		return Vault_Height = Height_Delta_Between_Wall_Depth_Result_Trace_Location_And_Wall_Vault_Result_Trace_Location;
	}
		
	else
	{
		Debug::Print(FString("Vault_Height: ") + FString::SanitizeFloat(Vault_Height), FColor::MakeRandomColor(), 37);
		
		return Vault_Height = 0;
	} 
}

void UCustom_Movement_Component::Validate_bIs_On_Ground()
{
	if(!Mesh)
	return; 
		
	/*This function will be called every "Tick()" within the function "Parkour_Call_In_Tick()". The goal of this function is to determine whether the character is 
	grounded or airborne. By using a BoxTraceSingleForObjects that generates a box ray cast that is located at the same location as the root bone of the character 
	the result of whether there is a blocking hit or not stored in the local FHitResult variable "Out_Hit" can be used to set the value of the global bool variable 
	"bIs_On_Ground".*/

	//If the current "Parkour_State" set on the character is "Parkour.State.Climb" then it is evident that the character is not grounded. In this case the global bool
	//variable "bIs_On_Ground" will be set to false.
	/* if(Parkour_State = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))) || 
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))) || 
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		bIs_On_Ground = false;
	} */

	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	{
		bIs_On_Ground = false;
	}

	else
	{
		FHitResult Out_Hit{};

		UKismetSystemLibrary::BoxTraceSingleForObjects(
			this,
			Mesh->GetSocketLocation(FName(TEXT("root"))),
			Mesh->GetSocketLocation(FName(TEXT("root"))),
			FVector(25, 25, 25),
			UKismetMathLibrary::MakeRotFromX(UpdatedComponent->GetForwardVector()),
			Parkour_Validate_On_Land_Trace_Types,
			false,
			Wall_Pipe_Actors_To_Ignore,
			Debug_Action,
			Out_Hit,
			false
		);

		if(Out_Hit.bBlockingHit)
		bIs_On_Ground = true;

		else
		bIs_On_Ground = false;
	}

}

void UCustom_Movement_Component::Decide_Climb_Style(const FVector& Impact_Point, const FRotator& Direction_For_Character_To_Face)
{
	/*The goal of this function is to use the impact point of the global FHitResult "Wall_Top_Result" and the direction of 
	the global FRotator "Reversed_Front_Wall_Normal_Z as the cornerstone of a new sphere trace. The location filled into the 
	input parameter "Impact_Point" is that of the global FHitResult "Wall_Top_Result" and the Rotation filled into the input
	argument "Direction_For_Character_To_Face" is that of the global FRotator "Reversed_Front_Wall_Normal_Z. 
	Using the helper function "Move_Vector_Down()", this location will be moved down 125 units. Next, from that location 
	it will be moved backwards 10 units using the helper function "Move_Vector_Backwards (this is the start location of 
	the sphere trace). Finally from the start location of the sphere trace the vector will be moved forward 25 units 
	(this will be the end location of the sphere trace). If the local FHitResult "Out_Hit" has a blocking hit then this 
	means there is a wall in front of the character (at the height of where the feet will be during a Braced_Climb) and 
	the climb style should be braced. If there is no blocking hit then this means there is no wall in front of the character 
	(at the height of where the feet will be during a Braced_Climb) and the climb style should be Free_Hang. The idea of this 
	sphere trace is to cast a ray trace from the height of where the character's feet will be during a braced climb to 
	determine if there is a wall there for the feet to land on during a Braced_Climb.*/
	
	FHitResult Out_Hit{};
	const FVector Move_Vector_Down_To_Feet_Level{Move_Vector_Down(Impact_Point, 125)};
	const FVector Start{Move_Vector_Backward(Move_Vector_Down_To_Feet_Level, Direction_For_Character_To_Face, 10.f)};
	const FVector End{Move_Vector_Forward(Start, Direction_For_Character_To_Face, 25.f)};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		Parkour_Decide_Climb_Style_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
	);
	
	if(Out_Hit.bBlockingHit)
	{
		Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))));
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}
	
	else if(!Out_Hit.bBlockingHit)
	{
		Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))));
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

}

bool UCustom_Movement_Component::Parkour_Climb_State_Detect_Wall(FHitResult& Parkour_Climbing_Detect_Wall_Hit_Result, FHitResult& Parkour_Climbing_Wall_Top_Result)
{
	//The main goal of this function is to detect whether or not there is a wall infront of the character when the global FGameplayTag "Parkour_State" is set to 
	//"Parkour.State.Climb". When the character is shimmying across the surface of a wall the mountable surface of the wall may end and therefore the charcater will need to
	//stop moving. There are two sphere traces which happen in this function. The sphere trace in the outer for loop handles determining whether or not there is a wall 
	//ahead of the character. The sphere trace in the inner for loop uses the impact point of the sphere trace of the outer for loop to determine the wall top result while
	//shimmying across the wall. This sphere trace also handles determining whether or not there is another enough space above the surface of the hands to continue shimmying. 

	//Since the value stored in the global double variable "Right_Left_Movement_Value" has a maximum of 1 (value represents the input from the controller for whether the character should move to the 
	//left or right with the value 1 being full input in the respective direction and 0 being no input for the character to move), whatever value is stored in said global variable will be multiplied
	//by 10 within the local double variable "Horizontal_Move_Direction_Update". 
	const float Right_Left_Movement_Value_Multiplier{10.f};

	//Get the value which the controller is putting into the global double value "Right_Left_Movement_Value" and multiply it by 10. This product is used as an offset value to begin generating the ray 
	//casts in the outer for loop. If the value is a negative number the ray cast will be on the left side of the arrow actor (which means the character is moving to the left). If the value is not
	//a negative number then the character is moving to the right and the sphere traces will be on the right side of the arrow actor. 
	const double Horizontal_Move_Direction_Update{Right_Left_Movement_Value * Right_Left_Movement_Value_Multiplier};

	//Offset the start location of the sphere trace according to the current FGameplayTag set on the global FGameplayTag variable "Parkour_Climb_Style".
	//This needs to happen because during shimmying across some surfaces the character moves to close to the surface of the wall. If this happens then the sphere trace
	//will not get a blocking hit abnd in result no wall will be detected.
	const float Value_To_Offset_Sphere_Trace_Backwards{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 10.f, 30.f)};

	int Index_1{};
	//This sphere trace generates sphere traces to the right or left of the arrow actor which is placed right ontop of the character. The side of the arrow actor which
	//the sphere traces are generated is determined by the value placed into the global double variable "Right_Left_Movement_Value". The value here is calculated by the input
	//into the controller within the function "&UCustom_Movement_Component::Add_Movement_Input" Three sphere traces are generated downwards until there is a blocking hit. If no
	//all sphere traces are generated and there is no blocking hit, then this means there is no surface which the character can shimmy across. If ".bStartPenetrating" is true
	//during any iteration of this for loop then this means the character is too close to the surface. In both instances "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables", 
    //"Reset_Movement_Input_Variables" and "return" should be called.
	for(Index_1; Index_1 <= 7; Index_1++)
	{
		//Get locattion of the Arrow actor.
		const FVector Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
		//Get the rotation of the Arrow actor
		const FRotator Arrow_Direction{Character_Direction_Arrow->GetActorRotation()};
		
		//Offset the location of the sphere trace start backwards according to the current FGameplayTag set on the global FGameplayTag variable "Parkour_Climb_Style". 
		const FVector Offset_Start_1{Move_Vector_Backward(Arrow_Location, Arrow_Direction, Value_To_Offset_Sphere_Trace_Backwards)};
		//Offset the location to the sphere trace to the right or left depending on the value stored in the local double variable "Horizontal_Move_Direction_Update".
		//The value stored in this variable is dependent on the input from the controller.
		const FVector Offset_Start_2{Move_Vector_Right(Offset_Start_1, Arrow_Direction, Horizontal_Move_Direction_Update)};
		//During each iteration of the loop move the sphere trace down by 10 units.
		const FVector Start{Move_Vector_Down(Offset_Start_2, Index_1 * 10)};
		//The sphere trace should have a lenght of 80.
		const FVector End {Move_Vector_Forward(Start, Arrow_Direction, 80)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Climbing_Detect_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Climbing_Detect_Wall_Hit_Result,
			false
			);

		//If ".bStartPenetrating" is true "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables", "Reset_Movement_Input_Variables" and "return" should be called. This is because the character
		//is too close to the surface of the wall and in result can't continue shimmying.  
		if(Parkour_Climbing_Detect_Wall_Hit_Result.bStartPenetrating)
		{
			Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
			return false;
		}

		//If ".bStartPenetrating" is false, then continue to check if the HitResult of the current loop iteration has a ".bBlockingHit". 
		else
		{
			//If the HitResult of the outer for loop iteration has a blocking hit continue to develop another sphere trace algorithem which will obtain the 
			//the "Parkour_Climbing_Wall_Top_Result".
			if(Parkour_Climbing_Detect_Wall_Hit_Result.bBlockingHit)
			{
				//Replace the current reversed wall normal stored in the global variable "Reversed_Front_Wall_Normal_Z" with the Reversed Wall Normal which may be calculated 
                //with the FHitResult from the outer for loop. This will be then new "Reversed_Front_Wall_Normal_Z" for as long as the character is shimmying.
				Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Parkour_Climbing_Detect_Wall_Hit_Result.ImpactNormal);

				//This for loop generates a sphere trace which shoots downwards from a forward and upward offset position of the "Parkour_Climbing_Detect_Wall_Hit_Result.ImpactPoint". The goal of this for loop is to perform a ray trace which starts inside
				//of the wall (infront and slightly upwards from the local FHitResult variable "Parkour_Climbing_Detect_Wall_Hit_Result"). With every iteration of the for loop the ray trace starting position will move up by five units. This is to obtain the new  
				//"Parkour_Climbing_Wall_Top_Result" (the const reference input parameter). If ".bStartPenetrating" is true and the outer for loop and this inner for loop are both complete then "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" 
				//and "return" will be called. In this outcome there is no wall top to be obtained and in result the character will not be able to move via the "Move_Character_To_New_Climb_Position_Interpolation_Settings" function. Said function uses the "Parkour_Climbing_Wall_Top_Result"
				//as the location to to move the character (on the "Z" axis) while simultaneously using "Parkour_Climbing_Detect_Wall_Hit_Result" as the location to move the characer to on the "X" and "Y" axis. Also, if ".bStartPenetrating" is true and both the outer and inner for loop are
				//complete then this means that there is no room above the hands for the character to shimmy. Due to the starting point of the sphere trace raising on the "Z" axis with every iteration of the for loop until until the threshold is reached the character will have the opportunity 
				//to shimmy up and down surface ledges. Once there is a blocking hit on this trace "break" will be called as the "Parkour_Climbing_Wall_Top_Result" will be stored, hence ther is room for the character to shimmy. Remember, this trace happens at the same "Y" location as the 
				//location stored in the FHitResult "Parkour_Climbing_Detect_Wall_Hit_Result". This means when the character is moveing to the left this sphere trace will be on the left side of the arrow actor (which is just above the character) and when the character is moveing to the left 
				//the sphere trace will be on the left side of the arrow actor. This is calculated using the local double variable "Horizontal_Move_Direction_Update" which is the global double variable "Right_Left_Movement_Value" multiplied by the input parameter 
				//"Right_Left_Movement_Value_Multiplier".

				int Index_2{};
				for(Index_2; Index_2 <= 7; Index_2++)
				{
					//Offset the vector from the hit result of the outer for loop "Parkour_Climbing_Detect_Wall_Hit_Result.ImpactPoint" forward (in front of the character) by two units. At this location the vector will be inisde of the wall.
					const FVector Nested_For_Loop_Offset_Start_1{Move_Vector_Forward(Parkour_Climbing_Detect_Wall_Hit_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 2.f)};
					//Offset the vector from the hit result of the outer for loop "Parkour_Climbing_Detect_Wall_Hit_Result.ImpactPoint" up by 5 units. At this location the vecor will be a bit under the top surface of the wall (still inside the wall).
					const FVector Nested_For_Loop_Offset_Start_2{Move_Vector_Up(Nested_For_Loop_Offset_Start_1, 5.f)};
					//With each iteration of this inner nested for loop move the start location up by five units from its previous start location.
					const FVector Nested_For_Loop_Start{Move_Vector_Up(Nested_For_Loop_Offset_Start_2, Index_2 * 5)};
					//The height of the sphere trace will be 30 units.
					const FVector Nested_For_Loop_End{Move_Vector_Down(Nested_For_Loop_Start, 30.f)};

					UKismetSystemLibrary::SphereTraceSingleForObjects(
						this,
						Nested_For_Loop_Start,
						Nested_For_Loop_End,
						3.f,
						Parkour_Climbing_Wall_Top_Result_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Parkour_Climbing_Wall_Top_Result,
						false
						);

					//If ".bStartPenetrating" is true and the outer for loop and this inner for loop are both complete then "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return" will be called. 
					//In this outcome there is no wall top to be obtained and in result the character will not be able to move via the "Move_Character_To_New_Climb_Position_Interpolation_Settings" function. Otherwise "continue" will be called.
					//This is because the threshold for the maximum height that the character can reach up to shimmy up a ledge hasn't been reahced. Otherwise "continue" will be called so that the the wall may continue to be analyzed.
					if(Parkour_Climbing_Wall_Top_Result.bStartPenetrating)
					{
						if(Index_1 == 7 && Index_2 == 7)
						{
							Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
							return false;
						}
						
						else
						{
							continue;
						}
					}

					//If ".bStartPenetrating" is false, check to see if the there is a blocking hit on the FHitResult 'Parkour_Climbing_Wall_Top_Result". If there is a blocking hit, return out of this function as the wall has been analyzed and in result the
					//"Parkour_Climbing_Detect_Wall_Hit_Result" and the "Parkour_Climbing_Wall_Top_Result" has both been obtained. If there is no blocking hit for the FHitResult "Parkour_Climbing_Wall_Top_Result", then check to see if both the outer and inner
					//for loop are complete. If both for loops are complete, call "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return". Otherwise call "continue" so the wall may be be analyzed further.
					else
					{
						if(Parkour_Climbing_Wall_Top_Result.bBlockingHit)
						{
							return true;
						}

						else
						{
							if(Index_1 == 7 && Index_2 == 7)
							{
								Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
								return false;
							}
						
							else
							{
								continue;
							}
						}
					}
				}
			}

			//If the HitResult of the outer loop iteration does not have a blocking hit check to see if the loop has completed. If this is the case call "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables"
			//and "return". Otherwise call "continue".
			else
			{
				if(Index_1 == 7)
				{
					//Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
					return false;
				}
					
				else
				{
					continue;
				}
			}
		}
	}

	return false;
}

bool UCustom_Movement_Component::Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Hands(const FVector& Starting_Impact_Point)
{
	/*This function develops line traces from an offset location above the FHitResult "Parkour_Climbing_Wall_Top_Result" (declared as a local variable within the function "Parkour_Climb_Handle_Shimmying_Movement"). Because said line traces
	use "Parkour_Climbing_Wall_Top_Result" as their starting point, the line traces will be generated on the side of the arrow actor that the character is shimmying. So if the character is moving to the left, the line traces will be on the
	left side of the arrow actor (offset just above the hands). Same goes for the right side. This is because "Parkour_Climbing_Wall_Top_Result" uses "Parkour_Climbing_Detect_Wall_Hit" as its starting point and "Parkour_Climbing_Detect_Wall_Hit 
	is generated on the side of the arrow actor which the character is moving. Said location is calculated by moving the vector to the right side of the arrow actor (or left if the value is negative) by using the helper function "Move_Vector_Right" 
	(passing in the "Right_Left_Movement_Value" multiplied by the "Right_Left_Movement_Value_Multiplier"). With each iteration of the for loop performed the line trace start point will be raised up five units from its previous start location. However, the line
	traces will only be generated if there is a blocking hit on the line trace which is generated on the first iteration of the for loop. If the first line trace which is generated does not have a blocking hit then this means there is no obstacle 
	on the respective side of the character's hands (the hand on the side of the body in which the character is shimmying). If there is a blocking hit on the first line trace which is generated, then the for loop will continue to generate more line
	traces which will all start five units above the previous line trace. This will happen until the threshold is reahced (which means that there is a obsticle on the side of the characters hands) and in result "true" should be returned 
	(which results in "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return" being called within the scope of the if chech which this function is used in within the function "Parkour_Climb_Handle_Shimmying_Movement") or until there
	is no blocking hit on the most recent line trace. In which case "break" will be called.*/
	
	//Offset the starting location of the line trace two units above the input parameter "Starting_Impact_Point". This location will be the "Parkour_Climbing_Wall_Top_Result".
	const FVector Offset_Starting_Impact_Point{Move_Vector_Up(Starting_Impact_Point, 2.f)};
	
	int Index{};
	FHitResult Parkour_Climbing_Check_Sides{};

	for(Index; Index <= 5; Index++)
	{
		const FVector Start{Move_Vector_Up(Offset_Starting_Impact_Point, Index * 5)};
		const FVector End{Move_Vector_Right(Start, Reversed_Front_Wall_Normal_Z, Right_Left_Movement_Value * 45.f)};

		UKismetSystemLibrary::LineTraceSingleForObjects(
			this,
			Start,
			End,
			Parkour_Climbing_Check_Sides_Of_Hands_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Climbing_Check_Sides,
			false
			);
		
		//Check to see if the line traced generated has a blocking hit. If it does have a blocking hit, check to see if the for loop is complete. If the for loop is complete, this means the threshold has been reached and there is a obstacle on the respective
		//side of the character's hands. in this case true will be returned which will lead to "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return" being called in the scope of the if chech that uses this function within the
		//fucntion "Parkour_Climb_Handle_Shimmying_Movement". If the for loop is not complete "continue" is called so the area on the respective side of the character's hands may be analyzed for obstacles.

		//IF there is no blocking hit on any line trace which is generated before the threshold is reahced "false" will be retruned.
		if(Parkour_Climbing_Check_Sides.bBlockingHit)
		{
			if(Index == 5)
			{
				return true;
			}
			
			else
			{
				continue;
			}
		}

		else
		{
			return false;
		}
	}

	//Default return value. This line is needed to meet the requirements of this function return type.
	return true;
}

bool UCustom_Movement_Component::Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Body(const FVector& Movemement_Impact_Location) const
{
	/*This function develops a dynamic a ray trace on the side of the characters body (the same side the character is shimmying) to determine if there is a obsticle which the character shouldn't be able to shimmy across.
	The starting location for the capsule trace is ofset to to the right side or left side (depending which direction the character is moving), as well as down depending on the "Parkour_Climb_Style". which the character is currently 
	in.*/

	//Dynamically set the height of the capsule trace based on the "Parkour_Climb_Style"
	const float Capsule_Trace_Dynamic_Down_Offset_And_Height{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 55.f, 90.f)};
	//Dynamically set the start location of the capsule trace based on the "Parkour_Climb_Style"
	const float Capsule_Trace_Dynamic_Start{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 30.f, 7.f)};
	////Dynamically set the end location of the capsule trace based on the "Parkour_Climb_Style"
	const float Capsule_Trace_Dynamic_End{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 40.f, 15.f)};
	
	FHitResult Out_Hit{};
	
	//The arrow actor is always facing the same direction as the character.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};
	
	//The sphere trace is offset to the right or left side of the arrow actor. Depending on the direction the character is shimmying (based on "Right_Left_Movement_Value" which is set within the 
	//function &UCustom_Movement_Component::Add_Movement_Input).
	const FVector Offset_Vector_To_The_Right{Move_Vector_Right(Movemement_Impact_Location, Direction_Character_Is_Facing, Right_Left_Movement_Value * 35.f)};
	
	//The sphere trace is offset down dynamically from the "Offset_Vector_To_The_Right" location.
	const FVector Offset_Vector_Down{Move_Vector_Down(Offset_Vector_To_The_Right, Capsule_Trace_Dynamic_Down_Offset_And_Height)};
	
	//The start location of the capsule trace location is set dynamically based on the "Parkour_Climb_Style".
	const FVector Start{Move_Vector_Backward(Offset_Vector_Down, Direction_Character_Is_Facing, Capsule_Trace_Dynamic_Start)};
	//The end location of the capsule trace location is set dynamically based on the "Parkour_Climb_Style"
	const FVector End{Move_Vector_Backward(Start, Direction_Character_Is_Facing, Capsule_Trace_Dynamic_End)};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		Capsule_Trace_Dynamic_Down_Offset_And_Height,
		Parkour_Climbing_Check_Sides_Of_Body_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
		);
	
	//If there is no a blocking hit "false" will be returned and the character will be allowed to shimmy in that direction.
	if(!Out_Hit.bBlockingHit)
	return false;

	//If there is a blocking hit "true" will be returned and in result "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return" will be called within the scope of 
	//the if check in which this function is used within "&UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement"
	else
	return true;
}

void UCustom_Movement_Component::Parkour_Climb_Initialize_IK_Hands(const bool& bIs_Left_Hand)
{
	/*This function handles the implementation of the IK hands when "Parkour_State" changes to "Parkour.State.Climb". When the input parameter is set to "true" when this
	function is called the implementation of this function will handle the left hand and when it is set to false the right hand will be handled.*/
	
	//This function will be called only when a montage loaded into the function "&UCustom_Movement_Component::Play_Parkour_Montage"  has "Parkour.State.Ready.To.Climb" as the the "In_State" of its Data Asset object.
	//Within the montage an animation notify state is set which triggeres this function within the the class  "UInitiialize_IK_Libs". Also, the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" must have 
	//a blocking hit. This is because this FHitResult will be used to initialize the starting location of the ray casts to be performed within this funtion.
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Ready.To.Climb"))) || !Initialize_Parkour_IK_Limbs_Hit_Result.bBlockingHit)
	return;

	int Index_1{};
	//The product of this varibale will be used to offset the location of the ray cast being performed and in result offset the location of the respective hand during each loop.
	const int Index_1_Multiplier{Index_1 * 2};
	
	//This value represents how many units away from the the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" the respective hand should be when the algorithm in this function is complete.
	int Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result{};

	if(bIs_Left_Hand == true)
	Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result = -15;

	else
	Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result = 18;
	
	//This value represents in which direction the respective limb should move. Because the helper function "Move_Vector_Right" will be used in the Offset_Vector location, if the respective hand
	//is the left hand it needs to be moved to the right so the value to move the hand will be multiplied by 1, if the respective hand is the right hand it will need to be moved to the left so the
	//value to move the hand "(Index_1_Multiplier - Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result)" will need to be multipled by -1. This will invert the result of the
	//"Move_Vector_Right" helper funtion
	int Select_Left_Or_Right_Hand_Value{};

	if(bIs_Left_Hand == true)
	Select_Left_Or_Right_Hand_Value = 1;

	else
	Select_Left_Or_Right_Hand_Value = -1;


	//This is the value to move the respective hand during each iteration of the algorithm.
	const int Move_Vector_For_Right_Or_Left_Hand_Value{Select_Left_Or_Right_Hand_Value * (Index_1_Multiplier - Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result)};

	FHitResult Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result{};

	FRotator Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z{};

	//Declaring the local FRotator and FVector which will store the location and rotation of the respective hand. The data stored in these variables will be passed in via the "IParkour_Locomotion_Interface" functions
	//"Set_Left_Hand_Shimmy_Location" and "Set_Left_Hand_Shimmy_Rotationn"
	FRotator Hand_Shimmy_Rotation{};   
	FVector Hand_Shimmy_Location{};     

	/*Obtain the "Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result"*/

	for(Index_1; Index_1 <= 4; Index_1++)
	{
		const FVector Offset_Vector{Move_Vector_Right(Initialize_Parkour_IK_Limbs_Hit_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, Move_Vector_For_Right_Or_Left_Hand_Value)};
		
		const FVector Start_1{Move_Vector_Backward(Offset_Vector, Reversed_Front_Wall_Normal_Z, 30.f)};
		const FVector End_1{Move_Vector_Forward(Start_1, Reversed_Front_Wall_Normal_Z, 70.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start_1,
			End_1,
			10.f,
			Parkour_Shimmying_Initialize_IK_Hands_Detect_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result,
			false
		);

		//If there is a blocking hit reverse the normal of the impact point on the Z axis by 180 degrees using the helper funtion "Reverse_Wall_Normal_Rotation_Z". This reversed normal will be used within this function alone
		//to set the rotation of the respective hand as well as offset any vectors which need offseting.
		if(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.bBlockingHit)
		{
			Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.ImpactNormal);

			//These are the best values to rotate the respecive hand which were realized via debugging.
			FRotator Hand_Shimmy_Rotation_Best_Rotation_Values{};
			
			if(bIs_Left_Hand)
			Hand_Shimmy_Rotation_Best_Rotation_Values = FRotator(50, 0, 270);
			
			else
			Hand_Shimmy_Rotation_Best_Rotation_Values = FRotator(230, 0, 270);

			//Use the reversed normal to set the rotation of the respective hand into the local FRotator variable "Hand_Shimmy_Rotation"
			Hand_Shimmy_Rotation = Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z + Hand_Shimmy_Rotation_Best_Rotation_Values;


			int Index_2{};

			FHitResult Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result{};

			/*Obtain the "Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result"*/

			for(Index_2; Index_2 <= 7; Index_2++)
			{
				const FVector Nested_Loop_Ofsset_Vector{Move_Vector_Forward(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z, 2.f)};

				const FVector Start_2{Move_Vector_Up(Nested_Loop_Ofsset_Vector, Index_2 * 5.f)};
				const FVector End_2{Move_Vector_Down(Start_2, 30.f)};

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					this,
					Start_2,
					End_2,
					3.f,
					Parkour_Shimmying_Initialize_IK_Hands_Wall_Top_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result,
					false
				);

				//If there is a blocking hit store the location of the impact point within the local FVector "Hand_Shimmy_Location". If the location needs to be offset backwards, forwards etc.
				//use the helper function along with the global FRotator "Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z" (if its needed) then "break" out of the inner for loop.
				//The location needs to be set for all the "Parkour_Climb_Style" because each one has different had placements on the surface which is being shimmied.
				if(Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.bBlockingHit && !Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.bStartPenetrating)
				{
					if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
					{
						const FVector Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result{Move_Vector_Backward(
																								  Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.ImpactPoint, 
																								  Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z,
																								  5.f)};
				
						Hand_Shimmy_Location = FVector(Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.X,
											  		   Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.Y, 
											   	       Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.Z + 3.f);
						break;
					}

					else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
					{
						const FVector Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result{Move_Vector_Backward(
																								  Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.ImpactPoint, 
																								  Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z,
																								  5.f)};
				
						Hand_Shimmy_Location = FVector(Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.X,
											  		   Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.Y, 
											   	       Offset_Initialize_Parkour_Shimmying_IK_Hands_Wall_Top_Hit_Result.Z - 3.f);
						break;
					}
				}

				//If there is no blocking hit check to see if the inner algorithm has reached its maximum threshold. If it has return, otherwise continue the inner algorithm.
				else
				{
					if(Index_2 == 7)
					return;

					else
					continue;
				}
			}

			//The rotation and the location of the respective hand has been realized. Therefore break out of the outer for loop as well.
			break;
		}	

		//If there is no blocking hit check to see if the outer algorithm has reached its maximum threshold. If it has return, otherwise continue the outer algorithm.
		else
		{
			if(Index_1 == 4)
			return;

			else
			continue;
		}
	}	

	if(bIs_Left_Hand == true)
	{
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Location(Anim_Instance, Hand_Shimmy_Location);
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Rotation(Anim_Instance, Hand_Shimmy_Rotation);
		return;
	}

	else
	{
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Location(Anim_Instance, Hand_Shimmy_Location);
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Rotation(Anim_Instance, Hand_Shimmy_Rotation);
		return;
	}
}

void UCustom_Movement_Component::Parkour_Climb_Dynamic_IK_Hands(const bool& bIs_Left_Hand)
{
	//This function is called within "UCustom_Movement_Component::Parkour_Call_In_Tick". Therefore, a check to make sure the value set within the global FGameplayTag "Parkour_State" is "Parkour.State.Climb" should be executed.
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	return;

	//Get the current rotation of the character. The "Characer_Direction_Arrow" rotation or the "Owning_Player_Character" rotation may be used. In this case The "Characer_Direction_Arrow" is used.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Get the location of the corresponding hand IK bone location. This location will be used to initiate the ray cast for the outer algorithm.
	FVector Select_Left_or_Right_IK_Hand{};

	if(bIs_Left_Hand == true)
	Select_Left_or_Right_IK_Hand = Mesh->GetSocketLocation(FName(TEXT("ik_hand_l")));

	else
	Select_Left_or_Right_IK_Hand = Mesh->GetSocketLocation(FName(TEXT("ik_hand_r")));

	//Set the offset value of the corresponding hand IK bone location. This value will be used to offset the ray cast for the outer algorithm which in result will set the location of the respective
	//hand to be closer to the center line (the Z axis) of the character. 
	int Value_To_Offset_Respective_Hand{};
	
	if(bIs_Left_Hand == true)
	Value_To_Offset_Respective_Hand = -15;

	else
	Value_To_Offset_Respective_Hand = 18;


	
	FVector Hand_Shimmy_Location{};

	FRotator Hand_Shimmy_Rotation{};


	int Index_1{};

	FHitResult Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result{};

	FRotator Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z{};

	/*Obtain the local FHitResult "Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result"*/
	for(Index_1; Index_1 <= 7; Index_1++)
	{
		const FVector Offset_Vector{Move_Vector_Left(Select_Left_or_Right_IK_Hand, Direction_Character_Is_Facing, Value_To_Offset_Respective_Hand)};
		const FVector Offset_Vector_1{Move_Vector_Down(Offset_Vector, Index_1 * 5.f)};
		const FVector Start_1{Move_Vector_Backward(Offset_Vector_1, Direction_Character_Is_Facing, 30.f)};
		const FVector End_1{Move_Vector_Forward(Start_1, Direction_Character_Is_Facing, 70.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start_1,
			End_1,
			10.f,
			Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result,
			false
		);

		//If there is a blocking hit reverse the normal of the impact point on the Z axis by 180 degrees using the helper funtion "Reverse_Wall_Normal_Rotation_Z". This reversed normal will be used within this function alone
		//to set the rotation of the respective hand as well as offset any vectors which need offseting.
		if(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.bBlockingHit && !Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.bStartPenetrating)
		{
			Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactNormal);
			
			//These are the best values to rotate the respecive hand which were realized via debugging.
			FRotator Hand_Shimmy_Rotation_Best_Rotation_Values{};
			
			if(bIs_Left_Hand)
			Hand_Shimmy_Rotation_Best_Rotation_Values = FRotator(50, 0, 270);
			
			else
			Hand_Shimmy_Rotation_Best_Rotation_Values = FRotator(230, 0, 270);

			//Use the reversed normal to set the rotation of the respective hand into the local FRotator variable "Hand_Shimmy_Rotation"
			Hand_Shimmy_Rotation = Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z + Hand_Shimmy_Rotation_Best_Rotation_Values;


			int Index_2{};

			FHitResult Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Hit_Result{};

			/*Obtain the "Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Hit_Result"*/
			for(Index_2; Index_2 <= 7; Index_2++)
			{
				const FVector Inner_Loop_Offset_Vector_1{Move_Vector_Forward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 2.f)};
				const FVector Start_2{Move_Vector_Up(Inner_Loop_Offset_Vector_1, Index_2 * 5.f)};
				const FVector End_2{Move_Vector_Down(Start_2, 70.f)};

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					this,
					Start_2,
					End_2,
					3.f,
					Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Hit_Result,
					false
				);

				//If there is a blocking hit store the location of the impact point within the local FVector "Hand_Shimmy_Location". If the location needs to be offset backwards, forwards etc.
				//use the helper function along with the global FRotator "Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z" (if its needed) then "break" out of the inner for loop.
				//The location for the hand needs to be set for each Climb Style as well as each Parkour Direction.
				if(Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Hit_Result.bBlockingHit && !Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Hit_Result.bStartPenetrating)
				{
					if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
					{
						const FVector Offset_Vector_Backwards{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																				   Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 3)};
						
						if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										   			       Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);
							break;
						}
						
						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										   			       Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);
							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										   				   Offset_Vector_Backwards.Y,
										   		           Offset_Vector_Backwards.Z + 5);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										                   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										                   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 5);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										                   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										                   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										   			       Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);
							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										   			       Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z + 3);
							break;
						}
					}

					else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
					{
						const FVector Offset_Vector_Backwards{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																				   Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 5)};
						
						if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
						{
							const FVector Offset_Vector_Backwards_2{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																						 Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 3)};
							
							Hand_Shimmy_Location = FVector( Offset_Vector_Backwards_2.X,
										   				    Offset_Vector_Backwards_2.Y,
										   				    Offset_Vector_Backwards_2.Z - 3);
							break;
						}
						
						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
						{
							const FVector Offset_Vector_Backwards_2{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																						 Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 3)};
							
							Hand_Shimmy_Location = FVector( Offset_Vector_Backwards_2.X,
										   				    Offset_Vector_Backwards_2.Y,
										   				    Offset_Vector_Backwards_2.Z - 3);
							break;
						}
						
						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										   				   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z - 7.f);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										  				   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z - 7.f);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										                   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z - 7);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
						{
							Hand_Shimmy_Location = FVector(Offset_Vector_Backwards.X,
										                   Offset_Vector_Backwards.Y,
										   				   Offset_Vector_Backwards.Z - 7);

							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
						{
							const FVector Offset_Vector_Backwards_2{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																						 Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 3)};
							
							Hand_Shimmy_Location = FVector( Offset_Vector_Backwards_2.X,
										   				    Offset_Vector_Backwards_2.Y,
										   				    Offset_Vector_Backwards_2.Z - 3);
							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
						{
							const FVector Offset_Vector_Backwards_2{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																						 Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 3)};
							
							Hand_Shimmy_Location = FVector( Offset_Vector_Backwards_2.X,
										   				    Offset_Vector_Backwards_2.Y,
										   				    Offset_Vector_Backwards_2.Z - 3);
							break;
						}

						else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
						{
							const FVector Offset_Vector_Backwards_2{Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, 
																						 Parkour_Shimmying_Dynamic_IK_Hands_Reversed_Front_Wall_Normal_Z, 3)};
							
							Hand_Shimmy_Location = FVector( Offset_Vector_Backwards_2.X,
										   				    Offset_Vector_Backwards_2.Y,
										   				    Offset_Vector_Backwards_2.Z - 3);
							break;
						}
					}
				}

				//If there is no blocking hit check to see if the inner algorithm has reached its maximum threshold. If it has call the function "&UCustom_Movement_Component::Reset_Parkour_IK_Hands" passing in "bIs_Left_Hand" as the input argument
				//then call return, otherwise continue the inner algorithm. "Reset_Parkour_IK_Hands" sets the location of the respective hand back to the location of the respective IK bone. This works well, however to keep the hands locked onto the surface
				//which is being shimmied when the edge of the surface has been reached and the character can't shimmy anymore, "Reset_Parkour_IK_Hands" will remain disabled until a better solution is realized.
				else
				{
					if(Index_2 == 7)
					{
						//When Performing a hop action if there is no valid hit for the local FHitResult "(Parkour_Shimmying_Dynamic_IK_Hands_Wall_Top_Hit_Result" when the character arrives at the hop destianation
						//call "&UCustom_Movement_Component::Reset_Parkour_IK_Hands" so that the respective hand is reset to it's original animation location.
						Reset_Parkour_IK_Hands(bIs_Left_Hand);
						return;
					}
			
					else
					{
						continue;
					}
				}
			}

			//The rotation and the location of the respective hand has been realized. Therefore break out of the outer for loop as well.
			break;
		}
		
		//If there is no blocking hit check to see if the outer algorithm has reached its maximum threshold. If it has call the function "&UCustom_Movement_Component::Reset_Parkour_IK_Hands" passing in "bIs_Left_Hand" as the input argument
		//then call return, otherwise continue the outer algorithm. "Reset_Parkour_IK_Hands" sets the location of the respective hand back to the location of the respective IK bone. This works well, however to keep the hands locked onto the surface
		//which is being shimmied when the edge of the surface has been reached and the character can't shimmy anymore, "Reset_Parkour_IK_Hands" will remain disabled until a better solution is realized.
		else
		{	
			if(Index_1 == 7)
			{
				/*Return out of this function early if there is no valid hit result for the local FHitResult "Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result". Returning from this function here will cause the 
				respective hand to stay in place at the last location where there was a valid hit result in the local FHitResult "Parkour_Shimmying_Dynamic_IK_Hands_Detect_Wall_Hit_Result". This in result will engance the
				Dynamic_IK_Hands by causing more realism to the character's shimmying.*/
				
				//Reset_Parkour_IK_Hands(bIs_Left_Hand);
				return;
			}
			
			else
			{
				continue;
			}
		}
	}

	//Pass in the realized location and rotation of the respecive hand into "UAnimInstance" via the respecive "IParkour_Locomotion_Interface" functions.
	if(bIs_Left_Hand == true)
	{
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Location(Anim_Instance, Hand_Shimmy_Location);
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Rotation(Anim_Instance, Hand_Shimmy_Rotation);
		return;
	}

	else
	{
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Location(Anim_Instance, Hand_Shimmy_Location);
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Rotation(Anim_Instance, Hand_Shimmy_Rotation);
		return;
	}

}

void UCustom_Movement_Component::Parkour_Climb_Initialize_IK_Feet(const bool& bIs_Left_Foot)
{
	//This function will be called only when a montage loaded into the function "&UCustom_Movement_Component::Play_Parkour_Montage"  has "Parkour.State.Ready.To.Climb" as the the "In_State" of its Data Asset object.
	//Within the montage an animation notify state is set which triggeres this function within the the class  "UInitiialize_IK_Libs". Also, the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" must have 
	//a blocking hit. This is because this FHitResult will be used to initialize the starting location of the ray casts to be performed within this funtion. Lastly, the global FGameplayTag "Parkour_Climb_Style" must have 
	//"Parkour.Climb.Style.Braced.Climb" set as its value. This is because this functions locks the feet of the character on to the surface of the wall which is being shimmied. Therefore if the value set within the 
	//global FGameplayTag "Parkour_Climb_Style" is "Parkour.Climb.Style.FreeHang" this function should return early.
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Ready.To.Climb"))) || 
	Parkour_Climb_Style != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))) || 
	!Initialize_Parkour_IK_Limbs_Hit_Result.bBlockingHit)
	return;

	//Offset the vector for the respective foot down from the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result". To keep the aesthetic natural use different values.
	FVector Offset_Vector_Down{};
	
	if(bIs_Left_Foot)
	Offset_Vector_Down = Move_Vector_Down(Initialize_Parkour_IK_Limbs_Hit_Result.ImpactPoint, 125.f);

	else
	Offset_Vector_Down = Move_Vector_Down(Initialize_Parkour_IK_Limbs_Hit_Result.ImpactPoint, 110.f);
	
	//Offset the vector to the right or left depending on the which foot is being modified.
	FVector Offset_Vector_To_Right_Or_Left{};

	if(bIs_Left_Foot == true)
	Offset_Vector_To_Right_Or_Left = Move_Vector_Left(Offset_Vector_Down, Reversed_Front_Wall_Normal_Z, 20.f);

	else
	Offset_Vector_To_Right_Or_Left = Move_Vector_Right(Offset_Vector_Down, Reversed_Front_Wall_Normal_Z, 20.f);

	int Index{};

	FHitResult Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall{};

	FRotator Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z{};

	FVector Foot_Shimmy_Location{};

	FRotator Foot_Shimmy_Rotation{};

	/*Obtain the local FHitResult "Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall"*/
	for(Index; Index <= 2; Index++)
	{
		const FVector Offset_Start{Move_Vector_Up(Offset_Vector_To_Right_Or_Left, Index * 5.f)};
		const FVector Start{Move_Vector_Backward(Offset_Start, Reversed_Front_Wall_Normal_Z, 30.f)};
		const FVector End{Move_Vector_Forward(Start, Reversed_Front_Wall_Normal_Z, 40.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			10.f,
			Parkour_Shimmying_Initialize_IK_Feet_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall,
			false
		);

		//If there is a blocking hit reverse the normal of the impact point on the Z axis by 180 degrees using the helper funtion "Reverse_Wall_Normal_Rotation_Z". This reversed normal will be used within this function alone
		//to set the rotation of the respective foot as well as offset any vectors which need offseting.
		if(Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall.bBlockingHit && !Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall.bStartPenetrating)
		{
			Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z = Reverse_Wall_Normal_Rotation_Z(Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall.ImpactNormal);
			
			//Move the vector backwords (from the hit result of the local FHitResult "Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall") so that the feet are not clipping throught the surface of the wall which is being shimmied.
			Foot_Shimmy_Location = Move_Vector_Backward(Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall.ImpactPoint, Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z, 17.f);
			

			//These are the best values to rotate the respecive foot which were realized via debugging.
			const FRotator Left_Foot_Shimmy_Rotation{FRotator(110, -10, 100)};
			const FRotator Right_Foot_Shimmy_Rotation{FRotator(-55, 0, 95)};

			//Use the reversed normal to set the rotation of the respective foot into the local FRotator variable "Foot_Shimmy_Rotation"
			if(bIs_Left_Foot == true)
			Foot_Shimmy_Rotation = Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z + Left_Foot_Shimmy_Rotation;

			else
			Foot_Shimmy_Rotation = Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z + Right_Foot_Shimmy_Rotation;

			//Break out of the for loop because the location and rotation of the respective foot has been realized.
			break;
		}

		//If there is no blocking hit check to see if the algorithm has reached its maximum threshold. If it has return, otherwise continue the algorithm.
		else
		{
			if(Index == 2)
			return;

			else
			continue;
		}

	}

	//Pass in the realized location and rotation of the respecive foot into "UAnimInstance" via the respecive "IParkour_Locomotion_Interface" functions.
	if(bIs_Left_Foot == true)
	{
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Location(Anim_Instance, Foot_Shimmy_Location);
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Rotation(Anim_Instance, Foot_Shimmy_Rotation);
	}
	
	else
	{
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Location(Anim_Instance, Foot_Shimmy_Location);
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Rotation(Anim_Instance, Foot_Shimmy_Rotation);
	}
	
}

void UCustom_Movement_Component::Parkour_Climb_Dynamic_IK_Feet(const bool& bIs_Left_Foot)
{
	if(Parkour_Climb_Style != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
	return;

	//Get the socket location for the left or right IK foot bone.
	FVector Left_Or_Right_IK_Foot_Socket_Location{};
	if(Mesh)
	{
		if(bIs_Left_Foot)
		Left_Or_Right_IK_Foot_Socket_Location = Mesh->GetSocketLocation(FName(TEXT("ik_foot_l")));

		else
		Left_Or_Right_IK_Foot_Socket_Location = Mesh->GetSocketLocation(FName(TEXT("ik_foot_r")));
	}
	
	//Get the socket location for the left or right ik hand bone.
	FVector Left_Or_Right_Hand_Socket_Location{};
	if(Mesh)
	{
		if(bIs_Left_Foot)
		Left_Or_Right_Hand_Socket_Location = Mesh->GetSocketLocation(FName(TEXT("ik_hand_l")));

		else
		Left_Or_Right_Hand_Socket_Location = Mesh->GetSocketLocation(FName(TEXT("ik_hand_r")));
	}

	//Offset the vector from the corresponding hand down to the level of where the feet should be.
	FVector Offset_Vector_Down{};

	if(bIs_Left_Foot)
	Offset_Vector_Down = FVector(Left_Or_Right_IK_Foot_Socket_Location.X, Left_Or_Right_IK_Foot_Socket_Location.Y, Left_Or_Right_Hand_Socket_Location.Z - 125.f);

	else
	Offset_Vector_Down = FVector(Left_Or_Right_IK_Foot_Socket_Location.X, Left_Or_Right_IK_Foot_Socket_Location.Y, Left_Or_Right_Hand_Socket_Location.Z - 110.f);;
	
	int Index{};
	FHitResult Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Hit_Result{};
	
	FVector Feet_Shimmy_Location{};
	FRotator Feet_Shimmy_Rotation{};

	for(Index; Index <= 2; Index++)
	{
		//Direction the character is facing
		const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

		//With each iteration of the for loop move the vector up (as long as there is no "bBlockingHit").
		const FVector Offset_Vector_Up_With_Each_Iteration_Of_For_Loop{Move_Vector_Up(Offset_Vector_Down, Index * 5.f)};
		//Start location of the ray cast
		const FVector Start{Move_Vector_Backward(Offset_Vector_Up_With_Each_Iteration_Of_For_Loop, Direction_Character_Is_Facing, 30.f)};
		//End location of the ray cast.
		const FVector End{Move_Vector_Forward(Start, Direction_Character_Is_Facing, 70.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			13.f,
			Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Hit_Result,
			false
		);

		//These are the best values to seet the corresponding foot's rotation to. Said values were discovered via debugging.
		const FRotator Left_Foot_Shimmy_Rotation{FRotator(110, -10, 100)};
		const FRotator Right_Foot_Shimmy_Rotation{FRotator(-55, 0, 95)};
		
		//Check to see if "bStartPenetrating" is false and there is a blocking hit. If this check passes assign the location of the impact point to the local FVector variable "Foot_Shimmy_Location" then
		//break out of the for loop. If this check fails check to see if the for loop has reached the maxiumim Index value. If so, Index return out of the function otherwise "continue".
		if(!Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Hit_Result.bStartPenetrating && Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Hit_Result.bBlockingHit)
		{
			const FRotator Parkour_Shimmying_IK_Feet_Reversed_Wall_Normal_On_Z{Reverse_Wall_Normal_Rotation_Z(Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Hit_Result.ImpactNormal)};

			Feet_Shimmy_Location = Move_Vector_Backward(Parkour_Shimmying_Dynamic_IK_Feet_Detect_Wall_Hit_Result.ImpactPoint, Parkour_Shimmying_IK_Feet_Reversed_Wall_Normal_On_Z, 17.f);


			if(bIs_Left_Foot)
			Feet_Shimmy_Rotation = Parkour_Shimmying_IK_Feet_Reversed_Wall_Normal_On_Z + Left_Foot_Shimmy_Rotation;
			
			else
			Feet_Shimmy_Rotation = Parkour_Shimmying_IK_Feet_Reversed_Wall_Normal_On_Z + Right_Foot_Shimmy_Rotation;

			break;
		}

		else
		{
			if(Index == 2)
			{	
				Reset_Parkour_IK_Feet(bIs_Left_Foot);
				return;
			}
			

			else
			{
				continue;
			}
		}
	}
	
	//Call the coresponding interface function for the appropriate limb.
	if(bIs_Left_Foot)
	{
		if(Parkour_Interface && Anim_Instance)
		{
			Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Location(Anim_Instance, Feet_Shimmy_Location);
			Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Rotation(Anim_Instance, Feet_Shimmy_Rotation);
		}
	}

	else
	{
		if(Parkour_Interface && Anim_Instance)
		{
			Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Location(Anim_Instance, Feet_Shimmy_Location);
			Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Rotation(Anim_Instance, Feet_Shimmy_Rotation);
		}
	}
}

bool UCustom_Movement_Component::Validate_Out_Corner_Shimmying()
{
	//To deter this function from executing multiple ray casts check to see if the current FGameplayTag stored within the global FGameplayTag "Parkour_Action" is set to "Parkour.Action.Corner.Move"
	//and if the global bool "bOut_Corner_Movement" is set to true (it is set to true if this function confirms it is safe to perform Out_Corner_Shimmying and is set to false when the timer 
	//"UCustom_Movement_Component::Set_bOut_Corner_Movement_To_False" which is called within &UCustom_Movement_Component::Parkour_Shimmy_Corner_Movement is complete).
	if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Corner.Move"))) || bOut_Corner_Movement)
	return false;
	
	//Get the location of the arrow actor.
	const FVector Location_To_Begin_Ray_Casts{Character_Direction_Arrow->GetActorLocation()};

	//Get the direction which the character is facing.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Offset the start location of the sphere trace according to the current FGameplayTag set on the global FGameplayTag variable "Parkour_Climb_Style".
	//This needs to happen because during shimmying across some surfaces the character moves to close to the surface of the wall. If this happens then the sphere trace
	//will not get a blocking hit abnd in result no wall will be detected.
	const float Value_To_Offset_Sphere_Trace_Backwards{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 10.f, 30.f)};

	//Since the value stored in the global double variable "Right_Left_Movement_Value" has a maximum of 1 (value represents the input from the controller for whether the character should move to the 
	//left or right with the value 1 being full input in the respective direction and 0 being no input for the character to move), whatever value is stored in said global variable will be multiplied
	//by 10 within the local double variable "Horizontal_Move_Direction_Update".
	const double Right_Left_Movement_Value_Multiplier{Right_Left_Movement_Value * 70.f};

	int Index_1{};

	FHitResult Parkour_Shimmying_Validate_Out_Corner_Wall_Hit_Result{};

	for(Index_1; Index_1 <= 7; Index_1++)
	{
		const FVector Nested_Outer_Loop_Offset_Vector_1{Move_Vector_Right(Location_To_Begin_Ray_Casts, Direction_Character_Is_Facing, Right_Left_Movement_Value_Multiplier)};
		const FVector Nested_Outer_Loop_Offset_Vector_2{Move_Vector_Down(Nested_Outer_Loop_Offset_Vector_1, Index_1 * 10.f)};
		
		const FVector Nested_Outer_Loop_Start{Move_Vector_Backward(Nested_Outer_Loop_Offset_Vector_2, Direction_Character_Is_Facing, Value_To_Offset_Sphere_Trace_Backwards)};
		const FVector Nested_Outer_Loop_End{Move_Vector_Forward(Nested_Outer_Loop_Start, Direction_Character_Is_Facing, 70.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Nested_Outer_Loop_Start,
			Nested_Outer_Loop_End,
			5.f,
			Parkour_Shimmying_Validate_Out_Corner_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Shimmying_Validate_Out_Corner_Wall_Hit_Result,
			false
		);

		//If "Parkour_Shimmying_Validate_Out_Corner_Wall_Hit_Result.bStartPenetrating" is true this means there is a wall on the respective side of the character which the character can rotate and shimmy onto. Therefore,
		//analyze the wall further. If this checks turns out to be false, check to see if the thresholf of the Index has been met, if it has then there's no surface on the side of the character which shimmying can be executed on. 
		//If the threshold hasn't been met then continue because the wall hasn't been fully analyzed yet.
		if(Parkour_Shimmying_Validate_Out_Corner_Wall_Hit_Result.bStartPenetrating)
		{	
			/*Obtain the Out Corner Shimmy Reversed Wall Normal On Z and store it in the global FRotator "Reversed_Wall_Normal_On_Z". Also store the HitResult within the global FHitResult "Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result"*/
			
			Debug::Print("Out_Corner_Shimmy_Found", FColor::MakeRandomColor(), 12);
			
			int Index_2{};

			for(Index_2; Index_2 <= 7; Index_2++)
			{
				const FVector Nested_Inner_Loop_Offset_Vector_1{Move_Vector_Down(Location_To_Begin_Ray_Casts, Index_2 * 10.f)};
				
				const FVector Nested_Inner_Loop_Start{Move_Vector_Backward(Nested_Inner_Loop_Offset_Vector_1, Direction_Character_Is_Facing, Value_To_Offset_Sphere_Trace_Backwards)};
				const FVector Nested_Inner_Loop_End{Move_Vector_Right(Nested_Inner_Loop_Start, Direction_Character_Is_Facing, Right_Left_Movement_Value_Multiplier)};

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					this,
					Nested_Inner_Loop_Start,
					Nested_Inner_Loop_End,
					5,
					Parkour_Shimmying_Detect_Out_Corner_Wall_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result,
					false
				);

				//Check to see if there is a blocking hit stored in the local FHitResult "Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result" and "bStartPenetrating" is false. If there is a blocking hit and "bStartPenetrating" is false 
				//then store the reverse the wall normal on X using the helper function"&UCustom_Movement_Component::Reverse_Wall_Normal_Rotation_Z" and store said wall normal in the global variable "Reversed_Front_Wall_Normal_Z". This 
				//will be the new rotation which the character will face when the corner movement is complete. If there is no blocking hit stored in the local FHitResult "Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result" check to see 
				//if the threshold for this inner for loop met. If the threshold is reached then the wall has been analyzed and it is not suitable for shimmying so return false. If the threshold is not met "continue" because the wall needs 
				//to be fully analyzed.
				if(Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result.bBlockingHit && !Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result.bStartPenetrating)
				{
					Debug::Print("Out_Corner_Shimmy_Wall_Surface_Obtained", FColor::MakeRandomColor(), 13);
					
					Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result.ImpactNormal);

					/*Obtain the Out Corner Shimmy Top Wall Result and store it in the global FHitResult "Wall_Top_Result"*/
					int Index_3{};

					for(Index_3; Index_3 <= 7; Index_3++)
					{
						const FVector Nested_Innermost_Loop_Offset_Vector_1{Move_Vector_Forward(Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 2.f)};
						const FVector Nested_Innermost_Loop_Offset_Vector_2{Move_Vector_Down(Nested_Innermost_Loop_Offset_Vector_1, 2.f)};

						const FVector Nested_Innermost_Loop_Start{Move_Vector_Up(Nested_Innermost_Loop_Offset_Vector_2, Index_3 * 5.f)};
						const FVector Nested_Innermost_Loop_End{Move_Vector_Down(Nested_Innermost_Loop_Start, 30.f)};

						UKismetSystemLibrary::SphereTraceSingleForObjects(
							this,
							Nested_Innermost_Loop_Start,
							Nested_Innermost_Loop_End,
							5.f,
							Parkour_Shimmying_Out_Corner_Wall_Top_Result_Trace_Types,
							false,
							TArray<AActor*>(),
							Debug_Action,
							Parkour_Shimmying_Out_Corner_Wall_Top_Result,
							false
						);

						//Check to see if "BStartPenetrating" is true for the local FHitResult "Parkour_Shimmying_Out_Corner_Wall_Top_Result". During the first loop of this innermost for loop this should be true because the ray trace begins within
						//the wall. If it is true then another check to see if this inner most for loop has reached the threshold set (7). If the threshold has been met then a Wall Top Result can't be obtained and the wall which is being analyzed can
						//not be shimmied onto. In result false should be returned. Owherwise "continue" should be called because the wall needs to be fully analyzed. When "BStartPenetrating" is false, chceck to see if there is a blocking hit. If there
						//a blocking hit store the hit result in the global FHitResult "Wall_Top_Result".
						if(Parkour_Shimmying_Out_Corner_Wall_Top_Result.bStartPenetrating)
						{
							if(Index_3 == 7)
							{
								Debug::Print("Out_Corner_Shimmy_Wall_Top_Not_Obtained", FColor::MakeRandomColor(), 14);
								Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
								return false;
							}
							
							else
							{
								continue;
							}
						}

						else
						{
							if(Parkour_Shimmying_Out_Corner_Wall_Top_Result.bBlockingHit)
							{
								Debug::Print("Out_Corner_Shimmy_Wall_Top_Obtained", FColor::MakeRandomColor(), 14);
								Decide_Climb_Style(Parkour_Shimmying_Out_Corner_Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
								bOut_Corner_Movement = true;
								return true;
							}

							else
							{
								Debug::Print("Out_Corner_Shimmy_Wall_Top_Not_Obtained", FColor::MakeRandomColor(), 14);
								Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
								return false;
							}
						}
					}
				}

				else
				{
					if(Index_2 == 7)
					{
						Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
						Debug::Print("Out_Corner_Shimmy_Wall_Surface_Obtained_Not_Obtained", FColor::MakeRandomColor(), 13);
						return false;
					}

					else
					{
						continue;
					}
				}
			}
		}

		else
		{	
			if(Index_1 == 7)
			{
				Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
				Debug::Print("Out_Corner_Shimmy_Not_Possible", FColor::MakeRandomColor(), 12);
				return false;
			}

			else
			{
				continue;
			}
		}
	}

	//The following line is to meet the requirement of the return type of this function.
	return false;
}

bool UCustom_Movement_Component::Validate_In_Corner_Shimmying()
{
	//To deter this function from executing multiple ray casts check to see if the current FGameplayTag stored within the global FGameplayTag "Parkour_Action" is set to "Parkour.Action.Corner.Move"
	//and if the global bool "bIn_Corner_Movement" is set to true (it is set to true if this function confirms it is safe to perform In_Corner_Shimmying and is set to false when the timer 
	//"UCustom_Movement_Component::Set_bIn_Corner_Movement_To_False" which is called within &UCustom_Movement_Component::Parkour_Shimmy_Corner_Movement is complete).
	if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Corner.Move"))) || bIn_Corner_Movement)
	return false;

	//Get the location and rotation of the "Character_Direction_Arrow". These variables will be used to obtain the initial location and rotation to use for the ray casts
	//until the global FHitResult "Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result" is filled with a valid hit result.
	const FVector Location_Of_Character_Direction_Arrow{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	/*Obtain the Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result*/

	int Index_1{};
	for(Index_1; Index_1 <= 7; Index_1++)
	{
		const FVector Offset_Vector_1{Move_Vector_Forward(Location_Of_Character_Direction_Arrow, Direction_Character_Is_Facing, 70.f)};
		const FVector Offset_Vector_2{Move_Vector_Right(Offset_Vector_1, Direction_Character_Is_Facing, Right_Left_Movement_Value * 50.f)};
		const FVector Start{Move_Vector_Down(Offset_Vector_2, Index_1 * 10)};
		FVector End{};
		
		if(Right_Left_Movement_Value == 1.f)
		{
			End = Move_Vector_Left(Start, Direction_Character_Is_Facing, 80.f);
		}
		
		else if(Right_Left_Movement_Value == -1.f)
		{
			End = Move_Vector_Right(Start, Direction_Character_Is_Facing, 80.f);
		}

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Shimmying_Detect_In_Corner_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result,
			false
		);

		if(Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result.bStartPenetrating)
		{
			Debug::Print("Not_Enough_Room_To_Perform_In_Corner_Movement", FColor::MakeRandomColor(), 12);
			return false;
		}

		else
		{
			if(Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result.bBlockingHit)
			{
				Debug::Print("In_Corner_Wall_Found", FColor::MakeRandomColor(), 13);
				//Set the value in a local FRotator "In_Corner_Shimmying_Reversed_Front_Wall_Normal_Z" to equal the reversed normal on Z for the local FHitResult 
				//"Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result". This will be the new direction the character is facing when the in corner movement
				//is complete.
				const FRotator In_Corner_Shimmying_Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result.ImpactNormal);

				/*Obtain the Parkour_Shimmying_In_Corner_Wall_Top_Result*/
				int Index_2{};

				for(Index_2; Index_2 <= 7; Index_2++)
				{
					const FVector Inner_Nested_Loop_Offset_Vector_1{Move_Vector_Forward(Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result.ImpactPoint, 
																						In_Corner_Shimmying_Reversed_Front_Wall_Normal_Z, 
																						4.f)};
					const FVector Inner_Nested_Loop_Start{Move_Vector_Up(Inner_Nested_Loop_Offset_Vector_1, Index_2 * 5.f)};
					const FVector Inner_Nested_Loop_End{Move_Vector_Down(Inner_Nested_Loop_Start, 30.f)};

					UKismetSystemLibrary::SphereTraceSingleForObjects(
						this,
						Inner_Nested_Loop_Start,
						Inner_Nested_Loop_End,
						5.f,
						Parkour_Shimmying_In_Corner_Wall_Top_Result_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Parkour_Shimmying_In_Corner_Wall_Top_Result,
						false
					);

					if(Parkour_Shimmying_In_Corner_Wall_Top_Result.bStartPenetrating)
					{
						if(Index_2 == 7)
						{
							Debug::Print("In_Corner_Wall_Top_Not_Found", FColor::MakeRandomColor(), 13);
							return false;
						}

						else
						{
							continue;
						}
					}

					else
					{
						if(Parkour_Shimmying_In_Corner_Wall_Top_Result.bBlockingHit)
						{
							Debug::Print("In_Corner_Wall_Top_Found", FColor::MakeRandomColor(), 13);
							
							/*Check to see if there is enough space to fit the character in the location stored within the global FHitResults "Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result"
							and "Parkour_Shimmying_In_Corner_Wall_Top_Result"*/

							//Store the values which will be used for the ray casts capsule half height.
							const float Dynamic_Capsule_Half_Height{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 48.f, 98.f)};
							//Based on the 
							const float Dynamic_Value_To_Offset_Vector_Down{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 100.f, 200.f)};

							const FVector Capsule_Trace_Space_Check_Offset_Vector_1{Move_Vector_Right(Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result.ImpactPoint, Direction_Character_Is_Facing, Right_Left_Movement_Value * 40.f)};
							const FVector Capsule_Trace_Space_Check_Offset_Vector_2{Move_Vector_Down(Capsule_Trace_Space_Check_Offset_Vector_1, Dynamic_Value_To_Offset_Vector_Down)};

							const FVector Capsule_Trace_Space_Check_Start{Move_Vector_Up(Capsule_Trace_Space_Check_Offset_Vector_2, Dynamic_Capsule_Half_Height)};
							const FVector Capsule_Trace_Space_Check_End{Capsule_Trace_Space_Check_Start};

							FHitResult Parkour_Shimmying_Validate_In_Corner_Wall_Space{};

							UKismetSystemLibrary::CapsuleTraceSingleForObjects(
								this,
								Capsule_Trace_Space_Check_Start,
								Capsule_Trace_Space_Check_End,
								30,
								Dynamic_Capsule_Half_Height,
								Parkour_Shimmying_Validate_In_Corner_Wall_Space_Trace_Types,
								false,
								TArray<AActor*>(),
								Debug_Action,
								Parkour_Shimmying_Validate_In_Corner_Wall_Space,
								false
							);

							if(Parkour_Shimmying_Validate_In_Corner_Wall_Space.bStartPenetrating || Parkour_Shimmying_Validate_In_Corner_Wall_Space.bBlockingHit)
							{
								Debug::Print("There_Is_Not_Enough_Space_To_Perform_In_Corner_Movement", FColor::MakeRandomColor(), 13);
								return false;
							}

							else
							{
								Debug::Print("There_Is_Enough_Space_To_Perform_In_Corner_Movement", FColor::MakeRandomColor(), 13);
								Reversed_Front_Wall_Normal_Z = In_Corner_Shimmying_Reversed_Front_Wall_Normal_Z;
								Decide_Climb_Style(Parkour_Shimmying_In_Corner_Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
								bIn_Corner_Movement = true;
								return true;
							}
						}

						else
						{
							if(Index_2 == 7)
							{
								Debug::Print("In_Corner_Wall_Top_Not_Found", FColor::MakeRandomColor(), 13);
								return false;
							}

							else
							{
								continue;
							}
						}
					}
				}
			}

			else
			{
				if(Index_1 == 7)
				{
					Debug::Print("In_Corner_Wall_Not_Found", FColor::MakeRandomColor(), 13);
					return false;
				}

				else
				{
					continue;
				}
			}
			
		}
	}

	//The following line is to meet the requirement of the return type of this function.
	return false;
}

bool UCustom_Movement_Component::Validate_Shimmy_180_Shimmy()
{
	//Get the location of the character direction arrow.
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	//Get the direction the character is facing.
	const FRotator Direction_Of_Character_Direction_Arrow{Character_Direction_Arrow->GetActorRotation()};
	
	//Offset the vector down.
	const float Offset_Vector_1_Down_Value{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 30.f, -15.f)};
	const FVector Offset_Vector_1{Move_Vector_Down(Character_Direction_Arrow_Location, Offset_Vector_1_Down_Value)};
	
	//Offset the vector forward.
	const float Offset_Vector_2_Forward_Value{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 100.f, 70.f)};
	const FVector Offset_Vector_2{Move_Vector_Forward(Offset_Vector_1, Direction_Of_Character_Direction_Arrow, Offset_Vector_2_Forward_Value)};

	int Index_1{};
	
	for(Index_1; Index_1 <= 7; Index_1++)
	{
		//With each iteration of the loop move the vector down
		const FVector Start{Move_Vector_Down(Offset_Vector_2, Index_1 * 5.f)};
		const FVector End{Move_Vector_Backward(Start, Direction_Of_Character_Direction_Arrow, 70.f)};

		FHitResult Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result{};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Validate_Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result,
			false
		);

		if(Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result.bBlockingHit && !Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result.bStartPenetrating)
		{
			const FRotator Shimmy_180_Rotation_To_Shimmy_Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result.ImpactNormal);
		
			/*Get Wall_Top_Result*/

			int Index_2{};

			FHitResult Shimmy_180_Rotation_To_Shimmy_Wall_Top_Result{};

			for(Index_2; Index_2 <= 7; Index_2++)
			{
				//Offset vector forward so the ray cast begins inside of the wall which is being examined.
				const FVector Offset_Vector_Loop_1{Move_Vector_Forward(Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result.ImpactPoint, Shimmy_180_Rotation_To_Shimmy_Reversed_Front_Wall_Normal_Z, 2.f)};

				//Offset vector up with each iteration of the loop.
				const FVector Inner_Loop_Start{Move_Vector_Up(Offset_Vector_Loop_1, Index_2 * 5.f)};
				const FVector Inner_Loop_End{Move_Vector_Down(Inner_Loop_Start, 10.f)};

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					this,
					Inner_Loop_Start,
					Inner_Loop_End,
					5.f,
					Validate_Shimmy_180_Rotation_To_Shimmy_Wall_Top_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Shimmy_180_Rotation_To_Shimmy_Wall_Top_Result,
					false
				);

				if(Shimmy_180_Rotation_To_Shimmy_Wall_Top_Result.bBlockingHit && !Shimmy_180_Rotation_To_Shimmy_Wall_Top_Result.bStartPenetrating)
				{
					FHitResult Shimmy_180_Rotation_To_Shimmy_Space_Check{};

					const FVector Space_Check_Offset_1{Move_Vector_Backward(Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result.ImpactPoint, Shimmy_180_Rotation_To_Shimmy_Reversed_Front_Wall_Normal_Z, 50.f)};
					const FVector Space_Check_Offset_2{Move_Vector_Down(Space_Check_Offset_1, 55.f)};
					const FVector Space_Check_Start{Space_Check_Offset_2};
					const FVector Space_Check_End{Space_Check_Start};

					UKismetSystemLibrary::CapsuleTraceSingleForObjects(
						this,
						Space_Check_Start,
						Space_Check_End,
						45.f,
						45.f,
						Validate_Shimmy_180_Rotation_To_Shimmy_Space_Check_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Shimmy_180_Rotation_To_Shimmy_Space_Check,
						false
					);

					if(Shimmy_180_Rotation_To_Shimmy_Space_Check.bBlockingHit || Shimmy_180_Rotation_To_Shimmy_Space_Check.bStartPenetrating)
					{
						Debug::Print("Shimmy_180_Shimmy_Not_Possible", FColor::Red, 17);
						return false;
					}

					else if(!Shimmy_180_Rotation_To_Shimmy_Space_Check.bBlockingHit && !Shimmy_180_Rotation_To_Shimmy_Space_Check.bStartPenetrating)
					{
						Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Shimmy_180_Rotation_To_Shimmy_Detect_Wall_Result.ImpactNormal);
						Wall_Top_Result = Shimmy_180_Rotation_To_Shimmy_Wall_Top_Result;
						Debug::Print("Shimmy_180_Shimmy_Possible", FColor::Green, 17);
						return true;
					}
				}

				else
				{
					if(Index_2 == 7)
					{
						Debug::Print("Shimmy_180_Shimmy_Not_Possible", FColor::MakeRandomColor(), 17);
						return false;
					}

					else
					{
						continue;
					}
				}
			}
		}

		else
		{
			if(Index_1 == 7)
			{
				Debug::Print("Shimmy_180_Shimmy_Not_Possible", FColor::MakeRandomColor(), 17);
				return false;
			}

			else
			{
				continue;
			}
		}
	}
	
	//This line is to meet the requirement of this function's return type
	return false;
}

bool UCustom_Movement_Component::Validate_Can_Mantle() const
{
	//This funtion should only be perfomred if the player is "Climbing".
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		Debug::Print("Can't_Mantle_Up_On_Surface", FColor::Black, 14);
		return false;
	}

	else
	{
		const float Capsule_Component_Half_Height{98.f};

		FHitResult Validate_Mantle_Hit_Result{};

		const FVector Offset_Vector_1{Move_Vector_Forward(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 40.f)};
		const FVector Offset_Vector_2{Move_Vector_Up(Offset_Vector_1, 5.f)};
		const FVector Start{Move_Vector_Up(Offset_Vector_2, Capsule_Component_Half_Height)};
		const FVector End{Start};

		/*Perform a Capsule Trace To determine if there is enough room to perform a mantle onto the surface of the wall which is being shimmied*/
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this,
			Start,
			End,
			42.f,
			Capsule_Component_Half_Height,
			Validate_Climb_Or_Hop_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Validate_Mantle_Hit_Result,
			false
		);

		//If there is a blocking hit or bStartPenetrating is true this means there is not enough room to perform a mantle onto the surface which is being shimmied.
		//Therefore return false. Otherwise return true.
		if(Validate_Mantle_Hit_Result.bBlockingHit || Validate_Mantle_Hit_Result.bStartPenetrating)
		{
			Debug::Print("Can't_Mantle_Up_On_Surface", FColor::Black, 14);
			return false;
		}

		else
		{
			Debug::Print("Can_Mantle_Up_On_Surface", FColor::Green, 14);
			return true;
		}
	}
}

void UCustom_Movement_Component::Hop_Grid_Scan_For_Hop_Hit_Result(const FVector& Previous_Trace_Location, const FRotator& Previous_Trace_Rotation, const int& Scan_Width_Value, const int& Scan_Height_Value)
{
	/*The input parameter "Previous_Trace_Rotation" is the Impact Normal for the "SphereTraceSingleForObjects()" found in "&UCustom_Movement_Component::Parkour_Detect_Wall()". 
	This normal will be reversed when this function is called so that the ray cast performed in this function will face towards the wall of the impact point passed into "Previous_Trace_Location".
	This impact point is also foundin "&UCustom_Movement_Component::Parkour_Detect_Wall() "Scan_Width_Value" and "Scan_Height_Value" will are variables which may be set in C++ or in the character Blueprint*/
	
	
	//The "LineTraceSingleForObjects()" performed in the for loop will fill this FHitResult with data.
	FHitResult Out_Hit{};

	//In this case it's good practice to empty the arrays at the beginnning of the function so that garagbage data from the previous function call can be wiped out.
	Grid_Scan_Hop_Hit_Traces.Empty();
	
	//Develop a for loop which will set the "Width" of the line traces performed. The goal is to have the line traces casted form a grid which have has a width of 5 line traces.
	//"Scan_Width_Value" will be filled with the the value 5.
	for(int Index_1{}; Index_1 <= Scan_Width_Value; Index_1++)
	{	
		//Multiply "Index_1" by 20 on each iteration of the for loop. This value will be subtracted from to generate the vector location of each of the line traces which make up the width of "grid"
		const int Index_1_Multiplier{Index_1 * 20};
		
		//Multiply the "Scan_Width_Value" by 10 during each iteration of the for loop. This value will be subtracted from "Index_1_Multiplier" during each iteration of the loop to position the vector 
		//which will serve as the location for respective the ray cast. 
		const int Scan_Width_Value_Multiplier{Scan_Width_Value * 10};
		
		//Subtract the value in "Scan_Width_Value_Multiplier" from the value in "Index_1_Multiplier" to position the vector accordingly.
		const int Set_Scan_Width_Trace_Location_Value{Index_1_Multiplier - Scan_Width_Value_Multiplier};
		
		//With the calculation (subtraction) above, on the first iteration of this for loop, the vector will start off at the left of the input parameter "Previous_Trace_Location" which is 
		//the impact point of the "SphereTraceSingleForObjects()" found in "&UCustom_Movement_Component::Parkour_Detect_Wall()". This is because the value will start of as a negative number 
		//on loop 0 and in result instead of "Move_Vector_Right" the vector will move to the left (because of the negative value). WIth each loop iteration, the vector will move to the right 
		//as the value increases. When "Index_1" is 2 the value of "Set_Scan_Width_Trace_Location_Value" will be 0 and in result the vector will be at the same location as the input parameter 
		//"Previous_Trace_Location". This will be followed by two more loops which will move the vector to the right of the "SphereTraceSingleForObjects()" found in "&UCustom_Movement_Component::Parkour_Detect_Wall()"  
		//(input parameter "Previous_Trace_Location"). In result there will be five raycast formed, hence forming the "width" of the grid scan.
		const FVector Set_Scan_Width_Trace_Location{Move_Vector_Right(Previous_Trace_Location, Previous_Trace_Rotation, Set_Scan_Width_Trace_Location_Value)};

		//Develop a nested for loop to handle the height of the grid scan.
		//"Scan_Height_Value" will be filled with the the value 30.
		for(int Index_2{}; Index_2 <= Scan_Height_Value; Index_2++)
		{
			//Multiply "Index_2" by 8 on each iteration of the for loop. This value will be used to generate the vector location of each of the line traces which make up the height of "grid" 
			const int Index_2_Multiplier{Index_2 * 8};
			
			//With each iteration of this nested loop the vector will have a starting point of the "Set_Scan_Width_Trace_Location" which happens to be at ground level. Therefore, to increase the height of the vector
			//consistently, making sure the position of the vector is above the position of the previous vector a multiplication of (Index_2 * 8) found in "Index_2_Multiplier" has to be done during each loop. 
			const FVector Set_Scan_Height_Trace_Location{Move_Vector_Up(Set_Scan_Width_Trace_Location, Index_2_Multiplier)};
			
			//Move the vector backwards so that it starts right in front of the character.
			const FVector Start{Move_Vector_Backward(Set_Scan_Height_Trace_Location, Previous_Trace_Rotation, 50.f)};
			
			//Move the vector forwards so that the line traces end a good distance away from the character
			const FVector End{Move_Vector_Forward(Start, Previous_Trace_Rotation, 80.f)};

			//Develop the line traces
			UKismetSystemLibrary::LineTraceSingleForObjects(
				this,
				Start,
				End,
				Parkour_Grid_Scan_For_Hit_Results_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Out_Hit,
				true
			);

			//Add the "Out_Hit" generated by each for loop iteration to the array "Wall_Hit_Traces_Height".
			Grid_Scan_Hop_Hit_Traces.Add(Out_Hit);
		}
	}

}

void UCustom_Movement_Component::Analyze_Hop_Grid_Scan_For_Front_Wall_Top_Edge_Hits()
{
	//In this case it's good practice to empty the arrays at the beginnning of the function so that garagbage data from the previous function call can be wiped out.
	Front_Wall_Top_Edge_Hop_Traces.Empty();

	//Before moving on to perform the following for loop the loop index has to be greater than 0 before anything can be done. This is because there is a need for at least two indexes (current and previous)
	//to perform the calculations needed. The goal of the following for loop is to get the distances (from Trace Start to Trace End if there is no impact point and from Trace Start to Impact Point if there is a blocking hit) 
	//of the line traces which were generated in the previous for loop. Once the distances of line traces are calculated for both the current (Index_3) and previous (Index_3 - 1) loop iteration, there is a check to see wheahter there is a blocking hit or not. 
	//Depending on this answer the corresponding value will be assighned to the global variables "Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration" and "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
	//After this there is another check to see if the difference between the two is greater than 7 units. If this is true this means the previous line trace is the ray cast that is is right under the top edge of the wall.
	//In result, said line trace from the previous loop iteration is added to the TArray "Front_Wall_Top_Edge_Traces" and the for loop "continues".
		
	for(int Index{}; Index != Grid_Scan_Hop_Hit_Traces.Num(); Index++)
	{
		/*The reason why this "continue" happens at the beginning of this array index based for loop (which handles the calculations for the current and previous index's line trace) is because there needs to be at least two array elements (Index = 1)
		loaded into this for loop for it to work correctly.*/
		if(Index == 0) continue;

		/*Getting the trace distance from the current loop iteration*/
		//Checking to see if the current line trace has a blocking hit or not. Depending on this answer the appropriate 
		//distance value will be assigned to the global variable "Distance_In_Grid_Scan_For_Hit_Results_Current_Iteration".
			
		//If there is a blocking hit this, is the distance from the line trace start to the impact point.
		const float Current_Iteration_Distance_If_Blocking_Hit{Grid_Scan_Hop_Hit_Traces[Index].Distance};
				
		//If there is no impact point get the distance between the line trace start and its end.
		const FVector_NetQuantize Current_Iteration_Line_Trace_Start{Grid_Scan_Hop_Hit_Traces[Index].TraceStart};
		const FVector_NetQuantize Current_Iteration_Line_Trace_End{Grid_Scan_Hop_Hit_Traces[Index].TraceEnd};
		const double Current_Iteration_Distance_If_No_Blocking_Hit{UKismetMathLibrary::Vector_Distance(Current_Iteration_Line_Trace_Start, Current_Iteration_Line_Trace_End)};

		//Depending on whether  there is a impact point, assign the corresponding value to "Distance_In_Grid_Scan_For_Hop_Hit_Results_Current_Iteration".
		if(Grid_Scan_Hop_Hit_Traces[Index].bBlockingHit) Distance_In_Grid_Scan_For_Hop_Hit_Results_Current_Iteration = Current_Iteration_Distance_If_Blocking_Hit;
		else if(!Grid_Scan_Hop_Hit_Traces[Index].bBlockingHit) Distance_In_Grid_Scan_For_Hop_Hit_Results_Current_Iteration = Current_Iteration_Distance_If_No_Blocking_Hit;


		/*Getting the trace distance in the previous loop iteration*/
		//Checking to see if the previous line trace has a blocking or not. Depending on this answer the appropriate 
		//distance value will be assigned to the global variable "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
		int Previous_Index{Index -1};

		//Getting a reference to the previous element (in respect to the current iteration of this nested for loop) in the TArray "Grid_Line_Hit_Trace".
		FHitResult& Previous_Iteration_Line_Trace_Reference{Grid_Scan_Hop_Hit_Traces[Previous_Index]};

		/*Getting the trace distance in previous loop iteration*/
		//Checking to see if the previous line trace has a blocking hit or not. Depending on this answer the appropriate 
		//distance value will be assigned to the global variable "Distance_In_Grid_Scan_For_Hit_Results_Previous_Iteration".
			
		//If there is a blocking hit this, is the distance from the line trace start to the impact point.
		const float Previous_Iteration_Distance_If_Blocking_Hit{Previous_Iteration_Line_Trace_Reference.Distance};

		//If there is no impact point get the distance between the line trace start and its end.
		const FVector_NetQuantize Previous_Iteration_Line_Trace_Start{Previous_Iteration_Line_Trace_Reference.TraceStart};
		const FVector_NetQuantize Previous_Iteration_Line_Trace_End{Previous_Iteration_Line_Trace_Reference.TraceEnd};
		const double Previous_Iteration_Distance_If_No_Blocking_Hit{UKismetMathLibrary::Vector_Distance(Previous_Iteration_Line_Trace_Start, Previous_Iteration_Line_Trace_End)};

		//Depending on whether  there is a impact point, assign the corresponding value to "Distance_In_Grid_Scan_For_Hop_Hit_Results_Previous_Iteration".
		if(Previous_Iteration_Line_Trace_Reference.bBlockingHit) Distance_In_Grid_Scan_For_Hop_Hit_Results_Previous_Iteration = Previous_Iteration_Distance_If_Blocking_Hit;
		else if(!Previous_Iteration_Line_Trace_Reference.bBlockingHit) Distance_In_Grid_Scan_For_Hop_Hit_Results_Previous_Iteration = Previous_Iteration_Distance_If_No_Blocking_Hit;
			
		//Get the difference between the assigned distances (whether there is a blocking hit or not) of current and the previous line traces.
		const double Distance_Between_Current_And_Previous_Line_Trace{Distance_In_Grid_Scan_For_Hop_Hit_Results_Current_Iteration - Distance_In_Grid_Scan_For_Hop_Hit_Results_Previous_Iteration};

		//If the difference between the assigned distances of current and the previous line traces is greater than 7 units this means the previous line trace is the line is right under the top edge of the wall.
		//In result, said line trace from the previous loop iteration is added to the TArray "Front_Wall_Top_Edge_Hop_Traces" and the nested for loop "continues"
		if(Distance_Between_Current_And_Previous_Line_Trace > 7)
		{
			Front_Wall_Top_Edge_Hop_Traces.Add(Previous_Iteration_Line_Trace_Reference);
			Debug::Print(FString(TEXT("Differences in Front_Wall_Top_Edge_Hop_Traces: ") + FString::FromInt(Distance_Between_Current_And_Previous_Line_Trace)), FColor::Blue, 5);
			//Draw_Debug_Sphere(Previous_Iteration_Line_Trace_Reference.ImpactPoint, 5.f, FColor::Black, 10.f, false, 1);
			continue;
		}

		else continue;
	}
}

void UCustom_Movement_Component::Realize_Front_Wall_Top_Edge_Best_Hop_Hit()
{
	//During the first iteration of the loop (Index == 0), assign the first element of the array in "Front_Wall_Top_Edge_Hop_Traces" into the global variable "Front_Wall_Top_Edge_Best_Hop_Hit".
	//This will only happen one time during this for loop. The global FHitResult variable "Front_Wall_Top_Edge_Best_Hit" needs to have a FHitResult assigned to it so said FHitResult
	//can be compared to the other FHitResults in said array. During each for loop iteration, the FHitResult found in the current element of the array (the current Index) will be compared 
	//to the FHitResult which was assigned to the global FHitResult variable "Front_Wall_Top_Edge_Best_Hit" (during Index == 0). When compared to the current location of the character, 
	//if the FHitResult in the element of the array (the current Index) has a smaller delta than that of the FHitResult stored in the global FHitResult variable 
	//"Front_Wall_Top_Edge_Best_Hit" then the FHitResult in the element of the array (the current Index) will replace the current FHitResult stored in the global FHitResult variable
	//"Front_Wall_Top_Edge_Best_Hit". At the end of the for loop the FHitResult which has the lowest delta when compared to the current location of the character will be stored in the 
	//global FHitResult variable "Front_Wall_Top_Edge_Best_Hit".

	const FVector Current_Copmponent_Location{UpdatedComponent->GetComponentLocation()};
	
	for(int Index{}; Index != Front_Wall_Top_Edge_Hop_Traces.Num(); Index++)
	{
		//Initialize the global FHitResult variable with the first element of the array. This will only happen once.
		if(Index == 0) Front_Wall_Top_Edge_Best_Hop_Hit = Front_Wall_Top_Edge_Hop_Traces[Index];
		
		else
		{	
			//Obtain the locatation of the impact points for the FHitResult stored in the global variable "Front_Wall_Top_Edge_Best_Hop_Hit" and the FHitResult which is
			//at the same element of that as the current loop iteration (Index).
			const FVector Current_Front_Wall_Top_Edge_Best_Hop_Hit_Location{Front_Wall_Top_Edge_Best_Hop_Hit.ImpactPoint};
			const FVector Current_Iteration_Trace_Location{Front_Wall_Top_Edge_Hop_Traces[Index].ImpactPoint};
			
			//Obtain the delta of the impact points for the FHitResult stored in the global variable "Front_Wall_Top_Edge_Best_Hop_Hit" and the FHitResult which is
			//at the same element of that as the current loop iteration (Index), when compared to the current location of the character.
			const double Delta_Between_Current_Iteration_Trace_Location_And_Component_Location
			{UKismetMathLibrary::Vector_Distance(Current_Iteration_Trace_Location, Current_Copmponent_Location)};
			
			const double Delta_Between_Current_Front_Wall_Top_Edge_Best_Hop_Hit_And_Component_Location
			{UKismetMathLibrary::Vector_Distance(Current_Front_Wall_Top_Edge_Best_Hop_Hit_Location, Current_Copmponent_Location)};

			//If the FHitResult in the element of the array (the current Index) has a smaller delta than that of the FHitResult stored in the global FHitResult variable 
			//"Front_Wall_Top_Edge_Best_Hop_Hit" when compared to the current location of the character, then the FHitResult in the element of the array (the current Index) 
			//will replace the current FHitResult stored in the global FHitResult variable "Front_Wall_Top_Edge_Best_Hop_Hit"
			if(Delta_Between_Current_Iteration_Trace_Location_And_Component_Location <= Delta_Between_Current_Front_Wall_Top_Edge_Best_Hop_Hit_And_Component_Location)
			Front_Wall_Top_Edge_Best_Hop_Hit = Front_Wall_Top_Edge_Hop_Traces[Index];

			//If the FHitResult in the element of the array (the current Index) does not have a smaller delta than that of the FHitResult stored in the global FHitResult variable 
			//"Front_Wall_Top_Edge_Best_Hop_Hit" when compared to the current location of the character, then no change happens regarding which FHitResult is stored in the global 
			//FHitResult variable "Front_Wall_Top_Edge_Best_Hop_Hit" and the for loop "continues"
			else continue;
		}

	}

	//Draw_Debug_Sphere(Front_Wall_Top_Edge_Best_Hop_Hit.ImpactPoint, 10.f, FColor::Cyan, 5.f, false, 5.f);
}

bool UCustom_Movement_Component::Get_Hop_Top_Result()
{
	FHitResult Out_Hit{};
		
	//Move the vector forward from its staring location which is the FHitResult "Front_Wall_Top_Edge_Best_Hop_Hit" impact point using the value in "Index_Multiplier". To make the vector move on the charater's forward direction
	//"the normal of the wall must be reversed". To do this the global FRotator variable "Reversed_Front_Wall_Normal_Z" is used. This variable is filled with the current wall that is being 
	//analyzed reversed normal on the Z axis. This happens in the function "&UCustom_Movement_Component::Analyze_Wall_Top_Surface()".
	const FVector Offset{Move_Vector_Forward(Front_Wall_Top_Edge_Best_Hop_Hit.ImpactPoint, Reversed_Front_Wall_Normal_Z, 2.f)};
		
	const FVector Start{Move_Vector_Up(Offset, 15.f)};
	const FVector End{Move_Vector_Down(Start, 20.f)};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		Parkour_Hop_Top_Result_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
		);
		
	//If there is no blocing hit on any of the sphere traces break out of the for loop. This is because the top surface of the wall which is being analyzed had dropped below the threshold which is desired (Sphere trace "End").
	if(!Out_Hit.bBlockingHit)
	return false;

	//If the index is 0 and there is a blocking hit on the Out_Hit of the current loop iteration (Index = 0), assign said FHitResult to the global FHitResult variable "Wall_Top_Result". This is the first and closest FHitResult
	//to the charater on the top surface of the wall which is being analyzed.
	else if(Out_Hit.bBlockingHit && !Out_Hit.bStartPenetrating)
	{
		Hop_Top_Hit_Result = Out_Hit;
		//Draw_Debug_Sphere(Hop_Top_Hit_Result.ImpactPoint, 15.f, FColor::Black, 7.f, false, 7.f);
		return true;
	}

	return false;
}

bool UCustom_Movement_Component::Validate_Absence_Of_Obstacles_Before_Hopping()
{
	if(!Character_Direction_Arrow || !Mesh)
	return false;


	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	const FVector Pelvis_Location{Mesh->GetSocketLocation(FName(TEXT("pelvis")))};
		
	int Index{};
		
	//Check to see if there is an obstacle blocking the path which the character will perform a hop action.
	FVector Offset_Vector_For_Obstacles_Check_Start{};

	FVector Offset_Vector_For_Obstacles_Check_End{};

	FHitResult Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result{};

	for(Index; Index <= 4; Index++)
	{
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
		{
			Offset_Vector_For_Obstacles_Check_Start = Move_Vector_Up(Pelvis_Location, Index * 20.f);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) ||
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			const FVector Offset_Vector_To_Left{Move_Vector_Left(Pelvis_Location, Character_Direction_Arrow_Rotation, 40.f)};
			Offset_Vector_For_Obstacles_Check_Start = Move_Vector_Right(Offset_Vector_To_Left, Character_Direction_Arrow_Rotation, Index * 20.f);
		}
			
		const FVector Start_2{Offset_Vector_For_Obstacles_Check_Start};


	
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
		{
			Offset_Vector_For_Obstacles_Check_End = Move_Vector_Up(Hop_Top_Hit_Result.ImpactPoint, Index * 20.f);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) ||
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			const FVector Offset_Vector_To_Left{Move_Vector_Left(Hop_Top_Hit_Result.ImpactPoint, Character_Direction_Arrow_Rotation, 40.f)};
			Offset_Vector_For_Obstacles_Check_End = Move_Vector_Right(Offset_Vector_To_Left, Character_Direction_Arrow_Rotation, Index * 20.f);
		}

		const FVector End_2{Move_Vector_Backward(Offset_Vector_For_Obstacles_Check_End, Character_Direction_Arrow_Rotation, 25.f)};



		UKismetSystemLibrary::LineTraceSingleForObjects(
			this,
			Start_2,
			End_2,
			Parkour_Detect_Wall_Pipe_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result,
			false
		);

		if(Index == 4 && !Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result.bBlockingHit)
		{
			Debug::Print("There_Is_No_Obstacle_And_Hop_Action_Can_Be_Performed", FColor::Green, 55);
			return true;
		}

		else
		{
			if(Index < 4 && !Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result.bBlockingHit)
			{
				continue;
			}

			else
			{
				Debug::Print("There_Is_An_Obstacle_And_Hop_Action_Can_Not_Be_Performed", FColor::Red, 55);
				return false;
			}
		}
	}

	Debug::Print("There_Is_An_Obstacle_And_Hop_Action_Can_Not_Be_Performed", FColor::Red, 55);
	return false;
}

bool UCustom_Movement_Component::Validate_Can_Fly_Hanging_Jump() const
{
	const FVector Start{Move_Vector_Forward(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 25.f)};
	const FVector End{Move_Vector_Forward(Start, Reversed_Front_Wall_Normal_Z, 100)};

	FHitResult Out_Hit{};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		100.f,
		Validate_Can_Fly_Hanging_Jump_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
	);

	if(!Out_Hit.bBlockingHit || !Out_Hit.bStartPenetrating)
	return true;

	else
	return false;
}

bool UCustom_Movement_Component::Validate_Can_Jump_From_Braced_Climb(const bool& bJump_Forward) const
{
	//Get Character Direction Arrow Location
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	//Get Chaaracter Direction Arrow Rotation
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	FVector Start{};
	FVector End{};

	if(bJump_Forward)
	{
		Start = Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 100.f);
		End = Move_Vector_Forward(Start, Reversed_Front_Wall_Normal_Z, 250);
	}

	else if(!bJump_Forward)
	{
		Start = Move_Vector_Backward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 40.f);
		End = Move_Vector_Backward(Start, Reversed_Front_Wall_Normal_Z, 200);
	}

	FHitResult Out_Hit{};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		100.f,
		Validate_Can_Jump_From_Braced_Climb_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
	);

	if(!Out_Hit.bBlockingHit || !Out_Hit.bStartPenetrating)
	return true;

	else
	return false;
}

bool UCustom_Movement_Component::Validate_Can_Jump_From_Free_Hang() const
{
	//Get Character Direction Arrow Location
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	//Get Chaaracter Direction Arrow Rotation
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	const FVector Start{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 40.f)};
	const FVector End{Move_Vector_Forward(Start, Reversed_Front_Wall_Normal_Z, 100)};

	FHitResult Out_Hit{};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		100.f,
		Validate_Can_Jump_From_Free_Hang_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
	);

	if(!Out_Hit.bBlockingHit || !Out_Hit.bStartPenetrating)
	return true;

	else
	return false;
}

bool UCustom_Movement_Component::Validate_Drop_To_Shimmy(const int& Maximum_Distance_To_Check_For_Drop)
{
	//This function should only be called when the value set in the global FGameplayTag Parkour_State is set to "Parkour.State.Free.Roam".
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	return false;

	//Get the location of the character direction arrow.
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	//Get the rotation of the character direction arrow.
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};
	
	FHitResult Validate_Accelerating_Drop_Result{};

	int Index_1{};
	for(Index_1; Index_1 <= Maximum_Distance_To_Check_For_Drop; Index_1++)
	{
		//Offset the vector forwards so that it is a bit infront of the character.
		const FVector Outermost_Loop_Start{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, Index_1 * 30.f)};
		const FVector Outermost_Loop_End{Move_Vector_Down(Outermost_Loop_Start, 350.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Outermost_Loop_Start,
			Outermost_Loop_End,
			10.f,
			Validate_Accelerating_Drop_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Validate_Accelerating_Drop_Result,
			false
		);
		
		if(Validate_Accelerating_Drop_Result.bStartPenetrating)
		{
			Debug::Print("Accelerating_Drop_Not_Possible_Hanging_Wall_Ahead", FColor::MakeRandomColor(), 20);
			return false;
		}

		else
		{
			if(Validate_Accelerating_Drop_Result.bBlockingHit)
			{
				if(Index_1 == 7)
				{
					Debug::Print("Drop_Not_Found_To_Perform_Accelerating_Drop", FColor::MakeRandomColor(), 20);
					return false;
				}
				

				else
				{
					continue;
				}
			}

			else
			{
				FHitResult Accelerating_Drop_Detect_Wall_Result{};

				int Index_2{};
				for(Index_2; Index_2 <= 7; Index_2++)
				{
					//Offset the vector downwards so that it is at ground level.
					const FVector Inner_Loop_1_Offset_Vector_1{Move_Vector_Down(Character_Direction_Arrow_Location, 200.f)};

					//Offset the vector forwards so that it is a bit infront of the character.
					const FVector Inner_Loop_1_Offset_Vector_2{Move_Vector_Forward(Inner_Loop_1_Offset_Vector_1, Character_Direction_Arrow_Rotation, 30.f)};


					//With each iteration of the loop, if bStartPenetrating is true Offset the vector forwards by 20 units.
					const FVector Inner_Loop_1_Start{Move_Vector_Forward(Inner_Loop_1_Offset_Vector_2, Character_Direction_Arrow_Rotation, Index_2 * 20.f)};
					const FVector Inner_Loop_1_End{Move_Vector_Backward(Inner_Loop_1_Start, Character_Direction_Arrow_Rotation, 30.f)};

					UKismetSystemLibrary::SphereTraceSingleForObjects(
						this,
						Inner_Loop_1_Start,
						Inner_Loop_1_End,
						5.f,
						Validate_Accelerating_Drop_Detect_Wall_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Accelerating_Drop_Detect_Wall_Result,
						false
					);

					if(Accelerating_Drop_Detect_Wall_Result.bStartPenetrating)
					{
						if(Index_2 == 7)
						{
							Debug::Print("Too_Far_To_Perform_Accelerating_Drop", FColor::MakeRandomColor(), 20);
							return false;
						}

						else
						{
							continue;
						}
					}

					else
					{
						if(Accelerating_Drop_Detect_Wall_Result.bBlockingHit)
						{
							Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Accelerating_Drop_Detect_Wall_Result.ImpactNormal);
							
							//Offset the vector forwards using the just set "Reversed_Front_Wall_Normal_Z" so that the vector is is within the wall which is being analyzed.
							const FVector Inner_Loop_2_Offset_Vector_1{Move_Vector_Forward(Accelerating_Drop_Detect_Wall_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 2.f)};
							//Offset the vector down so that is is within the wall which is being analyzed.
							const FVector Inner_Loop_2_Offset_Vector_2{Move_Vector_Down(Inner_Loop_2_Offset_Vector_1, 2.f)};
							
							FHitResult Accelerating_Drop_Wall_Top_Result{};

							int Index_3{};
							for(Index_3; Index_3 <= 7; Index_3++)
							{
								//for each iteration of the loop if "bStartPenetrating" is true offset the vector up. For the first loop within this 2x inner loop this will be gauranteed due to the offsets
								//"Inner_Loop_2_Offset_Vector_2".
								const FVector Inner_Loop_2_Start{Move_Vector_Up(Inner_Loop_2_Offset_Vector_2, Index_3 * 5.f)};
								const FVector Inner_Loop_2_End{Move_Vector_Down(Inner_Loop_2_Start, 10.f)};

								UKismetSystemLibrary::SphereTraceSingleForObjects(
									this,
									Inner_Loop_2_Start,
									Inner_Loop_2_End,
									5.f,
									Validate_Accelerating_Drop_Wall_Top_Trace_Types,
									false,
									TArray<AActor*>(),
									Debug_Action,
									Accelerating_Drop_Wall_Top_Result,
									false
								);

								if(Accelerating_Drop_Wall_Top_Result.bStartPenetrating)
								{
									if(Index_3 == 7)
									{
										Debug::Print("Accelerating_Drop_Not_Possible", FColor::MakeRandomColor(), 20);
										return false;
									}
									
									else
									{
										continue;
									}
								}

								else
								{
									if(Accelerating_Drop_Wall_Top_Result.bBlockingHit)
									{
										Wall_Top_Result = Accelerating_Drop_Wall_Top_Result;
											
										/*Execute one last check to make sure the character can fit in the space*/

										FHitResult Accelerating_Drop_Space_Check{};

										const FVector Inner_Loop_2_Space_Check_Offset_Vector_1{Move_Vector_Backward(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 70.f)};
											
										const FVector Inner_Loop_2_Space_Check_Start{Move_Vector_Down(Inner_Loop_2_Space_Check_Offset_Vector_1, 48.f)};
										const FVector Inner_Loop_2_Space_Check_End{Inner_Loop_2_Space_Check_Start};

										UKismetSystemLibrary::CapsuleTraceSingleForObjects(
											this,
											Inner_Loop_2_Space_Check_Start,
											Inner_Loop_2_Space_Check_End,
											42,
											98,
											Validate_Accelerating_Drop_Space_Check_Trace_Types,
											false,
											TArray<AActor*>(),
											Debug_Action,
											Accelerating_Drop_Space_Check,
											false
										);

										if(Accelerating_Drop_Space_Check.bBlockingHit || Accelerating_Drop_Space_Check.bStartPenetrating)
										{
											Debug::Print("Not_Enough_Space_To_Accelerating_Drop", FColor::MakeRandomColor(), 20);
											return false;
										}
											
										else
										{
											Debug::Print("Accelerating_Drop_Possible", FColor::MakeRandomColor(), 20);
											return true;
										}		
									}
								}
							}
						}
					}
				}
			}
		}
	}

	//The following line is to meet the requirement of the return type of this function.
	return false;
}

void UCustom_Movement_Component::Realize_Wall_Run_Surfaces(const bool& bAnalyze_Characters_Left_Side)
{
	if(!Character_Direction_Arrow)
	return;
	
	//Get the Character Direction Arrow Location and Rotation
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	//Depending on the value set through the input parameter set the value of the local int variable "Side_Of_The_Character_To_Execute_The_Raycast" to determine which side of the 
	//character to launch the ray cast.
	int Side_Of_The_Character_To_Execute_The_Raycast{};
	
	if(bAnalyze_Characters_Left_Side)
	Side_Of_The_Character_To_Execute_The_Raycast = -1;

	else if(!bAnalyze_Characters_Left_Side)
	Side_Of_The_Character_To_Execute_The_Raycast = 1;

	const FVector Start{Character_Direction_Arrow_Location};
	const FVector End{Move_Vector_Right(Start, Character_Direction_Arrow_Rotation, Side_Of_The_Character_To_Execute_The_Raycast * 70.f)};

	//Declare the local FHitResult which will store the hit result of the sphere trace which is performed.
	FHitResult Realize_Wall_Run_Surfaces_Hit_Result{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		Realize_Wall_Run_Surfaces_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Realize_Wall_Run_Surfaces_Hit_Result,
		false
	);

	//Depending on the value passed in through the input parameter, store the hit result into the appropriate global FHitResult variable.
	if(Realize_Wall_Run_Surfaces_Hit_Result.bBlockingHit && Realize_Wall_Run_Surfaces_Hit_Result.GetActor()->ActorHasTag(FName(TEXT("Wall_Run"))))
	{
		if(bAnalyze_Characters_Left_Side)
		{
			Realize_Wall_Run_Left_Side_Hit_Result = Realize_Wall_Run_Surfaces_Hit_Result;
			Debug::Print("Wall_Run_Wall_Found_On_Characters_Left_Side", FColor::MakeRandomColor(), 24);
			return;
		}

		else
		{
			Realize_Wall_Run_Right_Side_Hit_Result = Realize_Wall_Run_Surfaces_Hit_Result;
			Debug::Print("Wall_Run_Wall_Found_On_Characters_Right_Side", FColor::MakeRandomColor(), 24);
			return;
		}
	}
}

void UCustom_Movement_Component::Assign_Wall_Run_Hit_Result(const FHitResult& Wall_Found_On_Left_Side, const FHitResult& Wall_Found_On_Right_Side)
{
	/*Analyze the FHitResults which are passed in via the input arguments to determine which one is closest to the character. This will determine which 
	wall to perform the wall run on if there are two walls which have valid HitResults. This function will only be called if either of the FHitResults 
	passed in via the input parameter has a blocking hit.*/

	//Get the component location
	const FVector Component_Location{UpdatedComponent->GetComponentLocation()};

	//Declare a FVector local variable to store the delta location between the FHitResult that's passed in via "Wall_Found_On_Left_Side" and the character.
	float Distance_Between_Wall_Found_On_Left_Side_And_Character{};
	//Declare a FVector local variable to store the delta location between the FHitResult that's passed in via "Wall_Found_On_Right_Side" and the character.
	float Distance_Between_Wall_Found_On_Right_Side_And_Character{};

	//Check both of the FHitResults that are passed in via the input argument for a valid hit result then proceed to obtain assign the FHitResult with the shortest distance
	//to the global FHitResult "Wall_Run_Hit_Result".

	if(Wall_Found_On_Left_Side.bBlockingHit && Wall_Found_On_Right_Side.bBlockingHit)
	{
		Distance_Between_Wall_Found_On_Left_Side_And_Character = Wall_Found_On_Left_Side.Distance;

		Distance_Between_Wall_Found_On_Right_Side_And_Character = Wall_Found_On_Right_Side.Distance;

		if(Distance_Between_Wall_Found_On_Left_Side_And_Character <= Distance_Between_Wall_Found_On_Right_Side_And_Character)
		{
			Wall_Run_Hit_Result = Wall_Found_On_Left_Side;
			Set_Parkour_Wall_Run_Side(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))));
			
			return;
		}
		

		else
		{
			Wall_Run_Hit_Result = Wall_Found_On_Right_Side;
			Set_Parkour_Wall_Run_Side(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))));

			return;
		}
		
	}
	
	else if(Wall_Found_On_Left_Side.bBlockingHit)
	{
		Wall_Run_Hit_Result = Wall_Found_On_Left_Side;
		Set_Parkour_Wall_Run_Side(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))));

		return;
	}
	
	else if(Wall_Found_On_Right_Side.bBlockingHit)
	{
		Wall_Run_Hit_Result = Wall_Found_On_Right_Side;
		Set_Parkour_Wall_Run_Side(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))));
		
		return;
	}
}

void UCustom_Movement_Component::Wall_Run_Detect_Wall(const bool& bAnalyze_Characters_Left_Side)
{
	if(!Character_Direction_Arrow)
	return;
	
	//Get the Character Direction Arrow Location and Rotation
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	//Depending on the value set through the input parameter set the value of the local int variable "Side_Of_The_Character_To_Execute_The_Raycast" to determine which side of the 
	//character to launch the ray cast.
	int Side_Of_The_Character_To_Execute_The_Raycast{};
	
	if(bAnalyze_Characters_Left_Side)
	Side_Of_The_Character_To_Execute_The_Raycast = -1;

	else if(!bAnalyze_Characters_Left_Side)
	Side_Of_The_Character_To_Execute_The_Raycast = 1;

	//Offset the vector up
	const FVector Offset_Vector_1{Move_Vector_Down(Character_Direction_Arrow_Location, 200.f)};

	//Declare the local FHitResult which will store the hit result of the sphere trace which is performed.
	FHitResult Wall_Run_Detect_Wall_Hit_Result{};

	const FVector Start{Offset_Vector_1};
	const FVector End{Move_Vector_Right(Start, Character_Direction_Arrow_Rotation, Side_Of_The_Character_To_Execute_The_Raycast * 70.f)};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		20.f,
		Realize_Wall_Run_Surfaces_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Wall_Run_Detect_Wall_Hit_Result,
		false
	);

	//Check to see if the local FHitResult "Realize_Wall_Run_Surfaces_Hit_Result" has a blocking hit. If it does set the global FHitResult Wall_Run_Hit_Result to equal it's value.
	//If it doesn't clear the global FHitResult Wall_Run_Hit_Result. This will cause the Wall run to end via the check which happens in "&UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Wall_Run_Position".
	if(Wall_Run_Detect_Wall_Hit_Result.bBlockingHit)
	{
		Wall_Run_Hit_Result = Wall_Run_Detect_Wall_Hit_Result;
	}

	else
	{
		Wall_Run_Hit_Result.Reset(1.f, false);
	}
}

bool UCustom_Movement_Component::Validate_Can_Jump_From_Wall_Run() const
{
	//Get Character Direction Arrow Location
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	//Get Chaaracter Direction Arrow Rotation
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	const FVector Start{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 40.f)};
	const FVector End{Move_Vector_Forward(Start, Reversed_Front_Wall_Normal_Z, 170.f)};

	FHitResult Out_Hit{};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		100.f,
		Validate_Can_Jump_From_Wall_Run_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Out_Hit,
		false
	);

	if(!Out_Hit.bBlockingHit || !Out_Hit.bStartPenetrating)
	return true;

	else
	return false;
}

bool UCustom_Movement_Component::Validate_Drop_Off_Ledge_While_Sprinting()
{
	if(!Character_Direction_Arrow)
	return false;

	//Get the location of the Character_Direction_Arrow
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	//Get the rotation of the Character_Direction_Arrow.
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};
	
	FHitResult Drop_Off_Ledge_While_Sprinting_Sphere_Trace_Hit_Result{};

	//Offset the vector forward so that it is about two feet infront of the character
	const FVector Offset_Vector_Forward{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 170.f)};
	
	const FVector Start_1{Offset_Vector_Forward};
	const FVector End_1{Move_Vector_Down(Start_1, 200.f)};
	
	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_1,
		End_1,
		10.f,
		Validate_Drop_Off_Ledge_While_Sprinting_Trace_Types,
		false,
		Wall_Vault_Actors_To_Ignore,
		Debug_Action,
		Drop_Off_Ledge_While_Sprinting_Sphere_Trace_Hit_Result,
		false
	);

	if(!Drop_Off_Ledge_While_Sprinting_Sphere_Trace_Hit_Result.bBlockingHit && !Drop_Off_Ledge_While_Sprinting_Sphere_Trace_Hit_Result.bStartPenetrating)
	{
		//If the local FHitResult "Drop_Off_Ledge_While_Sprinting_Sphere_Trace_Hit_Result" does not have a blocking hit and bStartPenetrating is false check to see if there is enough
		//space for the character to fit at the location of the end point of the sphere trace which was just perfomred.

		FHitResult Drop_Off_Ledge_While_Sprinting_Capsule_Trace_1_Hit_Result{};

		const FVector Offset_Vector_Up{Move_Vector_Up(Drop_Off_Ledge_While_Sprinting_Sphere_Trace_Hit_Result.TraceEnd, 30.f)};

		const FVector Start_2{Offset_Vector_Up};
		const FVector End_2{Start_2};

		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this,
			Start_2,
			End_2,
			44,
			98.f,
			Validate_Drop_Off_Ledge_While_Sprinting_Trace_Types,
			false,
			Wall_Vault_Actors_To_Ignore,
			Debug_Action,
			Drop_Off_Ledge_While_Sprinting_Capsule_Trace_1_Hit_Result,
			false
		);

		if(!Drop_Off_Ledge_While_Sprinting_Capsule_Trace_1_Hit_Result.bBlockingHit && !Drop_Off_Ledge_While_Sprinting_Capsule_Trace_1_Hit_Result.bStartPenetrating)
		{
			FHitResult Drop_Off_Ledge_While_Sprinting_Capsule_Trace_2_Hit_Result{};
			
			const FVector Offset_Vector_Down{Move_Vector_Down(Character_Direction_Arrow_Location, 100.f)};

			const FVector Start_3{Move_Vector_Forward(Offset_Vector_Down, Character_Direction_Arrow_Rotation, 40.f)};
			const FVector End_3{Move_Vector_Forward(Start_3, Character_Direction_Arrow_Rotation, 300.f)};

			FHitResult Out_Hit{};

			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this,
				Start_3,
				End_3,
				10.f,
				100.f,
				Validate_Drop_Off_Ledge_While_Sprinting_Trace_Types,
				false,
				Wall_Vault_Actors_To_Ignore,
				Debug_Action,
				Drop_Off_Ledge_While_Sprinting_Capsule_Trace_2_Hit_Result,
				false
			);

			if(!Drop_Off_Ledge_While_Sprinting_Capsule_Trace_2_Hit_Result.bBlockingHit && !Drop_Off_Ledge_While_Sprinting_Capsule_Trace_2_Hit_Result.bStartPenetrating)
			return true;

			else
			return false;
		}
	}

	else
	return false;

	//The following line is to meet the return requirement of this function
	return false;
}

bool UCustom_Movement_Component::Realize_Wall_Pipe_Surfaces()
{
	/*This function checks to see if there is a valid "AWall_Pipe_Actor" object in range of the characer. The global pointer of type "AWall_Pipe_Actor" is set,
	bReady_To_Initialize_Parkour_Wall_Pipe is set to true so that the character may begin climbing the "Wall_Pipe" and the forward vector of the object of the 
	"AWall_Pipe_Actor" is stored within the global FVector "Wall_Pipe_Forward_Vector"*/
	
	//Get the Character_Direction_Arrow location.
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	//Get the Character_Direction_Arrow rotation.
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};
	
	const FVector Start{Character_Direction_Arrow_Location};
	//Move the vector forward 50 units so that it ends a good distance away from the character.
	const FVector End{Move_Vector_Forward(Start, Character_Direction_Arrow_Rotation, 140.f)};

	//Develop a "SphereTraceSingleForObjects()". The objects will be set in the character blueprint.
	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		Parkour_Detect_Wall_Pipe_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Realize_Wall_Pipe_Hit_Result,
		false
		);

	if(Realize_Wall_Pipe_Hit_Result.bBlockingHit)
	{
		//Use the local FHitResult "Realize_Wall_Pipe_Hit_Result = Wall_Top_Result" to get a reference to the Custom Actor "AWall_Pipe_Actor" by using the method ".GetActor()".
		Wall_Pipe_Actor = Cast<AWall_Pipe_Actor>(Realize_Wall_Pipe_Hit_Result.GetActor());

		//Check to see if the global pointer "Wall_Pipe_Actor" is valid. Also check to see if the ray cast performed hit the component of the actor which has the tah "Wall_Pipe".
		//The object of the "AWall_Pipe_Actor" has multiple components (meshes). Only the component which has said tag should return a positive hit result. This is the mesh whih the 
		//character will clamp onto.
		if(Wall_Pipe_Actor && Realize_Wall_Pipe_Hit_Result.GetComponent()->ComponentHasTag(FName(TEXT("Wall_Pipe"))))
		{
			//Set the global bool variable "bReady_To_Initialize_Parkour_Wall_Pipe" to true. This variable is used within &UCustom_Movememtn_Component::Decide_Parkour_Action to enable the checks to 
			//set the corresponding parkour action. After the "Parkour_Action" is set within &UCustom_Movememtn_Component::Decide_Parkour_Action the global bool variable "bReady_To_Initialize_Parkour_Wall_Pipe" 
			//will be set back to false.
			bReady_To_Initialize_Parkour_Wall_Pipe = true;

			//Get the forward vector of the "AWall_Pipe_Actor" object. This vector will be used to set the global variable "Reversed_Front_Wall_Normal_Z" using the helper function "Reverse_Wall_Normal_Rotation_Z()".
			//Said variable will be used within &UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb to set the global variable "Reversed_Front_Wall_Normal_Z". "Reversed_Front_Wall_Normal_Z" is used within the 
			//function "&UCustom_Movememtn_Component::Play_Parkour_Montage" to set the characters rotation while the montage is playin (usine montion warping).
			Wall_Pipe_Forward_Vector = Wall_Pipe_Actor->GetActorForwardVector();

			Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Wall_Pipe_Forward_Vector);
			
			Wall_Top_Result = Realize_Wall_Pipe_Hit_Result;
			
			Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result = Wall_Pipe_Actor->Get_Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result();

			Debug::Print("Wall_Pipe_Surface_Found_And_Verified", FColor::Green, 41);
			
			//Return true so that the "if statement" which is being analyzed when the function is called within "&UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb()" passes.
			return true;
		}

		else
		{
			Debug::Print("Wall_Pipe_Surface_Not_Found_And_Verified", FColor::Red, 41);
			//Return false so that the "if statement" which is being analyzed when the function is called within "&UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb()" fails.
			return false;
		}
	}

	else
	{
		//Return false so that the "if statement" which is being analyzed when the function is called within "&UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb()" fails.
		return false;
	}
}

void UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_Initialize_IK_Hands(const bool& bIs_Left_Hand)
{
	/*This function handles the implementation of the IK hands when "Parkour_State" changes to "Parkour.State.Wall.Pipe.Climb". When the input parameter is set to "true" when this
	function is called the implementation of this function will handle the left hand and when it is set to false the right hand will be handled.*/

	//This function will be called only when a montage loaded into the function "&UCustom_Movement_Component::Play_Parkour_Montage"  has "Parkour.State.Initialize.Wall.Pipe.Climb" as the the "In_State" of its Data Asset object.
	//Within the montage an animation notify state is set which triggeres this function within the the class  "UInitiialize_IK_Libs". Also, the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" must have 
	//a blocking hit. This is because this FHitResult will be used to initialize the starting location of the ray casts to be performed within this funtion.
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Pipe.Climb"))) || !Wall_Pipe_Actor || !Initialize_Parkour_IK_Limbs_Hit_Result.bBlockingHit)
	return;

	//Develop a custom FVector using the hit result stored in the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" and the "X" and "Y" location of the Raycast being performed within the "AWall_Pipe_Actor" class.
	const FVector Location_To_Begin_Ray_Casts{FVector(Wall_Pipe_Actor->Get_Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result().TraceStart.X, 
													  Wall_Pipe_Actor->Get_Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result().TraceStart.Y,
													  Initialize_Parkour_IK_Limbs_Hit_Result.ImpactPoint.Z)};
	
	int Index_1{};
	//The product of this varibale will be used to offset the location of the ray cast being performed and in result offset the location of the respective hand during each loop.
	const int Index_1_Multiplier{Index_1 * 2};
	
	//This value represents how many units away from the the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" the respective hand should be when the algorithm in this function is complete.
	const int Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result{40};
	
	//This value represents in which direction the respective limb should move. Because the helper function "Move_Vector_Right" will be used in the Offset_Vector location, if the respective hand
	//is the left hand it needs to be moved to the right so the value to move the hand will be multiplied by 1, if the respective hand is the right hand it will need to be moved to the left so the
	//value to move the hand "(Index_1_Multiplier - Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result)" will need to be multipled by -1. This will invert the result of the
	//"Move_Vector_Right" helper funtion
	int Select_Left_Or_Right_Hand_Value{};

	if(bIs_Left_Hand == true)
	Select_Left_Or_Right_Hand_Value = 1;

	else
	Select_Left_Or_Right_Hand_Value = -1;

	//Offset the vector up so the respective hand looks appropriate to the animation being used.
	FVector Offset_Vector_Up{};
	
	if(bIs_Left_Hand)
	Offset_Vector_Up = Move_Vector_Up(Location_To_Begin_Ray_Casts, 10.f);

	else
	Offset_Vector_Up = Move_Vector_Up(Location_To_Begin_Ray_Casts, 0.f);

	//This is the value to move the respective hand during each iteration of the algorithm.
	const int Move_Vector_For_Right_Or_Left_Hand_Value{Select_Left_Or_Right_Hand_Value * (Index_1_Multiplier - Distance_To_Offset_Hands_From_Initialize_Parkour_IK_Limbs_Hit_Result)};

	FHitResult Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result{};

	FRotator Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z{};

	//Declaring the local FRotator and FVector which will store the location and rotation of the respective hand. The data stored in these variables will be passed in via the "IParkour_Locomotion_Interface" functions
	//"Set_Left_Hand_Shimmy_Location" and "Set_Left_Hand_Shimmy_Rotationn"
	FRotator Hand_Wall_Pipe_Climb_Rotation{};

	FVector Hand_Wall_Pipe_Climb_Location{};

	/*Obtain the "Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result"*/
	for(Index_1; Index_1 <= 4; Index_1++)
	{
		const FVector Offset_Vector_Forward{Move_Vector_Forward(Offset_Vector_Up, Reversed_Front_Wall_Normal_Z, 7.f)};

		const FVector Start{Move_Vector_Right(Offset_Vector_Forward, Reversed_Front_Wall_Normal_Z, Move_Vector_For_Right_Or_Left_Hand_Value)};
		const FVector End{Move_Vector_Left(Start, Reversed_Front_Wall_Normal_Z, Move_Vector_For_Right_Or_Left_Hand_Value)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Shimmying_Initialize_IK_Hands_Detect_Wall_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result,
			false
		);

		//If there is a blocking hit reverse the normal of the impact point on the Z axis by 180 degrees using the helper funtion "Reverse_Wall_Normal_Rotation_Z". This reversed normal will be used within this function alone
		//to set the rotation of the respective hand as well as offset any vectors which need offseting.
		if(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.bBlockingHit)
		{
			Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.ImpactNormal);

			//Use the reversed normal to set the rotation of the respective hand into the local FRotator variable "Hand_Shimmy_Rotation"
			if(bIs_Left_Hand)
			{
				const FVector Offset_Left_Hand_Location{Move_Vector_Left(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z, 5.f)};
				Hand_Wall_Pipe_Climb_Location = Move_Vector_Backward(Offset_Left_Hand_Location, Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z, 3.f);
				
				//These are the best values to rotate the respecive hand which were realized via debugging.
				Hand_Wall_Pipe_Climb_Rotation = FRotator(130, 50, 20);
				break;
			}
			

			else
			{
				const FVector Offset_Left_Hand_Location{Move_Vector_Right(Initialize_Parkour_Shimmying_IK_Hands_Detect_Wall_Hit_Result.ImpactPoint, Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z, 5.f)};
				Hand_Wall_Pipe_Climb_Location = Move_Vector_Backward(Offset_Left_Hand_Location, Initialize_Parkour_Shimmying_IK_Hands_Reversed_Front_Wall_Normal_Z, 1.f);
				
				//These are the best values to rotate the respecive hand which were realized via debugging.
				Hand_Wall_Pipe_Climb_Rotation = FRotator(-50, -55, 30);
				break;
			}
			
		}

		//If there is no blocking hit check to see if the algorithm has reached its maximum threshold. If it has return, otherwise continue.
		else
		{
			if(Index_1 == 4)
			{
				Reset_Parkour_IK_Hands(bIs_Left_Hand);
				return;
			}
			

			else
			{
				continue;
			}
			
		}
	}	

	//Pass in the realized location and rotation of the respecive hand into "UAnimInstance" via the respecive "IParkour_Locomotion_Interface" functions.
	if(bIs_Left_Hand == true)
	{
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Location(Anim_Instance, Hand_Wall_Pipe_Climb_Location);
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Rotation(Anim_Instance, Hand_Wall_Pipe_Climb_Rotation);
	}

	else
	{
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Location(Anim_Instance, Hand_Wall_Pipe_Climb_Location);
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Rotation(Anim_Instance, Hand_Wall_Pipe_Climb_Rotation);
	}

}

void UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands(const bool& bIs_Left_Hand)
{
	if(!Character_Direction_Arrow || !Mesh || !Parkour_Interface || !Anim_Instance || Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return;

	//Get the rotation the character is faceing.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Get the location of the corresponding bone.
	FVector IK_Hand_Bone_Location{};

	if(bIs_Left_Hand)
	{
		IK_Hand_Bone_Location = Mesh->GetSocketLocation(FName(TEXT("ik_hand_l")));
	}

	else if(!bIs_Left_Hand)
	{
		IK_Hand_Bone_Location = Mesh->GetSocketLocation(FName(TEXT("ik_hand_r")));
	}

	//Offset the vector accordingly.
	FVector Offset_Vector_To_Right_Or_Left{};
	
	if(bIs_Left_Hand)
	{
		Offset_Vector_To_Right_Or_Left = Move_Vector_Left(IK_Hand_Bone_Location, Direction_Character_Is_Facing, 10.f);
	}

	else if(!bIs_Left_Hand)
	{
		Offset_Vector_To_Right_Or_Left = Move_Vector_Right(IK_Hand_Bone_Location, Direction_Character_Is_Facing, 20.f);
	}

	//Declaring the local FRotator and FVector which will store the location and rotation of the respective hand. The data stored in these variables will be passed in via the "IParkour_Locomotion_Interface" functions
	//"Set_Left_Hand_Shimmy_Location" and "Set_Left_Hand_Shimmy_Rotationn"
	FRotator Hand_Wall_Pipe_Climb_Rotation{};

	FVector Hand_Wall_Pipe_Climb_Location{};

	
	int Index{};
	for(Index; Index <= 4; Index++)
	{
		//With each iteration of the for loop move the vector to the left or right accordingly.
		FVector Dynamic_Offset_Vector_To_Right_Or_Left{};
		
		if(bIs_Left_Hand)
		{
			Dynamic_Offset_Vector_To_Right_Or_Left = Move_Vector_Right(Offset_Vector_To_Right_Or_Left, Direction_Character_Is_Facing, Index * 5.f);
		}

		else if(!bIs_Left_Hand)
		{
			Dynamic_Offset_Vector_To_Right_Or_Left = Move_Vector_Left(Offset_Vector_To_Right_Or_Left, Direction_Character_Is_Facing, Index * 5.f);
		}

		const FVector Start{Dynamic_Offset_Vector_To_Right_Or_Left};

		//Depending on which hand is being used within this function set the end location accordingly.
		FVector End{};

		if(bIs_Left_Hand)
		{
			End = Move_Vector_Right(Start, Direction_Character_Is_Facing, 10.f);
		}

		else if(!bIs_Left_Hand)
		{
			End = Move_Vector_Left(Start, Direction_Character_Is_Facing, 10.f);
		}

		FHitResult Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands_Hit_Result{};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Detect_Wall_Pipe_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands_Hit_Result,
			false
		);

		if(Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands_Hit_Result.bBlockingHit)
		{
			//Use the reversed normal to set the rotation of the respective hand into the local FRotator variable "Hand_Shimmy_Rotation"
			if(bIs_Left_Hand)
			{
				const FVector Offset_Left_Hand_Location{Move_Vector_Left(Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands_Hit_Result.ImpactPoint, Direction_Character_Is_Facing, 5.f)};
				Hand_Wall_Pipe_Climb_Location = Move_Vector_Forward(Offset_Left_Hand_Location, Direction_Character_Is_Facing, 2.f);
				
				//These are the best values to rotate the respecive hand which were realized via debugging.
				Hand_Wall_Pipe_Climb_Rotation = FRotator(130, 50, 20);
				break;
			}
			
			else
			{
				const FVector Offset_Left_Hand_Location{Move_Vector_Right(Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands_Hit_Result.ImpactPoint, Direction_Character_Is_Facing, 3.f)};
				Hand_Wall_Pipe_Climb_Location = Move_Vector_Forward(Offset_Left_Hand_Location, Direction_Character_Is_Facing, 4.f);
				
				//These are the best values to rotate the respecive hand which were realized via debugging.
				Hand_Wall_Pipe_Climb_Rotation = FRotator(-50, -55, 30);
				break;
			}
		}

		else
		{
			if(Index == 4)
			{
				return;
			}

			else
			{
				continue;
			}
		}
	}

	if(bIs_Left_Hand)
	{
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Rotation(Anim_Instance, Hand_Wall_Pipe_Climb_Rotation);
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Location(Anim_Instance, Hand_Wall_Pipe_Climb_Location);
	}

	else
	{
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Rotation(Anim_Instance, Hand_Wall_Pipe_Climb_Rotation);
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Location(Anim_Instance, Hand_Wall_Pipe_Climb_Location);
	}

}

void UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_Initialize_IK_Feet(const bool& bIs_Left_Foot)
{
	//This function will be called only when a montage loaded into the function "&UCustom_Movement_Component::Play_Parkour_Montage"  has "Parkour.State.Initialize.Wall.Pipe.Climb" as the the "In_State" of its Data Asset object.
	//Within the montage an animation notify state is set which triggeres this function within the the class  "UInitiialize_IK_Libs". Also, the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result" must have 
	//a blocking hit. This is because this FHitResult will be used to initialize the starting location of the ray casts to be performed within this funtion.
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Pipe.Climb"))) || !Initialize_Parkour_IK_Limbs_Hit_Result.bBlockingHit)
	return;

	//Offset the vector for the respective foot down from the global FHitResult "Initialize_Parkour_IK_Limbs_Hit_Result". To keep the aesthetic natural use different values.
	FVector Offset_Vector_Down{};
	
	if(bIs_Left_Foot)
	Offset_Vector_Down = Move_Vector_Down(Initialize_Parkour_IK_Limbs_Hit_Result.ImpactPoint, 80.f);

	else
	Offset_Vector_Down = Move_Vector_Down(Initialize_Parkour_IK_Limbs_Hit_Result.ImpactPoint, 80.f);

	//Offset the vector to the right or left depending on the which foot is being modified.
	FVector Offset_Vector_To_Right_Or_Left{};

	if(bIs_Left_Foot == true)
	Offset_Vector_To_Right_Or_Left = Move_Vector_Left(Offset_Vector_Down, Reversed_Front_Wall_Normal_Z, 15.f);

	else
	Offset_Vector_To_Right_Or_Left = Move_Vector_Right(Offset_Vector_Down, Reversed_Front_Wall_Normal_Z, 15.f);

	int Index{};

	FHitResult Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall{};

	FRotator Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z{};

	FVector Wall_Pipe_Climb_Foot_Location{};

	FRotator Wall_Pipe_Climb_Foot_Rotation{};

	/*Obtain the local FHitResult "Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall"*/
	for(Index; Index <= 2; Index++)
	{
		const FVector Offset_Vector_up{Move_Vector_Up(Offset_Vector_To_Right_Or_Left, Index * 5.f)};
		
		const FVector Start{Move_Vector_Forward(Offset_Vector_up, Reversed_Front_Wall_Normal_Z, 20.f)};
		const FVector End{Move_Vector_Forward(Start, Reversed_Front_Wall_Normal_Z, 10.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Shimmying_Initialize_IK_Feet_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall,
			false
		);

		//If there is a blocking hit reverse the normal of the impact point on the Z axis by 180 degrees using the helper funtion "Reverse_Wall_Normal_Rotation_Z". This reversed normal will be used within this function alone
		//to set the rotation of the respective foot as well as offset any vectors which need offseting.
		if(Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall.bBlockingHit /* && !Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall.bStartPenetrating */)
		{
			Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z = Reverse_Wall_Normal_Rotation_Z(Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall.ImpactNormal);
			
			//Move the vector backwords (from the hit result of the local FHitResult "Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall") so that the feet are not clipping throught the surface of the wall which is being shimmied.
			Wall_Pipe_Climb_Foot_Location = Move_Vector_Backward(Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall.ImpactPoint, Initialize_Parkour_Wall_Pipe_Climb_IK_Feet_Detect_Wall_Reversed_Wall_Normal_On_Z, 15.f);

			//These are the best values to rotate the respecive foot which were realized via debugging. They are used to set the rotation of the respective foot into the local FRotator variable "Foot_Shimmy_Rotation"
			if(bIs_Left_Foot == true)
			Wall_Pipe_Climb_Foot_Rotation = FRotator(45, 30, -80);

			else
			Wall_Pipe_Climb_Foot_Rotation = FRotator(-135, 0, -70);

			//Break out of the for loop because the location and rotation of the respective foot has been realized.
			break;
		}

		//If there is no blocking hit check to see if the algorithm has reached its maximum threshold. If it has return, otherwise continue the algorithm.
		else
		{
			if(Index == 2)
			return;

			else
			continue;
		}
	}

	//Pass in the realized location and rotation of the respecive foot into "UAnimInstance" via the respecive "IParkour_Locomotion_Interface" functions.
	if(bIs_Left_Foot == true)
	{
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Location(Anim_Instance, Wall_Pipe_Climb_Foot_Location);
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Rotation(Anim_Instance, Wall_Pipe_Climb_Foot_Rotation);
	}
	
	else
	{
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Location(Anim_Instance, Wall_Pipe_Climb_Foot_Location);
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Rotation(Anim_Instance, Wall_Pipe_Climb_Foot_Rotation);
	}
}

void UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet(const bool& bIs_Left_Foot)
{
	/* if(!Mesh || !Character_Direction_Arrow || !Parkour_Interface || !Anim_Instance || Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return;

	//Get the rotaion of the character.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Depening on the foot that is being used within this function get the location of the corresponding IK Hand bone.
	FVector IK_Hand_Bone_To_Use{};

	if(bIs_Left_Foot)
	{
		IK_Hand_Bone_To_Use = Mesh->GetSocketLocation(FName(TEXT("ik_foot_l")));
	}

	else if(!bIs_Left_Foot)
	{
		IK_Hand_Bone_To_Use = Mesh->GetSocketLocation(FName(TEXT("ik_foot_r")));
	}

	//Depening on the foot that is being used within this function offset the vector to the left or the right accordingly
	FVector Offset_Vector_To_Left_Or_Right{};

	if(bIs_Left_Foot)
	{
		Offset_Vector_To_Left_Or_Right = Move_Vector_Left(IK_Hand_Bone_To_Use, Direction_Character_Is_Facing, 15.f);
	}

	else if(!bIs_Left_Foot)
	{
		Offset_Vector_To_Left_Or_Right = Move_Vector_Right(IK_Hand_Bone_To_Use, Direction_Character_Is_Facing, 10.f);
	}

	//Offset the vector down accordingly.
	FVector Offset_Vector_Backward{};
	
	Offset_Vector_Backward = Move_Vector_Backward(Offset_Vector_To_Left_Or_Right, Direction_Character_Is_Facing, 15.f);
	

	int Index{};

	FHitResult Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet_Hit_Result{};

	FVector Wall_Pipe_Climb_Foot_Location{};

	FRotator Wall_Pipe_Climb_Foot_Rotation{};

	for(Index; Index <= 2; Index++)
	{
		//For every iteration of the for loop offset the vector forward
		const FVector Start{Move_Vector_Forward(Offset_Vector_Backward, Direction_Character_Is_Facing, Index * 5.f)};
		const FVector End{Move_Vector_Forward(Start, Direction_Character_Is_Facing, 35.f)}; */
	
	if(!Mesh || !Character_Direction_Arrow || !Parkour_Interface || !Anim_Instance || Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return;

	//Get the rotaion of the character.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Depening on the foot that is being used within this function get the location of the corresponding IK Foot bone.
	FVector IK_Foot_Bone_To_Use{};

	if(bIs_Left_Foot)
	{
		IK_Foot_Bone_To_Use = Mesh->GetSocketLocation(FName(TEXT("ik_foot_l")));
	}

	else if(!bIs_Left_Foot)
	{
		IK_Foot_Bone_To_Use = Mesh->GetSocketLocation(FName(TEXT("ik_foot_r")));
	}

	//Develop a vector that uses the corresponding IK Foot location on the "X" and "Y" and the appropriate IK Hand for the "Z". This will create a more aesthetic pose.
	//This vector is developed specifically for the animations being used.
	FVector Custom_Vector_Which_Combines_IK_Foot_X_And_Y_With_IK_Hand_Z_Location{};

	if(bIs_Left_Foot)
	{
		const FVector Custom_Location{IK_Foot_Bone_To_Use.X, IK_Foot_Bone_To_Use.Y, Mesh->GetSocketLocation(FName(TEXT("ik_hand_l"))).Z};
		Custom_Vector_Which_Combines_IK_Foot_X_And_Y_With_IK_Hand_Z_Location = Custom_Location;
	}

	else if(!bIs_Left_Foot)
	{
		const FVector Custom_Location{IK_Foot_Bone_To_Use.X, IK_Foot_Bone_To_Use.Y, Mesh->GetSocketLocation(FName(TEXT("ik_hand_r"))).Z};
		Custom_Vector_Which_Combines_IK_Foot_X_And_Y_With_IK_Hand_Z_Location = Custom_Location;
	}

	//Depending on which foot this function is using offset the vector down accordingly so that the pose always looks natural and asymmetric.
	FVector Offset_Vector_Down{};

	if(bIs_Left_Foot)
	{
		Offset_Vector_Down = Move_Vector_Down(Custom_Vector_Which_Combines_IK_Foot_X_And_Y_With_IK_Hand_Z_Location, 77.f);
	}

	else if(!bIs_Left_Foot)
	{
		Offset_Vector_Down = Move_Vector_Down(Custom_Vector_Which_Combines_IK_Foot_X_And_Y_With_IK_Hand_Z_Location, 70.f);
	}

	//Offset the vector to the left or the right accordingly
	FVector Offset_Vector_To_Left_Or_Right{};

	if(bIs_Left_Foot)
	{
		Offset_Vector_To_Left_Or_Right = Move_Vector_Left(Offset_Vector_Down, Direction_Character_Is_Facing, 10.f);
	}

	else if(!bIs_Left_Foot)
	{
		Offset_Vector_To_Left_Or_Right = Move_Vector_Right(Offset_Vector_Down, Direction_Character_Is_Facing, 10.f);
	}
	
	//Offset the vector forward accordingly.
	FVector Offset_Vector_Forward{};

	Offset_Vector_Forward = Move_Vector_Forward(Offset_Vector_To_Left_Or_Right, Direction_Character_Is_Facing, 7.f);


	int Index{};

	FHitResult Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet_Hit_Result{};

	FVector Wall_Pipe_Climb_Foot_Location{};

	FRotator Wall_Pipe_Climb_Foot_Rotation{};

	for(Index; Index <= 2; Index++)
	{
		//For every iteration of the for loop offset the vector forward
		const FVector Offset_Vector_Forward_With_Each_Iteration{Move_Vector_Forward(Offset_Vector_Forward, Direction_Character_Is_Facing, Index * 5.f)};

		const FVector Start{Offset_Vector_Forward_With_Each_Iteration};
		const FVector End{Move_Vector_Forward(Start, Direction_Character_Is_Facing, 15.f)};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Detect_Wall_Pipe_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet_Hit_Result,
			false
		);

		if(Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet_Hit_Result.bBlockingHit)
		{
			//Move the vector backwords (from the hit result of the local FHitResult "Initialize_Parkour_Shimmying_IK_Feet_Detect_Wall") so that the feet are not clipping throught the surface of the wall which is being shimmied.
			Wall_Pipe_Climb_Foot_Location = Move_Vector_Backward(Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet_Hit_Result.ImpactPoint, Direction_Character_Is_Facing, 15.f);

			//These are the best values to rotate the respecive foot which were realized via debugging. They are used to set the rotation of the respective foot into the local FRotator variable "Foot_Shimmy_Rotation"
			if(bIs_Left_Foot == true)
			Wall_Pipe_Climb_Foot_Rotation = FRotator(45, 30, -80);

			else
			Wall_Pipe_Climb_Foot_Rotation = FRotator(-135, 0, -70);

			//Break out of the for loop because the location and rotation of the respective foot has been realized.
			break;
		}

		else
		{
			if(Index == 2)
			{
				return;
			}

			else
			{
				continue;
			}
		}
	}

	if(bIs_Left_Foot)
	{
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Location(Anim_Instance, Wall_Pipe_Climb_Foot_Location);
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Rotation(Anim_Instance, Wall_Pipe_Climb_Foot_Rotation);
	}

	else
	{
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Location(Anim_Instance, Wall_Pipe_Climb_Foot_Location);
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Rotation(Anim_Instance, Wall_Pipe_Climb_Foot_Rotation);
	}
	
}

bool UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_State_Detect_Wall_Pipe(FHitResult& Parkour_Pipe_Climbing_Wall_Top_Result)
{
	if(!Mesh || !Character_Direction_Arrow || Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return false;
	
	//Get the location of the UpdatedComponent. This which will be used as the start location of the ray cast.
	const FVector Bone_Location{UpdatedComponent->GetComponentLocation()};

	//Get the current rotation of the character. The "Characer_Direction_Arrow" rotation or the "Owning_Player_Character" rotation may be used. In this case The "Characer_Direction_Arrow" is used.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Since the value stored in the global double variable "Forward_Backward_Movement_Value" has a maximum of 1 (value represents the input from the controller for whether the character should move 
	//forward or backward with the value 1 being full input in the respective direction and 0 being no input for the character to move), whatever value is stored in said global variable will be multiplied
	//by 10 within the local double variable "Vertical_Move_Direction_Update". 
	const float Forward_Backward_Movement_Value_Multiplier{10.f};

	//Get the value which the controller is putting into the global double value "Right_Left_Movement_Value" and multiply it by 10. This product is used as an offset value to begin generating the ray 
	//casts in the outer for loop. If the value is a negative number the ray cast will be on the left side of the arrow actor (which means the character is moving to the left). If the value is not
	//a negative number then the character is moving to the right and the sphere traces will be on the right side of the arrow actor. 
	const double Vertical_Move_Direction_Update{Forward_Backward_Movement_Value * Forward_Backward_Movement_Value_Multiplier};
	
	//Offset the start location of the sphere trace backwards by 10 units.
	const float Value_To_Offset_Sphere_Trace_Backwards{10.f};

	const FVector Offset_Vector_Backwards{Move_Vector_Backward(Bone_Location, Direction_Character_Is_Facing, Value_To_Offset_Sphere_Trace_Backwards)};
	const FVector Start{Move_Vector_Up(Offset_Vector_Backwards, Vertical_Move_Direction_Update)};
	const FVector End{Move_Vector_Forward(Start, Direction_Character_Is_Facing, 40.f)};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		Parkour_Detect_Wall_Pipe_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Parkour_Pipe_Climbing_Wall_Top_Result,
		false
	);

	if(Parkour_Pipe_Climbing_Wall_Top_Result.bBlockingHit)
	{
		/*Wall_Pipe_Actor = Cast<AWall_Pipe_Actor>(Parkour_Pipe_Climbing_Wall_Top_Result.GetActor()); */
		
		//This pointer is set in &UCustom_Movement_Component::Realize_Wall_Pipe_Surfaces.
		if(Wall_Pipe_Actor)
		{
			Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result = Wall_Pipe_Actor->Get_Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result();
			Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Wall_Pipe_Actor->GetActorForwardVector());
			return true;
		}

		else
		{
			return false;
		}
	}

	else
	{
		return false;
	}

	//This line is to meet the default return requirement of the function type.
	return false;
}

bool UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body(const FVector& Movemement_Impact_Location) const
{
	if(!Character_Direction_Arrow || Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return true;

	//Get the current rotation of the character. The "Characer_Direction_Arrow" rotation or the "Owning_Player_Character" rotation may be used. In this case The "Characer_Direction_Arrow" is used.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Offset the vector backward
	const FVector Offset_Vector_Backward{Move_Vector_Backward(Movemement_Impact_Location, Direction_Character_Is_Facing, 12.f)};

	//Offset vector up or down based on value of the global double variable "Forward_Backward_Movement_Value".
	const float Forward_Backward_Movement_Value_Multiplier{77.f};
	const double Vertical_Move_Direction_Update{Forward_Backward_Movement_Value * Forward_Backward_Movement_Value_Multiplier};

	const FVector Offset_Vector_Up_Or_Down{Move_Vector_Up(Offset_Vector_Backward, Vertical_Move_Direction_Update)};

	
	const FVector Start{Offset_Vector_Up_Or_Down};
	const FVector End{Move_Vector_Backward(Start, Direction_Character_Is_Facing, 35.f)};

	FHitResult Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body_Hit_Result{};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		10.f,
		15.f,
		Parkour_Detect_Wall_Pipe_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body_Hit_Result,
		false
	);

	if(Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body_Hit_Result.bBlockingHit || 
	Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body_Hit_Result.bStartPenetrating)
	return true;

	else
	return false;
}

bool UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_Detect_End_Of_Wall_Pipe(const FVector& Movemement_Impact_Location) const
{
	if(!Character_Direction_Arrow || Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return true;

	//Get the current rotation of the character. The "Characer_Direction_Arrow" rotation or the "Owning_Player_Character" rotation may be used. In this case The "Characer_Direction_Arrow" is used.
	const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

	//Offset the vector backward
	const FVector Offset_Vector_Backward{Move_Vector_Backward(Movemement_Impact_Location, Direction_Character_Is_Facing, 35.f)};

	//Offset vector up or down based on value of the global double variable "Forward_Backward_Movement_Value".
	const float Forward_Backward_Movement_Value_Multiplier{77.f};
	const double Vertical_Move_Direction_Update{Forward_Backward_Movement_Value * Forward_Backward_Movement_Value_Multiplier};

	const FVector Offset_Vector_Up_Or_Down{Move_Vector_Up(Offset_Vector_Backward, Vertical_Move_Direction_Update)};

	
	const FVector Start{Offset_Vector_Up_Or_Down};
	const FVector End{Move_Vector_Forward(Start, Direction_Character_Is_Facing, 40.f)};

	FHitResult Parkour_Wall_Pipe_Climb_Detect_End_Of_Wall_Pipe_Hit_Result{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		Parkour_Detect_Wall_Pipe_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Parkour_Wall_Pipe_Climb_Detect_End_Of_Wall_Pipe_Hit_Result,
		false
	);

	if(Parkour_Wall_Pipe_Climb_Detect_End_Of_Wall_Pipe_Hit_Result.bBlockingHit)
	return true;

	else
	return false;
}

bool UCustom_Movement_Component::Validate_Can_Maneuver_To_Free_Hang()
{
	if(!Character_Direction_Arrow)
	return false;
	
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	
	const FVector Offset_Vector_Up{Move_Vector_Up(Character_Direction_Arrow_Location, 5.f)};

	const FVector Offset_Vector_Forward{Move_Vector_Forward(Offset_Vector_Up, Character_Direction_Arrow_Rotation, 20.f)};

	int Index_1{};

	FHitResult Can_Maneuver_To_FreeHang_Detect_Wall_Result{};

	for(Index_1; Index_1 <= 7; Index_1++)
	{
		const FVector Start{Move_Vector_Backward(Offset_Vector_Forward, Character_Direction_Arrow_Rotation, Index_1 * 5.f)};
		const FVector End{Move_Vector_Forward(Start, Character_Direction_Arrow_Rotation, 10.f)};
		
		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			5.f,
			Parkour_Detect_Wall_Pipe_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Can_Maneuver_To_FreeHang_Detect_Wall_Result,
			false	
		);

		if(Can_Maneuver_To_FreeHang_Detect_Wall_Result.bStartPenetrating)
		{
			if(Index_1 == 7)
			{
				return false;
			}

			else
			{
				continue;
			}
		}

		else if(!Can_Maneuver_To_FreeHang_Detect_Wall_Result.bStartPenetrating && Can_Maneuver_To_FreeHang_Detect_Wall_Result.bBlockingHit)
		{
			Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Can_Maneuver_To_FreeHang_Detect_Wall_Result.ImpactNormal);

			const FVector Innder_Loop_Offset_Vector_Forward{Move_Vector_Forward(Can_Maneuver_To_FreeHang_Detect_Wall_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, 2.f)};
			
			FHitResult Can_Maneuver_To_FreeHang_Wall_Top_Result{};

			int Index_2{};

			for(Index_2; Index_2 <= 4; Index_2++)
			{
				const FVector Inner_Start{Move_Vector_Up(Innder_Loop_Offset_Vector_Forward, Index_2 * 5.f)};
				const FVector Inner_End{Move_Vector_Down(Inner_Start, 10.f)};

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					this,
					Inner_Start,
					Inner_End,
					5.f,
					Parkour_Detect_Wall_Pipe_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Can_Maneuver_To_FreeHang_Wall_Top_Result,
					false
				);

				if(Can_Maneuver_To_FreeHang_Wall_Top_Result.bStartPenetrating)
				{
					if(Index_2 == 4)
					{
						return false;
					}

					else
					{
						continue;
					}
				}

				else if(!Can_Maneuver_To_FreeHang_Wall_Top_Result.bStartPenetrating && Can_Maneuver_To_FreeHang_Wall_Top_Result.bBlockingHit)
				{
					Wall_Top_Result = Can_Maneuver_To_FreeHang_Wall_Top_Result;
					return true;
				}
			}
		}
	}

	//This line is to meet the return requirements of this functions return type
	return false;
}

bool UCustom_Movement_Component::Validate_Wall_Pipe_Can_Mantle()
{
	if(!Character_Direction_Arrow)
	return false;
	
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};

	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};
	
	
	
	const FVector Check_For_Obstacles_Offset_Forward{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 20.f)};

	const FVector Check_For_Obstacles_Offset_Down{Move_Vector_Down(Check_For_Obstacles_Offset_Forward, 10.f)};
	
	if(Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body(Check_For_Obstacles_Offset_Down))
	return false;
	

	
	const FVector Offset_Vector_Forward{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 100.f)};

	const FVector Start_1{Move_Vector_Up(Offset_Vector_Forward, 100.f)};

	const FVector End_1{Move_Vector_Down(Start_1, 120.f)};

	FHitResult Wall_Pipe_Can_Mantle_Hit_Result{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_1,
		End_1,
		5.f,
		Parkour_Detect_Wall_Pipe_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Wall_Pipe_Can_Mantle_Hit_Result,
		false
	);

	if(Wall_Pipe_Can_Mantle_Hit_Result.bBlockingHit)
	{
		const float Capsule_Component_Half_Height{98.f};

		FHitResult Validate_Mantle_Hit_Result{};

		const FVector Offset_Vector_2{Move_Vector_Up(Wall_Pipe_Can_Mantle_Hit_Result.ImpactPoint, 5.f)};
		const FVector Start_2{Move_Vector_Up(Offset_Vector_2, Capsule_Component_Half_Height)};
		const FVector End_2{Start_2};

		/*Perform a Capsule Trace To determine if there is enough room to perform a mantle onto the surface of the wall which is being shimmied*/
		UKismetSystemLibrary::CapsuleTraceSingleForObjects(
			this,
			Start_2,
			End_2,
			42.f,
			Capsule_Component_Half_Height,
			Parkour_Detect_Wall_Pipe_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Validate_Mantle_Hit_Result,
			false
		);

		//If there is a blocking hit or bStartPenetrating is true this means there is not enough room to perform a mantle onto the surface which is being shimmied.
		//Therefore return false. Otherwise return true.
		if(Validate_Mantle_Hit_Result.bBlockingHit || Validate_Mantle_Hit_Result.bStartPenetrating)
		{
			Debug::Print("Can't_Mantle_Up_On_Surface", FColor::Black, 14);
			return false;
		}

		else
		{
			Debug::Print("Can_Mantle_Up_On_Surface", FColor::Green, 14);
			Wall_Top_Result = Wall_Pipe_Can_Mantle_Hit_Result;
			return true;
		}
	}

	else
	{
		return false;
	}
}

bool UCustom_Movement_Component::Validate_Wall_Pipe_Hop_Location(const double& Wall_Pipe_Horizontal_Hop_Distance, const double& Wall_Pipe_Vertical_Hop_Distance)
{
	if(!Character_Direction_Arrow || !Mesh)
	return false;


	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	const FVector Pelvis_Location{Mesh->GetSocketLocation(FName(TEXT("pelvis")))};

	
	const FVector Offset_Vector_Up_Or_Down{Move_Vector_Up(Pelvis_Location, Wall_Pipe_Vertical_Hop_Distance)};

	const FVector Offset_Vector_Left_Or_Right{Move_Vector_Right(Offset_Vector_Up_Or_Down, Character_Direction_Arrow_Rotation, Wall_Pipe_Horizontal_Hop_Distance)};

	

	FHitResult Wall_Pipe_Hop_Location_Hit_Result{};

	const FVector Start_1{Move_Vector_Backward(Offset_Vector_Left_Or_Right, Character_Direction_Arrow_Rotation, 10.f)};

	const FVector End_1{Move_Vector_Forward(Start_1, Character_Direction_Arrow_Rotation, 50.f)};
	
	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_1,
		End_1,
		10.f,
		Parkour_Detect_Wall_Pipe_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Wall_Pipe_Hop_Location_Hit_Result,
		false
	);

	if(!Wall_Pipe_Hop_Location_Hit_Result.bStartPenetrating && Wall_Pipe_Hop_Location_Hit_Result.bBlockingHit)
	{
		Debug::Print("Wall_Pipe_Actor_Found", FColor::Red, 72);

		int Index{};
		
		//Check to see if there is an obstacle blocking the path which the character will perform a hop action.
		FVector Offset_Vector_For_Obstacles_Check_Start{};

		FVector Offset_Vector_For_Obstacles_Check_End{};

		FHitResult Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result{};

		for(Index; Index <= 4; Index++)
		{
			if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
			{
				Offset_Vector_For_Obstacles_Check_Start = Move_Vector_Up(Pelvis_Location, Index * 20.f);
			}

			else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) ||
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
			{
				const FVector Offset_Vector_To_Left{Move_Vector_Left(Pelvis_Location, Character_Direction_Arrow_Rotation, 40.f)};
				Offset_Vector_For_Obstacles_Check_Start = Move_Vector_Right(Offset_Vector_To_Left, Character_Direction_Arrow_Rotation, Index * 20.f);
			}
			
			const FVector Start_2{Offset_Vector_For_Obstacles_Check_Start};


	
			if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
			{
				Offset_Vector_For_Obstacles_Check_End = Move_Vector_Up(Wall_Pipe_Hop_Location_Hit_Result.ImpactPoint, Index * 20.f);
			}

			else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) ||
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
			{
				const FVector Offset_Vector_To_Left{Move_Vector_Left(Wall_Pipe_Hop_Location_Hit_Result.ImpactPoint, Character_Direction_Arrow_Rotation, 40.f)};
				Offset_Vector_For_Obstacles_Check_End = Move_Vector_Right(Offset_Vector_To_Left, Character_Direction_Arrow_Rotation, Index * 20.f);
			}

			const FVector End_2{Move_Vector_Backward(Offset_Vector_For_Obstacles_Check_End, Character_Direction_Arrow_Rotation, 25.f)};



			UKismetSystemLibrary::LineTraceSingleForObjects(
				this,
				Start_2,
				End_2,
				Parkour_Detect_Wall_Pipe_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result,
				false
			);

			if(Index == 4 && !Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result.bBlockingHit)
			{
				Debug::Print("There_Is_No_Obstacle_In_The_Way", FColor::Green, 73);

				//Check to see if the location where the character will hop to will result in the character being partially clamped to the Wall Pipe actor.
				//When this happens the character will get stuck with half of the body clamped onto the Wall Pipe actor and the other half of the body floating.
				FVector Offset_Detect_End_Of_Wall_Pipe_Valid_Check{};
				if(Vertical_Hop_Distance > 0.f)
				{
					Offset_Detect_End_Of_Wall_Pipe_Valid_Check = Move_Vector_Up(Wall_Pipe_Hop_Location_Hit_Result.ImpactPoint, 45.f);
				}

				else
				{
					Offset_Detect_End_Of_Wall_Pipe_Valid_Check = Move_Vector_Down(Wall_Pipe_Hop_Location_Hit_Result.ImpactPoint, 20.f);
				}

				if(Parkour_Wall_Pipe_Climb_Detect_End_Of_Wall_Pipe(Offset_Detect_End_Of_Wall_Pipe_Valid_Check))
				{
					Debug::Print("Hop_Location_Is_Not_Beyond_Wall_Pipe_End_Point", FColor::Green, 74);
				
					Wall_Pipe_Actor = Cast<AWall_Pipe_Actor>(Wall_Pipe_Hop_Location_Hit_Result.GetActor());

					if(Wall_Pipe_Actor && Wall_Pipe_Hop_Location_Hit_Result.GetComponent()->ComponentHasTag(FName(TEXT("Wall_Pipe"))))
					{
						Debug::Print("Wall_Pipe_Climb_Validated", FColor::Green, 72);
					
						//Get the forward vector of the "AWall_Pipe_Actor" object. This vector will be used to set the global variable "Reversed_Front_Wall_Normal_Z" using the helper function "Reverse_Wall_Normal_Rotation_Z()".
						//Said variable will be used within &UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb to set the global variable "Reversed_Front_Wall_Normal_Z". "Reversed_Front_Wall_Normal_Z" is used within the 
						//function "&UCustom_Movememtn_Component::Play_Parkour_Montage" to set the characters rotation while the montage is playin (usine montion warping).
						Wall_Pipe_Forward_Vector = Wall_Pipe_Actor->GetActorForwardVector();
			
						Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Wall_Pipe_Forward_Vector);
				
						Wall_Top_Result = Wall_Pipe_Hop_Location_Hit_Result;
			
						Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result = Wall_Pipe_Actor->Get_Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result();

						return true;
					}

					else
					{
						Debug::Print("Wall_Pipe_Climb_Not_Validated", FColor::Red, 72);
						return false;
					}
				}
		
				else
				{
					Debug::Print("Hop_Location_Is_Beyond_Wall_Pipe_End_Point", FColor::Red, 74);

					return false;
				}	
			}

			else
			{
				if(Index < 4 && !Wall_Pipe_Hop_No_Obstacle_Check_Hit_Result.bBlockingHit)
				{
					continue;
				}

				else
				{
					Debug::Print("There_Is_An_Obstacle_In_The_Way", FColor::Red, 73);
					return false;
				}
			}
		}
	}

	else
	{
		Debug::Print("Wall_Pipe_Actor_Not_Found", FColor::Red, 72);
		return false;
	}

	Debug::Print("Wall_Pipe_Actor_Not_Found", FColor::Red, 72);
	return false;
}

bool UCustom_Movement_Component::Validate_Foot_Contact_With_Ground(const bool& bIs_Left_Foot) const
{
	if(!Mesh)
	return false;

	//Store the location of the foot which will be analyzed. This location will be used to generate the offset for the required sphere trace.
	FVector Foot_To_Be_Analyzed{};
	
	if(bIs_Left_Foot)
	Foot_To_Be_Analyzed = Mesh->GetSocketLocation(FName(TEXT("foot_l")));

	else
	Foot_To_Be_Analyzed = Mesh->GetSocketLocation(FName(TEXT("foot_r")));

	//Offset the location to begin the respective foot's sphere trace upwards.
	const FVector Start{Move_Vector_Up(Foot_To_Be_Analyzed, 30.f)};

	const FVector End{Move_Vector_Down(Start, 70.f)};

	FHitResult Validate_Foot_Contact_With_Ground_Hit_Result{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		Validate_Foot_Contact_With_Ground_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Validate_Foot_Contact_With_Ground_Hit_Result,
		false
	);

	if(Validate_Foot_Contact_With_Ground_Hit_Result.bBlockingHit)
	return true;
	
	else
	return false;
}

bool UCustom_Movement_Component::Validate_Jumping_Destination_Ground_Surface(const bool& bCharacter_Is_Walking)
{
	if(!Character_Direction_Arrow)
	return false;

	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	int Distance_To_Check_For_Landing_Destination{};

	if(bCharacter_Is_Walking)
	Distance_To_Check_For_Landing_Destination = 10;

	else
	Distance_To_Check_For_Landing_Destination = 15;

	int Index{};

	for(Index; Index <= Distance_To_Check_For_Landing_Destination; Index++)
	{
		//Set the starting location of the sphere traces.
		const FVector Offset_Vector_Up{Move_Vector_Up(Character_Direction_Arrow_Location, 20.f)};
		
		const FVector Start_1{Move_Vector_Forward(Offset_Vector_Up, Character_Direction_Arrow_Rotation, 100.f + (Index * 20.f))};

		const FVector End_1{Move_Vector_Down(Start_1, 150.f)};

		FHitResult Validate_Jumping_Destination_Ground_Surface_Hit_Result{};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start_1,
			End_1,
			10.f,
			Validate_Jumping_Destination_Ground_Surface_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Validate_Jumping_Destination_Ground_Surface_Hit_Result,
			false
		);

		if(Validate_Jumping_Destination_Ground_Surface_Hit_Result.bBlockingHit)
		{
			Debug::Print("Jumping_Destination_Found", FColor::Green, 80);

			//Check to make sure there is no obstacle which will hinder the character from reaching the landing destination.

			const FVector Start_2{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 100.f)};

			const FVector Offset_Obstacle_Check{Move_Vector_Up(Start_2, Validate_Jumping_Destination_Ground_Surface_Hit_Result.ImpactPoint.Z)};

			const FVector End_2{Move_Vector_Forward(Offset_Obstacle_Check, Character_Direction_Arrow_Rotation, Index * 20.f)};

			FHitResult Validate_Jumping_Destination_For_Obstacles{};

			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this,
				Start_2,
				End_2,
				5.f,
				100.f,
				Validate_Jumping_Destination_Ground_Surface_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Validate_Jumping_Destination_For_Obstacles,
				false
			);

			if(Validate_Jumping_Destination_For_Obstacles.bBlockingHit || Validate_Jumping_Destination_For_Obstacles.bStartPenetrating)
			{
				Debug::Print("There_Is_An_Obstacle_Between_Character_And_Jumping_Destination", FColor::Red, 81);
				return false;
			}
			
			else
			{
				Debug::Print("There_Is_No_Obstacle_Between_Character_And_Jumping_Destination", FColor::Green, 81);
				
				//Check to see if there is a drop a few units ahead of the landing destination. The result of this check will detemine how the character will land.

				const FVector Offset_Check_For_Drop_1{Move_Vector_Up(Validate_Jumping_Destination_Ground_Surface_Hit_Result.ImpactPoint, 70.f)};
				
				const FVector Start_3{Move_Vector_Forward(Offset_Check_For_Drop_1, Character_Direction_Arrow_Rotation, 150.f)};

				const FVector End_3{Move_Vector_Down(Start_3, 250)};

				FHitResult Validate_Jumping_Destination_For_Drop{};

				UKismetSystemLibrary::SphereTraceSingleForObjects(
					this,
					Start_3,
					End_3,
					10.f,
					Validate_Jumping_Destination_Ground_Surface_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Validate_Jumping_Destination_For_Drop,
					false
				);

				if(Validate_Jumping_Destination_For_Drop.bBlockingHit)
				{
					Debug::Print("There_Is_No_Drop_After_Landing_Destination", FColor::Green, 82);
					
					//Check to make sure there is enough room for the character to land on.

					const FVector Check_For_Space_1{Move_Vector_Forward(Validate_Jumping_Destination_Ground_Surface_Hit_Result.ImpactPoint, Character_Direction_Arrow_Rotation, 10.f)};

					const FVector Check_For_Space_2{Move_Vector_Up(Check_For_Space_1, 105.f)};

					FHitResult Validate_Jumping_Destination_For_Space{};

					UKismetSystemLibrary::CapsuleTraceSingleForObjects(
						this,
						Check_For_Space_2,
						Check_For_Space_2,
						40.f,
						100.f,
						Validate_Jumping_Destination_Ground_Surface_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Validate_Jumping_Destination_For_Space,
						false
					);

					if(!Validate_Jumping_Destination_For_Space.bBlockingHit && !Validate_Jumping_Destination_For_Space.bStartPenetrating)
					{
						Debug::Print("There_Is_Enough_Room_For_Character_Land", FColor::Green, 83);

						Wall_Top_Result = Validate_Jumping_Destination_Ground_Surface_Hit_Result;
						Reversed_Front_Wall_Normal_Z = Character_Direction_Arrow_Rotation;
						return true;
					}

					else
					{
						Debug::Print("There_Is_Not_Enough_Room_For_Character_Land", FColor::Red, 83);
						return false;
					}

				}

				else
				{
					Debug::Print("There_Is_A_Drop_After_Landing_Destination", FColor::Red, 82);
					return false;
				}
			}
		}

		else
		{
			if(Index < Distance_To_Check_For_Landing_Destination)
			{
				continue;
			}

			else
			{
				Debug::Print("Jumping_Destination_Not_Found", FColor::Red, 80);
				return false;
			}
		}
	}
	
	return false;
}

void UCustom_Movement_Component::Detect_Balance_Traversal_Actors(const FVector& Initial_Balance_Traversal_Actor_Forward_Vector_Location, const FRotator& Initial_Balance_Traversal_Actor_Rotation, const int& Scan_Width, const int& Scan_Height)
{
	if(!Detect_Balance_Traversal_Actors_Hit_Traces.IsEmpty())
	Detect_Balance_Traversal_Actors_Hit_Traces.Empty();
	
	int Index_1{};

	for(Index_1; Index_1 <= Scan_Width; Index_1++)
	{
		const FVector Offset_Vector_Forward{Move_Vector_Forward(Initial_Balance_Traversal_Actor_Forward_Vector_Location, Initial_Balance_Traversal_Actor_Rotation, 100)};
		
		const FVector Offset_Vector_Down{Move_Vector_Down(Offset_Vector_Forward, 100)};

		const FVector Offset_Vector_Left{Move_Vector_Left(Offset_Vector_Down, Initial_Balance_Traversal_Actor_Rotation, 300)};

		const FVector Set_Width_Trace_Location{Move_Vector_Right(Offset_Vector_Left, Initial_Balance_Traversal_Actor_Rotation, Index_1 * 40)};
		
		int Index_2{};

		FHitResult Out_Hit{};

		//Develop a nested for loop to handle the height of the grid scan.
		//"Scan_Height_Value" will be filled with the the value 30.
		for(Index_2; Index_2 <= Scan_Height; Index_2++)
		{
			const FVector Start{Move_Vector_Up(Set_Width_Trace_Location, Index_2 * 40)};
			
			//Move the vector forwards so that the line traces end a good distance away from the character
			const FVector End{Move_Vector_Forward(Start, Initial_Balance_Traversal_Actor_Rotation, 350.f)};

			//Develop the line traces
			UKismetSystemLibrary::SphereTraceSingleForObjects(
				this,
				Start,
				End,
				15.f,
				Balance_Traversal_Actors_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Out_Hit,
				true
			);

			//Add the "Out_Hit" generated by each for loop iteration to the array "Detect_Balance_Traversal_Actors_Hit_Traces".
			Detect_Balance_Traversal_Actors_Hit_Traces.Add(Out_Hit);
		}
	}
}

void UCustom_Movement_Component::Analyze_Detect_Balance_Traversal_Actors_Hit_Traces_For_Best_Hit()
{
	const FVector Current_Copmponent_Location{UpdatedComponent->GetComponentLocation()};
	
	for(int Index{}; Index != Detect_Balance_Traversal_Actors_Hit_Traces.Num(); Index++)
	{
		//Initialize the global FHitResult variable with the first element of the array. This will only happen once.
		if(Index == 0) Balance_Traversal_Actors_Best_Hit = Detect_Balance_Traversal_Actors_Hit_Traces[Index];
		
		else
		{	
			//Obtain the locatation of the impact points for the FHitResult stored in the global variable "Balance_Traversal_Actors_Best_Hit" and the FHitResult which is
			//at the same element of that as the current loop iteration (Index).
			const FVector Current_Balance_Traversal_Actors_Best_Hit_Location{Balance_Traversal_Actors_Best_Hit.ImpactPoint};
			const FVector Current_Iteration_Trace_Location{Detect_Balance_Traversal_Actors_Hit_Traces[Index].ImpactPoint};
			
			//Obtain the delta of the impact points for the FHitResult stored in the global variable "Balance_Traversal_Actors_Best_Hit" and the FHitResult which is
			//at the same element of that as the current loop iteration (Index), when compared to the current location of the character.
			const double Delta_Between_Current_Iteration_Trace_Location_And_Component_Location
			{UKismetMathLibrary::Vector_Distance(Current_Iteration_Trace_Location, Current_Copmponent_Location)};
			
			const double Delta_Between_Current_Balance_Traversal_Actors_Best_Hit_And_Component_Location
			{UKismetMathLibrary::Vector_Distance(Current_Balance_Traversal_Actors_Best_Hit_Location, Current_Copmponent_Location)};

			//If the FHitResult in the element of the array (the current Index) has a smaller delta than that of the FHitResult stored in the global FHitResult variable 
			//"Balance_Traversal_Actors_Best_Hit" when compared to the current location of the character, then the FHitResult in the element of the array (the current Index) 
			//will replace the current FHitResult stored in the global FHitResult variable "Balance_Traversal_Actors_Best_Hit"
			if(Delta_Between_Current_Iteration_Trace_Location_And_Component_Location <= Delta_Between_Current_Balance_Traversal_Actors_Best_Hit_And_Component_Location)
			{
				Balance_Traversal_Actors_Best_Hit = Detect_Balance_Traversal_Actors_Hit_Traces[Index];
				Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Balance_Traversal_Actors_Best_Hit.ImpactNormal);
			}
			

			//If the FHitResult in the element of the array (the current Index) does not have a smaller delta than that of the FHitResult stored in the global FHitResult variable 
			//"Balance_Traversal_Actors_Best_Hit" when compared to the current location of the character, then no change happens regarding which FHitResult is stored in the global 
			//FHitResult variable "Balance_Traversal_Actors_Best_Hit" and the for loop "continues"
			else
			{
				continue;
			} 
		}	
	}

	Draw_Debug_Sphere(Balance_Traversal_Actors_Best_Hit.ImpactPoint, 20.f, FColor::Cyan, 5.f, false, 5.f);
}

bool UCustom_Movement_Component::Validate_Balance_Traversal_Location()
{
	if(!Balance_Traversal_Actors_Best_Hit.bBlockingHit || !Owning_Player_Character || !Balance_Traversal_Actor)
	return false;

	const FVector Character_Location{Owning_Player_Character->GetActorLocation()};
	
	const FVector Offset_Vector_Forward{Move_Vector_Forward(Balance_Traversal_Actors_Best_Hit.ImpactPoint, Reversed_Front_Wall_Normal_Z, 10.f)};

	int Index{};

	FHitResult Out_Hit_1{};

	for(Index; Index <= 7; Index++)
	{
		const FVector Start_1{Move_Vector_Up(Offset_Vector_Forward, Index * 7.f)};
		const FVector End_1{Move_Vector_Down(Start_1, 10.f)};
		
		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start_1,
			End_1,
			7.f,
			Balance_Traversal_Actors_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Out_Hit_1,
			false
		);

		if(Out_Hit_1.bStartPenetrating)
		{
			if(Index == 7)
			{
				Debug::Print("Validate_Balance_Traversal_Warp_Location_Failed", FColor::Red, 74);
				return false;
			}

			else if(Index < 7)
			{
				continue;
			}
		}

		else if(Out_Hit_1.bBlockingHit && !Out_Hit_1.bStartPenetrating)
		{
			Debug::Print("Validate_Balance_Traversal_Warp_Location_Succeeded", FColor::Green, 74);
			
			//check to see if there is an obstacle in the way.

			const FVector Custom_Character_Location{Character_Location.X, Character_Location.Y, Character_Location.Z - 40.f};


			int Index_2{};

			FHitResult Out_Hit_2{};

			for(Index_2; Index_2 <= 4; Index_2++)
			{
				const FVector Start_2{Move_Vector_Up(Custom_Character_Location, Index_2 * 40.f)};

				const FVector Offset_End_Vector_Up{Move_Vector_Up(Out_Hit_1.ImpactPoint, 10.f)};

				const FVector End_2{Move_Vector_Up(Offset_End_Vector_Up, Index_2 * 40.f)};
			
				UKismetSystemLibrary::LineTraceSingleForObjects(
					this,
					Start_2,
					End_2,
					Balance_Traversal_Actors_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Out_Hit_2,
					false
				);

				if(!Out_Hit_2.bBlockingHit)
				{
					if(Index_2 != 4)
					{
						continue;
					}

					else if(Index_2 == 4)
					{
						Debug::Print("Validate_Balance_Traversal_Warp_Location_For_No_Obstacles_Succeeded", FColor::Green, 75);

						//Check for enough room for the character to land.

						const FVector Start_3{Move_Vector_Up(Out_Hit_1.ImpactPoint, 110.f)};

						FHitResult Out_Hit_3{};

						UKismetSystemLibrary::CapsuleTraceSingleForObjects(
							this,
							Start_3,
							Start_3,
							40.f,
							100.f,
							Balance_Traversal_Actors_Trace_Types,
							false,
							TArray<AActor*>(),
							Debug_Action,
							Out_Hit_3,
							false
						);

						if(!Out_Hit_3.bBlockingHit && !Out_Hit_3.bStartPenetrating)
						{
							Debug::Print("Validate_Balance_Traversal_Warp_Location_For_Enough_Room_For_Character_To_Land_Succeeded", FColor::Green, 77);

							Wall_Top_Result = Out_Hit_1;

							return true;
						}
				
						else
						{
							Debug::Print("Validate_Balance_Traversal_Warp_Location_For_Enough_Room_For_Character_To_Land_Failed", FColor::Red, 77);
							return false;
						}
					}
				}

				else
				{
					Debug::Print("Validate_Balance_Traversal_Warp_Location_For_No_Obstacles_Failed", FColor::Red, 75);
					return false;
				}
			}
		}
	}

	//THis line of code is to meet the return requirmenet of this function type.
	return false;
}

bool UCustom_Movement_Component::Parkour_Balance_Walk_Detect_Balance_Surface(FHitResult& Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result_Reference, FHitResult& Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference)
{
	//Check to see if the corresponding pointers are valid. If not return safely.
	if(!Character_Direction_Arrow)
	return false;

	//Get the location and rotation of the character direction arrow.
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	/*Check to see if there is a surface for the character to walk on.*/
	
	//Offset the vector forward so that it is right in front of the characters body.
	const FVector Offset_Balance_Walk_Detect_Balance_Surface_Check_Forward{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 10.f)};
	//Offset the vector down so that it is at the same height with the top of the character's head.
	const FVector Start_1{Move_Vector_Down(Offset_Balance_Walk_Detect_Balance_Surface_Check_Forward, 30.f)};
	//Offset the vector down so that it can detect if there is a surface which the character can "Balance_Walk" on.
	const FVector End_1{Move_Vector_Down(Start_1, 170.f)};

	FHitResult Balance_Walk_Detect_Balance_Surface_Hit_Result{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_1,
		End_1,
		10.f,
		Balance_Traversal_Actors_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Balance_Walk_Detect_Balance_Surface_Hit_Result,
		false
	);
	
	if(!Balance_Walk_Detect_Balance_Surface_Hit_Result.bBlockingHit || Balance_Walk_Detect_Balance_Surface_Hit_Result.bStartPenetrating)
	{
		Debug::Print("There_Is_No_Surface_For_The_Character_To_Balance_Walk_On", FColor::Red, 77);
		Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		return false;
	}

	else if(Balance_Walk_Detect_Balance_Surface_Hit_Result.bBlockingHit && !Balance_Walk_Detect_Balance_Surface_Hit_Result.bStartPenetrating)
	{
		Debug::Print("There_Is_A_Surface_For_The_Character_To_Balance_Walk_On", FColor::Green, 77);
		
		//Set global FHitResult "Wall_Top_Result" to equal the same value as the local FHitResult "Balance_Walk_Detect_Balance_Surface_Hit_Result". This will be used to determine if the character 
		//can perfrom a Balance Walk hop action or not..
		Wall_Top_Result = Balance_Walk_Detect_Balance_Surface_Hit_Result;

		/*Get the location in which the character will be walking on.*/
		
		//Offset the vector down to just above the level of the character's feet.
		const FVector Offset_Vector_Down_To_Characters_Feet{Move_Vector_Down(Character_Direction_Arrow_Location, 170.f)};

		//Offset the vector forward by multiplyng the value placed into the global double variable "Forward_Backward_Movement_Value" by 110.
		//The value placed into said global double variable is calculated by the controller within &UCustom_Movement_Component::Add_Movement_Input. 
		//This will move the vector to the location where the character intends to "Balance_Walk" to.
		const FVector Offset_Vector_Forward_To_Location_To_Balance_Walk_To{Move_Vector_Forward(Offset_Vector_Down_To_Characters_Feet, Character_Direction_Arrow_Rotation, Forward_Backward_Movement_Value * 50.f)};

		int Index_1{};

		for(Index_1; Index_1 <= 7; Index_1++)
		{
			//With each iteration of the loop offset the vector down. This is to make sure that if the surface of which the character is "Balance_Walking" on has an incline or decline the
			//correct location values to increase or decrease the height of the character will be calculated.
			const FVector Offset_Vector_Down_With_Each_Iteration{Move_Vector_Down(Offset_Vector_Forward_To_Location_To_Balance_Walk_To, Index_1 * 7.f)};
			//Offset the vector to the right side of the characters feet with each iteration of this outer for loop.
			const FVector Start_2{Move_Vector_Right(Offset_Vector_Down_With_Each_Iteration, Character_Direction_Arrow_Rotation, 50.f)};
			//Launch the vector to the left to see if there is a surface.
			const FVector End_2{Move_Vector_Left(Start_2, Character_Direction_Arrow_Rotation, 70.f)};

			UKismetSystemLibrary::SphereTraceSingleForObjects(
				this,
				Start_2,
				End_2,
				5.f,
				Balance_Traversal_Actors_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result_Reference,
				false
			);
		
			//Check to see if there is a blocking hit on "Balance_Walk_Detect_Balance_Surface_Hit_Result" and "bStartPenetrating is false". If this is the case move on to
			//the next section of the for loop.
			if(Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result_Reference.bBlockingHit && !Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result_Reference.bStartPenetrating)
			{
				Debug::Print("The_Surace_Has_Begun_Being_Analyzed_For_Balance_Walking", FColor::Green, 78);
			
				Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result_Reference.ImpactNormal);
			
				/*Get the location on the surface of the "Balance_Walk" Actor in which the character should always aim to interpolate its location to (Top Result).*/

				//Offset the vector forward so that it is inside the surface which the character is attempting to "Balance_Walk" on. The vector should be in the middle
				//of the "Balance_Walk" actor when measuring the width.
				const FVector Offset_Vector_Forward_Using_Reversed_Front_Wall_Normal_Z{Move_Vector_Forward(Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result_Reference.ImpactPoint, Reversed_Front_Wall_Normal_Z, 15.f)};

				//Offset vector down so it is inside the walkable surface.
				const FVector Offset_Vector_To_Inside_Walkable_Surface{Move_Vector_Down(Offset_Vector_Forward_Using_Reversed_Front_Wall_Normal_Z, 5.f)};
			
				int Index_2{};

				for(Index_2; Index_2 <= 7; Index_2++)
				{
					//With each iteration of this inner for loop offset the vector up
					const FVector Start_3{Move_Vector_Up(Offset_Vector_To_Inside_Walkable_Surface, Index_2 * 5)};
					//Launch the vector down to get the location on the top surface of the "Balance_Walk" actor.
					const FVector End_3{Move_Vector_Down(Start_3, Index_2 * 10)};

					UKismetSystemLibrary::SphereTraceSingleForObjects(
						this,
						Start_3,
						End_3,
						5.f,
						Balance_Traversal_Actors_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference,
						false
					);

					//Check to see if "bStartPenetrating" is true. If it is then this means that there is a an incline on the surface that thae character is attempting to "balance_Walk"
					//on. If this is the case continue accordingly. 
					if(Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.bStartPenetrating)
					{
						if(Index_2 < 7)
						{
							continue;
						}

						else
						{
							Debug::Print("The_Surace_Is_Too_High_For_Balance_Walking", FColor::Red, 79);
							Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
							return false;
						}
					}

					else if(Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.bBlockingHit && !Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.bStartPenetrating)
					{
						Debug::Print("The_Surace_Is_Not_Too_High_For_Balance_Walking", FColor::Green, 79);
						return true;
					}
				}
			}

			else
			{
				if(Index_1 < 7)
				{
					continue;
				}

				else
				{
					Debug::Print("The_Surace_Hasn't_Been_Retrieved_For_Balance_Walking", FColor::Red, 78);
					Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
					return false;
				}
			}
		}
	}
	
	//The following line of code is to meet the return requirement of this function type.
	return false;
}

bool UCustom_Movement_Component::Parkour_Balance_Walk_Are_There_Obstacles_In_Front_Of_Feet(const FHitResult& Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference)
{
	//The following line of code is to meet the return requirement of this function type.
	return false;
}

bool UCustom_Movement_Component::Parkour_Balance_Walk_Are_There_Obstacles_In_Front_Of_Body(const FHitResult& Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference)
{
	//Check to see if the corresponding FHitResult and pointer are valid. If not return safely.
	if(!Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.bBlockingHit || !Character_Direction_Arrow)
	return false;

	//Get the rotation of the "Character_Direction_Arrow".
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	/*Check to see if there is an obstacle in front of the character.*/

	//Offset the vector up so that it is at the same height as the character's hips.
	const FVector Offset_Vector_To_Hips_Height{Move_Vector_Up(Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.ImpactPoint, 90.f)};
	//Offset the vector to the left so that it is in the same allignment on the character's local Y axis as the left shoulder.
	const FVector Start{Move_Vector_Left(Offset_Vector_To_Hips_Height, Character_Direction_Arrow_Rotation, 15.f)};
	//Offset the vector to the right so that it is in the same allignment on the character's local Y axis as the right shoulder.
	const FVector End{Move_Vector_Right(Start, Character_Direction_Arrow_Rotation, 30.f)};


	FHitResult Balance_Walk_Are_There_Obstacles_In_Front_Of_Body_Hit_Result{};

	UKismetSystemLibrary::CapsuleTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		80,
		Balance_Traversal_Actors_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Balance_Walk_Are_There_Obstacles_In_Front_Of_Body_Hit_Result,
		false
	);

	if(Balance_Walk_Are_There_Obstacles_In_Front_Of_Body_Hit_Result.bBlockingHit || Balance_Walk_Are_There_Obstacles_In_Front_Of_Body_Hit_Result.bStartPenetrating)
	{
		Debug::Print("There_Is_An_Obstacle_In_Front_Of_The_Character", FColor::Red, 80);
		Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		return true;
	}

	else if(!Balance_Walk_Are_There_Obstacles_In_Front_Of_Body_Hit_Result.bBlockingHit && !Balance_Walk_Are_There_Obstacles_In_Front_Of_Body_Hit_Result.bStartPenetrating)
	{
		Debug::Print("There_Is_No_Obstacle_In_Front_Of_The_Character", FColor::Green, 80);
		return false;
	}

	//The following line of code is to meet the return requirement of this function type.
	return false;
}

void UCustom_Movement_Component::Balance_Walk_Hop_Detect_Wall()
{	
	int Index{};

	for(Index; Index <= 20; Index++)
	{
		//Get the location of the character.
		const FVector Component_Location{UpdatedComponent->GetComponentLocation()};
		//Set the vector which the sphere trace will use to be at ground level. This for loop will create new sphere traces from this location. Each sphere trace will stack ontop of the previous forming a "tower".
		const FVector Set_Vector_Below_Character{Move_Vector_Down(Component_Location, 300.f)};
		//With each iteration of the for loop move the vector up 17 units by multiplying the index by 17.
		const FVector Move_Vector_Up_With_Each_Iteration_Of_Loop{Move_Vector_Up(Set_Vector_Below_Character, Index * 20.f)};
		//Move the vector forwards 20 units so that it starts right in front of the character.
		const FVector Start{Move_Vector_Forward(Move_Vector_Up_With_Each_Iteration_Of_Loop, UpdatedComponent->GetComponentRotation(), 90)};
		//Move the vector forward 140 units so that it ends a good distance away from the character.
		const FVector End{Move_Vector_Forward(Start, UpdatedComponent->GetComponentRotation(), 290)};

		//Develop a "SphereTraceSingleForObjects()". The objects will be set in the character blueprint.
		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start,
			End,
			20.f,
			Balance_Traversal_Actors_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Initial_Front_Wall_Hit_Result,
			false
			);

		//If there is a blocking hit and there is no initial overlap break out of the for loop early.
		if(Initial_Front_Wall_Hit_Result.bBlockingHit && !Initial_Front_Wall_Hit_Result.bStartPenetrating)
		break;
	}
	
	//Drawing debug sphere so the "EDrawDebugTrace" can be set to none on the "SphereTraceSingleForObjects()".
	//Draw_Debug_Sphere(Initial_Front_Wall_Hit_Result.ImpactPoint, 5.f, FColor::Blue, 1.f, false, 7.f);
} 

bool UCustom_Movement_Component::Balance_Walk_Hop_Calculate_Wall_Top_Surface()
{
	if(!Character_Direction_Arrow)
	return false;
	
	//The global FRotator "Reversed_Front_Wall_Normal_Z" has to be set here to the directrion of the reversed impact normal at 180 degrees for the gloabal FHitResult "Front_Wall_Top_Edge_Best_Hit". 
	//because within the function &UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Balance_Walk_Position there is a rotation of -90 degrees which is subtracted from the global FRotator "Reversed_Front_Wall_Normal_Z".
	Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Front_Wall_Top_Edge_Best_Hit.ImpactNormal);
	

	const FVector Offset_Vector_Forward{Move_Vector_Forward(
				  Front_Wall_Top_Edge_Best_Hit.ImpactPoint, 
				  Reversed_Front_Wall_Normal_Z,
				  15.f)};
		
	const FVector Start{Move_Vector_Up(Offset_Vector_Forward, 15.f)};
	const FVector End{Move_Vector_Down(Start, 20.f)};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start,
		End,
		5.f,
		Balance_Traversal_Actors_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Balance_Walk_Automatic_Hop_Top_Result,
		false
		);
		
	//If there is no blocing hit return. This is because the top surface of the wall which is being analyzed had dropped below the threshold which is desired (Sphere trace "End").
	if(!Balance_Walk_Automatic_Hop_Top_Result.bBlockingHit)
	{
		Debug::Print("Analyzing_Balance_Walk_Hop_Calculate_Wall_Top_Surface_Failed", FColor::Red, 80);
		return false;
	}
	
	//If there is a blocking hit on the Out_Hit, assign said FHitResult to the global FHitResult variable "Wall_Top_Result". 
	else if(Balance_Walk_Automatic_Hop_Top_Result.bBlockingHit)
	{
		Debug::Print("Analyzing_Balance_Walk_Hop_Calculate_Wall_Top_Surface_Succeeded", FColor::Green, 80);
		//Draw_Debug_Sphere(Wall_Top_Result.ImpactPoint, 15.f, FColor::Emerald, 7.f, false, 7.f);
		return true;
	}

	else
	{
		Debug::Print("Analyzing_Balance_Walk_Hop_Calculate_Wall_Top_Surface_Failed. There_Is_No_Wall_Top_Result", FColor::Red, 80);
		return false;
	}

}

bool UCustom_Movement_Component::Validate_Balance_Walk_Automatic_Hop_Location()
{
	if(!Owning_Player_Character || !Balance_Walk_Automatic_Hop_Top_Result.bBlockingHit)
	{
		Debug::Print("Validate_Balance_Walk_Automatic_Hop_Location_Did_Not_Begin", FColor::Red, 83);
		return false;
	}
	

	const FVector Character_Location{Owning_Player_Character->GetActorLocation()};
	
	/*Check to see if there is an obstacle in the way.*/

	const FVector Custom_Character_Location{Character_Location.X, Character_Location.Y, Character_Location.Z - 40.f};


	int Index_2{};

	FHitResult Out_Hit_1{};

	for(Index_2; Index_2 <= 4; Index_2++)
	{
		const FVector Start_1{Move_Vector_Up(Custom_Character_Location, Index_2 * 40.f)};

		const FVector Offset_End_Vector_Up{Move_Vector_Up(Balance_Walk_Automatic_Hop_Top_Result.ImpactPoint, 10.f)};

		const FVector End_1{Move_Vector_Up(Offset_End_Vector_Up, Index_2 * 40.f)};
			
		UKismetSystemLibrary::LineTraceSingleForObjects(
			this,
			Start_1,
			End_1,
			Balance_Traversal_Actors_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Out_Hit_1,
			false
		);

		if(!Out_Hit_1.bBlockingHit)
		{
			if(Index_2 != 4)
			{
				continue;
			}

			else if(Index_2 == 4)
			{
				Debug::Print("Validate_Balance_Walk_Automatic_Hop_Location_For_No_Obstacles_Succeeded", FColor::Green, 83);

				/*Check for enough room for the character to land.*/

				const FVector Start_2{Move_Vector_Up(Balance_Walk_Automatic_Hop_Top_Result.ImpactPoint, 110.f)};

				FHitResult Out_Hit_2{};

				UKismetSystemLibrary::CapsuleTraceSingleForObjects(
					this,
					Start_2,
					Start_2,
					40.f,
					100.f,
					Balance_Traversal_Actors_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Out_Hit_2,
					false
				);

				if(!Out_Hit_2.bBlockingHit && !Out_Hit_2.bStartPenetrating)
				{
					Debug::Print("Validate_Balance_Walk_Automatic_Hop_Location_For_Enough_Room_For_Character_To_Land_Succeeded", FColor::Green, 84);

					Wall_Top_Result = Out_Hit_1;

					return true;
				}
				
				else
				{
					Debug::Print("Validate_Balance_Walk_Automatic_Hop_Location_For_Enough_Room_For_Character_To_Land_Failed", FColor::Red, 84);
					return false;
				}
			}
		}

		else
		{
			Debug::Print("Validate_Balance_Walk_Automatic_Hop_Location_For_No_Obstacles_Failed", FColor::Red, 83);
			return false;
		}
	}

	//The following line of code is to meet the return requirement of this function type.
	return false;
	
}

bool UCustom_Movement_Component::Validate_Balance_Drop_Hanging(const bool& bDrop_To_Left_Side)
{
	if(!Character_Direction_Arrow)
	{
		Debug::Print("Validate_Balance_Drop_Hanging_Could_Not_Be_Started", FColor::Red, 75);
		return false;
	}

	//Get the location and rotation of the "Character_Direction_Arrow".
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	//Offset the vector just below the height of the character's feet.
	const FVector Offset_Vector_To_The_Level_Of_The_Feet{Move_Vector_Down(Character_Direction_Arrow_Location, 200.f)};
	
	//Whether the value passed in via the input parameter is true or false move the vector to the left or right side of the character.
	FVector Start_1{};
	
	if(bDrop_To_Left_Side)
	{
		Start_1 = Move_Vector_Left(Offset_Vector_To_The_Level_Of_The_Feet, Character_Direction_Arrow_Rotation, 50.f);
	}

	else
	{
		Start_1 = Move_Vector_Right(Offset_Vector_To_The_Level_Of_The_Feet, Character_Direction_Arrow_Rotation, 50.f);
	}

	//Whether the value passed in via the input parameter is true or false move the vector back towards the appropriate side of the character character 
	FVector End_1{};

	if(bDrop_To_Left_Side)
	{
		End_1 = Move_Vector_Right(Offset_Vector_To_The_Level_Of_The_Feet, Character_Direction_Arrow_Rotation, 55.f);
	}

	else
	{
		End_1 = Move_Vector_Left(Offset_Vector_To_The_Level_Of_The_Feet, Character_Direction_Arrow_Rotation, 55.f);
	}

	FHitResult Detect_Balance_Drop_Hanging{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_1,
		End_1,
		5.f,
		Balance_Traversal_Actors_Trace_Types,
		false,
		TArray<AActor*>(),
		Debug_Action,
		Detect_Balance_Drop_Hanging,
		false
	);

	if(Detect_Balance_Drop_Hanging.bBlockingHit && !Detect_Balance_Drop_Hanging.bStartPenetrating)
	{
		Debug::Print("Detect_Balance_Drop_Hanging_Succeeded", FColor::Green, 75);

		const FRotator Reversed_Detect_Balance_Drop_Hanging_Normal{Reverse_Wall_Normal_Rotation_Z(Detect_Balance_Drop_Hanging.ImpactNormal)};

		/*Get the Wall Top Result*/
		const FVector Offset_Vector_Forward{Move_Vector_Forward(Detect_Balance_Drop_Hanging.ImpactPoint, Reversed_Detect_Balance_Drop_Hanging_Normal, 2.f)};
		const FVector Start_2{Move_Vector_Up(Offset_Vector_Forward, 10.f)};
		const FVector End_2{Move_Vector_Down(Start_2, 15.f)};


		FHitResult Balance_Drop_Hanging_Top_Result{};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start_2,
			End_2,
			5.f,
			Balance_Traversal_Actors_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Balance_Drop_Hanging_Top_Result,
			false
		);

		if(Balance_Drop_Hanging_Top_Result.bBlockingHit)
		{
			Debug::Print("Balance_Drop_Hanging_Top_Result_Succeeded", FColor::Green, 77);

			/*Check to see if there is engough room for the character to Execute_Balance_Drop_Hanging*/

			const FVector Offset_Vector_Backward{Move_Vector_Backward(Detect_Balance_Drop_Hanging.ImpactPoint, Reversed_Detect_Balance_Drop_Hanging_Normal, 50.f)};
			const FVector Start_3{Move_Vector_Down(Offset_Vector_Backward, 100.f)};
			const FVector End_3{Start_3};
			
			FHitResult Validate_Balance_Drop_Hanging_For_Space{};

			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this,
				Start_3,
				End_3,
				40.f,
				98.f,
				Balance_Traversal_Actors_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Validate_Balance_Drop_Hanging_For_Space,
				false
			);

			if(!Validate_Balance_Drop_Hanging_For_Space.bBlockingHit && !Validate_Balance_Drop_Hanging_For_Space.bStartPenetrating)
			{
				Debug::Print("Validate_Balance_Drop_Hanging_For_Space_Succeeded", FColor::Green, 78);
				Wall_Top_Result = Balance_Drop_Hanging_Top_Result;
				Reversed_Front_Wall_Normal_Z = Reversed_Detect_Balance_Drop_Hanging_Normal;
				return true;
			}

			else
			{
				Debug::Print("Validate_Balance_Drop_Hanging_For_Space_Failed", FColor::Red, 78);
				return false;
			}
			
		}

		else
		{
			Debug::Print("Balance_Drop_Hanging_Top_Result_Failed", FColor::Red, 77);
			return false;
		}
	}

	else
	{
		Debug::Print("Detect_Balance_Drop_Hanging_Failed", FColor::Red, 75);
		return false;
	}
	
	//The following line of code is to meet the return requirement of this function type.
	return false;
}

bool UCustom_Movement_Component::Validate_Free_Hang_To_Balanced_Walk(const bool& bClimb_To_Left_Side)
{
	if(!Character_Direction_Arrow)
	{
		Debug::Print("Validate_Free_Hang_To_Balanced_Walk_Did_Not_Start", FColor::Red, 77);
		return false;
	}

	//Get the location and rotation of the "Character_Direction_Arrow".
	const FVector Character_Direction_Arrow_Location{Character_Direction_Arrow->GetActorLocation()};
	const FRotator Character_Direction_Arrow_Rotation{Character_Direction_Arrow->GetActorRotation()};

	//Offset Vector Backwards
	const FVector Start_1{Move_Vector_Backward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 40.f)};
	//Offset Vector Forward
	const FVector End_1{Move_Vector_Forward(Start_1, Character_Direction_Arrow_Rotation, 45.f)};

	FHitResult Detect_Free_Hang_To_Balanced_Walk_1{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_1,
		End_1,
		5.f,
		Balance_Traversal_Actors_Trace_Types,
		false,
		TArray<AActor*>(),
		EDrawDebugTrace::ForDuration,
		Detect_Free_Hang_To_Balanced_Walk_1,
		false
	);

	//Offset Vector Forward
	const FVector Start_2{Move_Vector_Forward(Character_Direction_Arrow_Location, Character_Direction_Arrow_Rotation, 70.f)};
	//Offset Vector Backward
	const FVector End_2{Move_Vector_Backward(Start_2, Character_Direction_Arrow_Rotation, 75.f)};

	FHitResult Detect_Free_Hang_To_Balanced_Walk_2{};

	UKismetSystemLibrary::SphereTraceSingleForObjects(
		this,
		Start_2,
		End_2,
		5.f,
		Balance_Traversal_Actors_Trace_Types,
		false,
		TArray<AActor*>(),
		EDrawDebugTrace::ForDuration,
		Detect_Free_Hang_To_Balanced_Walk_2,
		false
	);



	if((Detect_Free_Hang_To_Balanced_Walk_1.bBlockingHit && Detect_Free_Hang_To_Balanced_Walk_2.bBlockingHit) && (!Detect_Free_Hang_To_Balanced_Walk_1.bStartPenetrating && !Detect_Free_Hang_To_Balanced_Walk_2.bStartPenetrating))
	{
		Debug::Print("Detect_Free_Hang_To_Balanced_Walk_Succeeded", FColor::Green, 77);

		const FRotator Free_Hang_To_Balanced_Walk_Reverse_Normal{Reverse_Wall_Normal_Rotation_Z(Detect_Free_Hang_To_Balanced_Walk_1.ImpactNormal)};

		//Get the wall top result.

		const FVector Offset_Vector_Forward{Move_Vector_Forward(Detect_Free_Hang_To_Balanced_Walk_1.ImpactPoint, Free_Hang_To_Balanced_Walk_Reverse_Normal, 10.f)};
		const FVector Start_3{Move_Vector_Up(Offset_Vector_Forward, 10.f)};
		const FVector End_3{Move_Vector_Down(Start_3, 15.f)};


		FHitResult Free_Hang_To_Balanced_Walk_Top_Result{};

		UKismetSystemLibrary::SphereTraceSingleForObjects(
			this,
			Start_3,
			End_3,
			5.f,
			Balance_Traversal_Actors_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Free_Hang_To_Balanced_Walk_Top_Result,
			false
		);

		if(Free_Hang_To_Balanced_Walk_Top_Result.bBlockingHit)
		{
			Debug::Print("Free_Hang_To_Balanced_Walk_Top_Result_Succeeded", FColor::Green, 78);

			/*Check to see if there is engough room for the character to Free_Hang_To_Balanced_Walk*/

			const FVector Start_4{Move_Vector_Up(Free_Hang_To_Balanced_Walk_Top_Result.ImpactPoint, 100.f)};
			const FVector End_4{Start_4};
			
			FHitResult Validate_Free_Hang_To_Balanced_Walk_For_Space{};

			UKismetSystemLibrary::CapsuleTraceSingleForObjects(
				this,
				Start_4,
				End_4,
				40.f,
				98.f,
				Balance_Traversal_Actors_Trace_Types,
				false,
				TArray<AActor*>(),
				Debug_Action,
				Validate_Free_Hang_To_Balanced_Walk_For_Space,
				false
			);

			if(!Validate_Free_Hang_To_Balanced_Walk_For_Space.bBlockingHit && !Validate_Free_Hang_To_Balanced_Walk_For_Space.bStartPenetrating)
			{
				Debug::Print("Validate_Free_Hang_To_Balanced_Walk_For_Space_Succeeded", FColor::Green, 79);
				
				/*Check to see if there is a wall in front of the character*/

				FRotator Rotated_Vector_To_Check_For_Wall{};
				
				if(bClimb_To_Left_Side)
				{
					Rotated_Vector_To_Check_For_Wall = FRotator(0.f, Add_Rotator(Free_Hang_To_Balanced_Walk_Reverse_Normal, 90).Yaw, 0.f);
				}
				
				else
				{
					Rotated_Vector_To_Check_For_Wall = FRotator(0.f, Add_Rotator(Free_Hang_To_Balanced_Walk_Reverse_Normal, -90).Yaw, 0.f);
				}
			
				const FVector Offset_Wall_Top_Vector_Forward_To_Check_For_Wall{Move_Vector_Forward(Free_Hang_To_Balanced_Walk_Top_Result.ImpactPoint, Rotated_Vector_To_Check_For_Wall, 40.f)};

				const FVector Offset_Wall_Top_Vector_Up_To_Check_For_Wall{Move_Vector_Up(Offset_Wall_Top_Vector_Forward_To_Check_For_Wall, 10.f)};
				
				int Index{};

				FHitResult Validate_Free_Hang_To_Balanced_Walk_For_Obstacle{};

				for(Index; Index <= 7; Index++)
				{
					const FVector Start_5{Move_Vector_Up(Offset_Wall_Top_Vector_Up_To_Check_For_Wall, 30.f * Index)};

					const FVector End_5{Move_Vector_Forward(Start_5, Rotated_Vector_To_Check_For_Wall, 140.f)};

					UKismetSystemLibrary::LineTraceSingleForObjects(
						this,
						Start_5,
						End_5,
						Balance_Traversal_Actors_Trace_Types,
						false,
						TArray<AActor*>(),
						Debug_Action,
						Validate_Free_Hang_To_Balanced_Walk_For_Obstacle,
						false
					);

					if(!Validate_Free_Hang_To_Balanced_Walk_For_Obstacle.bBlockingHit)
					{
						if(Index < 7)
						{
							continue;
						}

						else
						{
							Debug::Print("Validate_Free_Hang_To_Balanced_Walk_For_Obstacle_Succeeded", FColor::Green, 80);
							Wall_Top_Result = Free_Hang_To_Balanced_Walk_Top_Result;
							Reversed_Front_Wall_Normal_Z = Free_Hang_To_Balanced_Walk_Reverse_Normal;
							return true;
						}
					}

					else
					{
						Debug::Print("Validate_Free_Hang_To_Balanced_Walk_For_Obstacle_Failed", FColor::Red, 80);
						return false;
					}
				}
			}

			else
			{
				Debug::Print("Validate_Free_Hang_To_Balanced_Walk_For_Space_Failed", FColor::Red, 79);
				return false;
			}
		}

		else
		{
			Debug::Print("Free_Hang_To_Balanced_Walk_Top_Result_Failed", FColor::Red, 78);
			return false;
		}	
	}

	else
	{
		Debug::Print("Detect_Free_Hang_To_Balanced_Walk_Failed", FColor::Red, 77);
		return false;
	}

	//The following line of code is to meet the return requirement of this function type.
	return false;
}

bool UCustom_Movement_Component::Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(const FGameplayTag& Direction_To_Tic_Tac, const bool& bCan_Tic_Tac_Over_Front_Wall)
{
	if(!Tic_Tac_Actor || !Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().bBlockingHit ||!Wall_Top_Result.bBlockingHit)
	{
		return false;
	}
	
	const FVector Offset_Vector_Down{Move_Vector_Down(Wall_Top_Result.ImpactPoint, 50.f)};

	FVector Offset_Vector_To_Left_Or_Right{};

	//Overlapping wall on left side.
	if(Tic_Tac_Actor_Area_Box_ID == 2)
	{
		Offset_Vector_To_Left_Or_Right = Move_Vector_Left(Offset_Vector_Down, Tic_Tac_Actor->GetActorRotation(), 30.f);
	}

	//Overlapping wall on right side.
	else if(Tic_Tac_Actor_Area_Box_ID == 3)
	{
		Offset_Vector_To_Left_Or_Right = Move_Vector_Right(Offset_Vector_Down, Tic_Tac_Actor->GetActorRotation(), 30.f);
	}

	FVector Start{};

	FVector End{};

	int Index_1{};

	FHitResult Validate_Lack_Of_Obstacles_Hit_Result{};

	for(Index_1; Index_1 <= 4; Index_1++)
	{
		Start = Move_Vector_Up(Offset_Vector_To_Left_Or_Right, Index_1 * 35.f);

		//Overlapping wall on left side.
		if(Tic_Tac_Actor_Area_Box_ID == 2)
		{
			if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
			{
				const FVector Offset_Vector_Up{Move_Vector_Up(Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().ImpactPoint, 35.f)};
				End = Move_Vector_Up(Offset_Vector_Up, Index_1 * 35.f);
			}

			else if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
			Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
			{
				const FVector Offset_Vector_Backward{Move_Vector_Backward(Start, Tic_Tac_Actor->GetActorRotation(), 200.f)};
				const FVector Offset_Vector_Left{Move_Vector_Left(Offset_Vector_Backward, Tic_Tac_Actor->GetActorRotation(), 200.f)};
				const FVector Offset_Vector_Up{Move_Vector_Up(Offset_Vector_Left, 70.f)};
				
				End = Move_Vector_Up(Offset_Vector_Up, Index_1 * 35.f);
			}
		}

		//Overlapping wall on right side.
		else if(Tic_Tac_Actor_Area_Box_ID == 3)
		{
			if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
			{
				const FVector Offset_Vector_Up{Move_Vector_Up(Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().ImpactPoint, 35.f)};
				End = Move_Vector_Up(Offset_Vector_Up, Index_1 * 35.f);
			}

			else if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))) || 
			Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
			{
				const FVector Offset_Vector_Backward{Move_Vector_Backward(Start, Tic_Tac_Actor->GetActorRotation(), 200.f)};
				const FVector Offset_Vector_Right{Move_Vector_Right(Offset_Vector_Backward, Tic_Tac_Actor->GetActorRotation(), 200.f)};
				const FVector Offset_Vector_Up{Move_Vector_Up(Offset_Vector_Right, 70.f)};
				
				End = Move_Vector_Up(Offset_Vector_Up, Index_1 * 35.f);
			}
		}

		UKismetSystemLibrary::LineTraceSingleForObjects(
			this,
			Start,
			End,
			Tic_Tac_Actors_Trace_Types,
			false,
			TArray<AActor*>(),
			Debug_Action,
			Validate_Lack_Of_Obstacles_Hit_Result,
			false
		);

		if(!Validate_Lack_Of_Obstacles_Hit_Result.bBlockingHit)
		{
			if(Index_1 == 4)
			{
				Debug::Print("Validate_Tic_Tac_Lack_Of_Obstacles_SUCCEEDED", FColor::Green, 80);

				//Check if there is enough room for charater to land after performing the "Tic_Tac" action.

				FVector Inner_Loop_Start{};
				FVector Inner_Loop_End{};

				if(bCan_Tic_Tac_Over_Front_Wall)
				{
					//Overlapping wall on left side.
					if(Tic_Tac_Actor_Area_Box_ID == 2)
					{
						if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
						{
							const FVector Inner_Offset_Vector_Backward{Move_Vector_Backward(Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().ImpactPoint, Tic_Tac_Actor->GetActorRotation(), 90.f)};
							const FVector Inner_Offset_Vector_Down{Move_Vector_Down(Inner_Offset_Vector_Backward, 55.f)};
							
							Inner_Loop_Start = Inner_Offset_Vector_Down;
							Inner_Loop_End = Inner_Loop_Start;
						}

						else if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
						Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
						{
							const FVector Offset_Vector_Backward{Move_Vector_Backward(Wall_Top_Result.ImpactPoint, Tic_Tac_Actor->GetActorRotation(), 200.f)};
							const FVector Inner_Offset_Vector_Left{Move_Vector_Left(Offset_Vector_Backward, Tic_Tac_Actor->GetActorRotation(), 370.f)};
							const FVector Inner_Offset_Vector_Down{Move_Vector_Down(Inner_Offset_Vector_Left, 80.f)};

							Inner_Loop_Start = Inner_Offset_Vector_Down;
							Inner_Loop_End = Inner_Loop_Start;
						}
					}

					//Overlapping wall on right side.
					if(Tic_Tac_Actor_Area_Box_ID == 3)
					{
						if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
						{
							const FVector Inner_Offset_Vector_Backward{Move_Vector_Backward(Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().ImpactPoint, Tic_Tac_Actor->GetActorRotation(), 90.f)};
							const FVector Inner_Offset_Vector_Down{Move_Vector_Down(Inner_Offset_Vector_Backward, 55.f)};
							
							Inner_Loop_Start = Inner_Offset_Vector_Down;
							Inner_Loop_End = Inner_Loop_Start;
						}

						else if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))) || 
						Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
						{
							const FVector Offset_Vector_Backward{Move_Vector_Backward(Wall_Top_Result.ImpactPoint, Tic_Tac_Actor->GetActorRotation(), 200.f)};
							const FVector Inner_Offset_Vector_Right{Move_Vector_Right(Offset_Vector_Backward, Tic_Tac_Actor->GetActorRotation(), 370.f)};
							const FVector Inner_Offset_Vector_Down{Move_Vector_Down(Inner_Offset_Vector_Right, 80.f)};

							Inner_Loop_Start = Inner_Offset_Vector_Down;
							Inner_Loop_End = Inner_Loop_Start;
						}
					}
				}

				else
				{	
					//Overlapping wall on left side.
					if(Tic_Tac_Actor_Area_Box_ID == 2)
					{
						if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
						{
							const FVector Inner_Offset_Vector_Up{Move_Vector_Up(Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().ImpactPoint, 110.f)};
							
							Inner_Loop_Start = Inner_Offset_Vector_Up;
							Inner_Loop_End = Inner_Loop_Start;
						}

						else if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
						Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
						{
							const FVector Offset_Vector_Backward{Move_Vector_Backward(Wall_Top_Result.ImpactPoint, Tic_Tac_Actor->GetActorRotation(), 200.f)};
							const FVector Inner_Offset_Vector_Left{Move_Vector_Left(Offset_Vector_Backward, Tic_Tac_Actor->GetActorRotation(), 370.f)};
							const FVector Inner_Offset_Vector_Down{Move_Vector_Down(Inner_Offset_Vector_Left, 80.f)};

							Inner_Loop_Start = Inner_Offset_Vector_Down;
							Inner_Loop_End = Inner_Loop_Start;
						}
					}

					//Overlapping wall on right side.
					if(Tic_Tac_Actor_Area_Box_ID == 3)
					{
						if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
						{
							const FVector Inner_Offset_Vector_Up{Move_Vector_Up(Tic_Tac_Actor->Get_Tic_Tac_Front_Wall_Hit_Result().ImpactPoint, 110.f)};
							
							Inner_Loop_Start = Inner_Offset_Vector_Up;
							Inner_Loop_End = Inner_Loop_Start;
						}

						else if(Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))) || 
						Direction_To_Tic_Tac == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
						{
							const FVector Offset_Vector_Backward{Move_Vector_Backward(Wall_Top_Result.ImpactPoint, Tic_Tac_Actor->GetActorRotation(), 200.f)};
							const FVector Inner_Offset_Vector_Right{Move_Vector_Right(Offset_Vector_Backward, Tic_Tac_Actor->GetActorRotation(), 370.f)};
							const FVector Inner_Offset_Vector_Down{Move_Vector_Down(Inner_Offset_Vector_Right, 80.f)};

							Inner_Loop_Start = Inner_Offset_Vector_Down;
							Inner_Loop_End = Inner_Loop_Start;
						}
					}
				}

				FHitResult Validate_Enough_Space_For_Character_Hit_Result{};

				UKismetSystemLibrary::CapsuleTraceSingleForObjects(
					this,
					Inner_Loop_Start,
					Inner_Loop_End,
					48.f,
					100.f,
					Tic_Tac_Actors_Trace_Types,
					false,
					TArray<AActor*>(),
					Debug_Action,
					Validate_Enough_Space_For_Character_Hit_Result,
					false
				);

				if(!Validate_Enough_Space_For_Character_Hit_Result.bStartPenetrating && !Validate_Enough_Space_For_Character_Hit_Result.bBlockingHit)
				{
					
					Debug::Print("Validate_Enough_Space_For_Character_To_Tic_Tac_SUCCEEDED", FColor::Green, 81);
					return true;
				}

				else
				{
					
					Debug::Print("Validate_Enough_Space_For_Character_To_Tic_Tac_FAILED", FColor::Red, 81);
					return false;
				}
			}

			else
			{
				continue;
			}
		}

		else
		{
			Debug::Print("Validate_Tic_Tac_Lack_Of_Obstacles_FAILED", FColor::Red, 80);
			return false;
		}
	}
	
	//The following line of code is to meet the return requirement of this function type.
	return false;
}

#pragma endregion

#pragma region Parkour_Core

void UCustom_Movement_Component::Parkour_State_Settings(const ECollisionEnabled::Type& Collision_Type, const EMovementMode& New_Movement_Mode, const bool& bStop_Movement_Immediately, const bool& bIgnore_Controller_Input)
{
	if(!Capsule_Component || !Character_Movement || !Player_Controller)
	return;

	Capsule_Component->SetCollisionEnabled(Collision_Type);
	Character_Movement->SetMovementMode(New_Movement_Mode);
	
	if(bStop_Movement_Immediately) Character_Movement->StopMovementImmediately();
	if(bIgnore_Controller_Input) Player_Controller->SetIgnoreMoveInput(true);
	if(!bIgnore_Controller_Input) Player_Controller->ResetIgnoreMoveInput();
}

void UCustom_Movement_Component::Set_Parkour_State_Attributes(const FGameplayTag& Current_Parkour_State)
{
	if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	Parkour_State_Settings(ECollisionEnabled::QueryAndPhysics, EMovementMode::MOVE_Walking, false, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Jump"))))
	Parkour_State_Settings(ECollisionEnabled::QueryAndPhysics, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Mantle"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Vault"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Ready.To.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Run"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Pipe.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Balance.Walk"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	Parkour_State_Settings(ECollisionEnabled::QueryOnly, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Tic.Tac"))))
	Parkour_State_Settings(ECollisionEnabled::QueryAndPhysics, EMovementMode::MOVE_Flying, true, true);
		
	Server_Set_Parkour_State_Attributes(Current_Parkour_State);
		
}

void UCustom_Movement_Component::Set_Parkour_State(const FGameplayTag& New_Parkour_State)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_State != New_Parkour_State)
	{
		Parkour_State = New_Parkour_State;
		Parkour_Interface->Execute_Set_Parkour_State(Anim_Instance, Parkour_State);
		Set_Parkour_State_Attributes(Parkour_State);
		Server_Set_Parkour_State(New_Parkour_State);
	}

	else return;
}

void UCustom_Movement_Component::Set_Parkour_Climb_Style(const FGameplayTag& New_Parkour_Climb_Style)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_Climb_Style != New_Parkour_Climb_Style)
	{
		Parkour_Climb_Style = New_Parkour_Climb_Style;
		Parkour_Interface->Execute_Set_Parkour_Climb_Style(Anim_Instance, Parkour_Climb_Style);
		Server_Set_Parkour_Climb_Style(New_Parkour_Climb_Style);
	}
	
	else return;
}

void UCustom_Movement_Component::Set_Parkour_Wall_Run_Side(const FGameplayTag& New_Wall_Run_Side)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_Wall_Run_Side != New_Wall_Run_Side)
	{
		Parkour_Wall_Run_Side = New_Wall_Run_Side;
		Parkour_Interface->Execute_Set_Parkour_Wall_Run_Side(Anim_Instance, Parkour_Wall_Run_Side);
		Server_Set_Parkour_Wall_Run_Side(New_Wall_Run_Side);
	}

	else return;
}

void UCustom_Movement_Component::Set_Parkour_Direction(const FGameplayTag& New_Parkour_Direction)
{
	if(!Parkour_Interface)
	return;

	if(Parkour_Direction != New_Parkour_Direction)
	{
		Parkour_Direction = New_Parkour_Direction;
		Parkour_Interface->Execute_Set_Parkour_Direction(Anim_Instance, Parkour_Direction);
		Server_Set_Parkour_Direction(New_Parkour_Direction);
	}

	else return;
}

void UCustom_Movement_Component::Set_Parkour_Action(const FGameplayTag& New_Parkour_Action)
{
	/*The goal of this function is to use the FGameplayTag input argument passed into it via the function 
	"Decide_Parkour_Action()" to check if the global FGameplaytag variable "Parkour_Action" has the same FGameplaytag 
	value. If said global variable does not have the same FGameplaytag  as what is passed in via the input argument 
	then said FGameplaytag  should be set to equal the value of what is passed in via the input argument.This is followed 
	by setting the "Parkour_Action" in the interface by using the pointer to said interface and calling the generated 
	(at compile) interface function which begins with the prefix "Execute_" "Set_Parkour_Action()". Lastly, there 
	are "if '' and "else if" checks which need to be analyzed to check whether the FGameplaytag global variable which 
	has just been set equals specific Gameplay tags. If the global FGameplaytag == "Parkour.Action.No.Action" then the 
	function "Reset_Parkour_Variables()" should be called. Otherwise other "else if" statements should follow to check 
	whether said global FGameplaytag variable "Parkour_Action"   == any of the other Parkour Action gameplay tags. 
	Whichever tag said global variable equals the function "Play_Parkour_Montage()" should be called, passing in the 
	"UParkour_Action_Data*"which holds the address to the Asset Data object that is stored inside the character Blueprint 
	within the Custom_Movement_Component.*/

	/* if(!Parkour_Interface || !Owning_Player_Character || !CharacterOwner)
	return;

	if(Parkour_Action != New_Parkour_Action)
	Parkour_Action = New_Parkour_Action;

	Parkour_Interface->Execute_Set_Parkour_Action(Anim_Instance, Parkour_Action); */

	if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
	{
		Reset_Parkour_Variables();
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb"))))
	{
		/* Play_Parkour_Montage(Braced_Jump_To_Climb);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.FreeHang"))
	{
		/* Play_Parkour_Montage(Free_Hang_Jump_To_Climb);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Braced.Climb.Falling.Climb"))
	{
		/* // Execute_Random_Montage(Airborne_To_Braced_Climb_Array);
		Play_Parkour_Montage(Braced_Jump_To_Climb_Airborne);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))
	{
		/* Play_Parkour_Montage(Leap_Entry_To_Climb_Hang_Idle); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.FreeHang.Falling.Climb"))
	{
		/* Play_Parkour_Montage(Free_Hang_Jump_To_Climb_Airborne);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb.Hanging.Jump"))))
	{
		/* Play_Parkour_Montage(Fly_Hanging_Jump); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Corner.Move"))
	{
		//Parkour_Shimmy_Handle_Corner_Movement();

		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag("Parkour.Climb.Style.Braced.Climb"))
		{
			if(bOut_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Left"))
				{
					/* Play_Parkour_Montage(Ledge_Corner_Outer_L); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Right"))
				{
					/* Play_Parkour_Montage(Ledge_Corner_Outer_R); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}
			}

			else if(bIn_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Left"))
				{
					/* Play_Parkour_Montage(Ledge_Corner_Inner_L); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Right"))
				{
					/* Play_Parkour_Montage(Ledge_Corner_Inner_R); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}
			}
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag("Parkour.Climb.Style.FreeHang"))
		{
			if(bOut_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Left"))
				{
					/* Play_Parkour_Montage(Hanging_Corner_Outer_L); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Right"))
				{
					/* Play_Parkour_Montage(Hanging_Corner_Outer_R); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}
			}

			else if(bIn_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Left"))
				{
					/* Play_Parkour_Montage(Hanging_Corner_Inner_L); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Right"))
				{
					/* Play_Parkour_Montage(Hanging_Corner_Inner_R); */
					
					int Not_Needed_Here{};
					Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
				}
			}
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Shimmy.180.Shimmy"))))
	{
		//Select a random integer from the specified range. This integer will be used to play the respective random montage via a switch statement.
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		/* Execute_Random_Montage(Braced_And_Ledge_Shimmy_180_Shimmy_Array); */
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Shimmy.180.Shimmy"))))
	{
		//Select a random integer from the specified range. This integer will be used to play the respective random montage via a switch statement.
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		/* Execute_Random_Montage(Hanging_Shimmy_180_Shimmy_Array); */
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Braced.Climb.Climb.Up"))
	{
		/* Execute_Random_Montage(Ledge_Climb_Up_Array);
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		
		//Select a random integer from the specified range. This integer will be used to play the respective random montage via a switch statement.
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 5);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 5);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.FreeHang.Climb.Up"))
	{
		/* Execute_Random_Montage(Hanging_Climb_Up_Array);
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		
		//Select a random integer from the specified range. This integer will be used to play the respective random montage via a switch statement.
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()))
	{
		if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Up"))))
		{
			/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 5);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 5);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play;

			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);

			/* Execute_Random_Montage(Hop_Up_Array); */
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left"))))
		{
			/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play;

			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);

			/*Execute_Random_Montage(Braced_And_Ledge_Hop_Left_Array);*/
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right"))))
		{
			/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play;

			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
			
			/*Execute_Random_Montage(Braced_And_Ledge_Hop_Right_Array);*/
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Up"))))
		{
			/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play;

			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
			
			/*Execute_Random_Montage(Braced_And_Adventure_Hop_Up_Left_Array);*/
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Up"))))
		{
			/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play;

			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
			
			/*Execute_Random_Montage(Braced_And_Adventure_Hop_Up_Right_Array);*/
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Down"))))
		{
			/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play;

			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);

			/*Execute_Random_Montage(Braced_And_Ledge_Hop_Down_Array);*/
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Down"))))
		{
			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
		}
	
		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Down"))))
		{
			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
		}

		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Left"))))
		{
			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
		}
	
		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Right"))))
		{
			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
		}
	
		else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Down"))))
		{
			Server_Perform_Hop_Action(Parkour_State, Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Random_Montage_To_Play);
		}

		/* Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction())); */
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Forward"))))
	{
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		/* //Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Monkey_Vault);
			break;

			case 2:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Reverse_L_Vault);
			break;

			case 3:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Reverse_R_Vault);
			break;

			case 4:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Safety_L_Vault);
			break;

			case 5:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Safety_R_Vault);
			break;

			case 6:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Thief_L_Vault);
			break;

			case 7:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Thief_R_Vault);
			break;

			case 8:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_TwoHand_L_Vault);
			break;

			case 9:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_TwoHand_R_Vault);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Exit_Jump_Forward_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);

		} */
		
		/* Execute_Random_Montage(Exit_Ledge_Jump_Forward_Array);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Backward"))))
	{
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

	/* 	//Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Exit_Ledge_Jump_Backward_L);
			break;

			case 2:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Exit_Ledge_Jump_Backward_R);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Exit_Jump_Forward_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);

		} */
		
		/* Execute_Random_Montage(Exit_Ledge_Jump_Backward_Array);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Exit.Jump"))))
	{
		/* Play_Parkour_Montage(Exit_Hanging_Jump); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Accelerating.Drop"))))
	{
		//Select a random integer from the specified range. This integer will be used to play the respective random montage via a switch statement.
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
/* 
		//Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Accelerating_Drop_Ledge_L);
			break;

			case 2:
			Play_Parkour_Montage(Accelerating_Drop_Ledge_R);
			break;

			case 3:
			Play_Parkour_Montage(Accelerating_Drop_Slide_Ledge_L);
			break;

			case 4:
			Play_Parkour_Montage(Accelerating_Drop_Slide_Ledge_R);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			break;
		} */

		/* Execute_Random_Montage(Drop_Ledge_Array); */
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);

	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Accelerating.Drop"))))
	{
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		/* //Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Accelerating_Drop_Hanging_L);
			break;

			case 2:
			Play_Parkour_Montage(Accelerating_Drop_Hanging_R);
			break;

			default:
			Debug::Print("Parkour_Action_FreeHang_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		} */
		
		/* Execute_Random_Montage(Drop_Hanging_Array); */
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Normal.Drop"))))
	{
		/* Play_Parkour_Montage(Braced_Drop_Down); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Normal.Drop"))))
	{
		/* Play_Parkour_Montage(FreeHang_Drop_Down); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Left"))))
	{
		/* Play_Parkour_Montage(Wall_Run_L_Start); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Right"))))
	{
		/* Play_Parkour_Montage(Wall_Run_R_Start); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.L.Jump.F"))))
	{
		/* Play_Parkour_Montage(Wall_Run_L_Jump_F); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.R.Jump.F"))))
	{
		/* Play_Parkour_Montage(Wall_Run_R_Jump_F); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Left.Jump.90.R"))))
	{
		/* Play_Parkour_Montage(Wall_Run_L_Jump_90_R); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Right.Jump.90.L"))))
	{
		/* Play_Parkour_Montage(Wall_Run_R_Jump_90_L); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.L.Finish"))))
	{
		/* Play_Parkour_Montage(Wall_Run_L_Finish); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.R.Finish"))))
	{
		/* Play_Parkour_Montage(Wall_Run_R_Finish); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Light"))))
	{
		/* Play_Parkour_Montage(Landing_Down_Light); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Impact"))))
	{
		/* Play_Parkour_Montage(Landing_Down_Impact); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Front"))))
	{
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		/* //Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Landing_Front_L);
			break;

			case 2:
			Play_Parkour_Montage(Landing_Front_R);
			break;

			default:
			Debug::Print("Parkour_Landing_Down_Front_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		} */
		
		/* Execute_Random_Montage(Landing_Down_Front_Array); */

		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Roll"))))
	{
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		/* //Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Landing_Roll_A_L);
			break;

			case 2:
			Play_Parkour_Montage(Landing_Roll_A_R);
			break;

			case 3:
			Play_Parkour_Montage(Landing_Roll_B_L);
			break;

			case 4:
			Play_Parkour_Montage(Landing_Roll_B_R);
			break;

			default:
			Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		} */
		
		/* Execute_Random_Montage(Landing_Down_Roll_Array); */
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Free.Roam.Accelerating.Drop")))))
	{
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		/* //Execute a switch statement to play the random montage.
		switch(Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Dash_Drop);
			break;

			case 2:
			Play_Parkour_Montage(Jump_Drop);
			break;

			case 3:
			Play_Parkour_Montage(Monkey_Drop);
			break;

			case 4:
			Play_Parkour_Montage(Reverse_L_Drop);
			break;

			case 5:
			Play_Parkour_Montage(Reverse_R_Drop);
			break;

			case 6:
			Play_Parkour_Montage(Speed_L_Drop);
			break;

			case 7:
			Play_Parkour_Montage(Speed_R_Drop);
			break;

			case 8:
			Play_Parkour_Montage(Two_Hand_L_Drop);
			break;

			case 9:
			Play_Parkour_Montage(Two_Hand_R_Drop);
			break;

			default:
			Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		} */

		
		/* Execute_Random_Montage(Free_Roam_Accelerating_Drop_Array); */
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Grounded")))))
	{
		/* Play_Parkour_Montage(Idle_To_Wall_Pipe_Attach);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Airborne")))))
	{
		/* Play_Parkour_Montage(Jumping_To_Wall_Pipe_Attach);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Fall.Down")))))
	{
		/* Play_Parkour_Montage(Wall_Pipe_Fall_Down);
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Climb.Up.2.Hand")))))
	{
		/* Play_Parkour_Montage(Wall_Pipe_Climb_Up_2_Hand);
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f); */
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()))
	{
		/*Perform_Hop_Action(Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()));*/
		
		int Not_Needed_Here{};
		Server_Perform_Hop_Action(Parkour_State, Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()), Not_Needed_Here);
		
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Up")))))
	{
		/*Play_Parkour_Montage(Jump_Up);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.L")))))
	{
		/*Play_Parkour_Montage(Accurate_Jump_Start_L);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.R")))))
	{
		/*Play_Parkour_Montage(Accurate_Jump_Start_R);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.L.Warp")))))
	{
		/*Play_Parkour_Montage(Accurate_Jump_Start_L_Warp);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.R.Warp")))))
	{
		/*Play_Parkour_Montage(Accurate_Jump_Start_R_Warp);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Finish")))))
	{
		/*Play_Parkour_Montage(Accurate_Jump_Finish);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.L.Start")))))
	{
		/*Play_Parkour_Montage(Jump_Front_L_Start);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.R.Start")))))
	{
		/*Play_Parkour_Montage(Jump_Front_R_Start);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.L.Start.Warp")))))
	{
		/*Play_Parkour_Montage(Jump_Front_L_Start_Warp);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.R.Start.Warp")))))
	{
		/*Play_Parkour_Montage(Jump_Front_R_Start_Warp);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Finish")))))
	{
		/*Play_Parkour_Montage(Jump_Finish);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.One.L")))))
	{
		/*Play_Parkour_Montage(Jump_One_L);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.One.R")))))
	{
		/*Play_Parkour_Montage(Jump_One_R);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.90.L")))))
	{
		/*Play_Parkour_Montage(Balance_Walk_90_L);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.90.R")))))
	{
		/*Play_Parkour_Montage(Balance_Walk_90_R);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.180")))))
	{
		/*Play_Parkour_Montage(Balance_Walk_180);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.Automatic.Hop")))))
	{
		/*Execute_Random_Montage(Balance_Walk_Automatic_Hop_Array);*/
		
		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Drop.L.Hanging")))))
	{
		/*Play_Parkour_Montage(Balance_Drop_L_Hanging);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Drop.R.Hanging")))))
	{
		/*Play_Parkour_Montage(Balance_Drop_R_Hanging);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.L")))))
	{
		/*Play_Parkour_Montage(Hanging_Climb_Up_To_Balanced_Walk_L);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.R")))))
	{
		/*Play_Parkour_Montage(Hanging_Climb_Up_To_Balanced_Walk_R);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Idle.Wall.Vault.On")))))
	{
		/*Play_Parkour_Montage(Idle_Wall_Two_Hand_L_On);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Idle.Wall.Vault.Over")))))
	{
		/*Play_Parkour_Montage(Idle_Wall_Safety_On);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.On")))))
	{
		/*Execute_Random_Montage(Running_Wall_Vault_On);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.Over")))))
	{
		/*Execute_Random_Montage(Running_Wall_Vault_On);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.Under.Bar")))))
	{
		/*Play_Parkour_Montage(Wall_Under_Bar);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.Over.180.Shimmy")))))
	{
		/*Execute_Random_Montage(Running_Wall_Vault_Over_180_Shimmy_Array);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Idle"))))
	{
		/*Execute_Random_Montage(Tic_Tac_L_On_Front_Wall_To_Idle_Array);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Idle"))))
	{
		/*Execute_Random_Montage(Tic_Tac_R_On_Front_Wall_To_Idle_Array);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Run"))))
	{
		/*Play_Parkour_Montage(Tic_Tac_L_Jump_On_Run);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Run"))))
	{
		/*Play_Parkour_Montage(Tic_Tac_R_Jump_On_Run);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Front.Wall"))))
	{
		/*Execute_Random_Montage(Tic_Tac_L_Over_Front_Wall_Array);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Front.Wall"))))
	{
		/*Execute_Random_Montage(Tic_Tac_R_Over_Front_Wall_Array);*/

		Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;
		
		Server_Set_Parkour_Action(New_Parkour_Action, Random_Montage_To_Play);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Right.Wall"))))
	{
		/*Play_Parkour_Montage(Tic_Tac_L_Jump_Side_Over);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Left.Wall"))))
	{
		/*Play_Parkour_Montage(Tic_Tac_R_Jump_Side_Over);*/
		
		int Not_Needed_Here{};
		Server_Set_Parkour_Action(New_Parkour_Action, Not_Needed_Here);
	}

	
}

float UCustom_Movement_Component::Select_Value_Based_On_Climb_Style(const FGameplayTag& Climb_Style, const float& Braced_Value, const float& Free_Hang_Value) const
{
	const float& Parkour_Braced_Value{Braced_Value};
	const float& Parkour_Free_Hang_Value{Free_Hang_Value};

	if((Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb")))))
	{
		return Parkour_Braced_Value;
	}

	else //if(Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
	{
		return Parkour_Free_Hang_Value;
	}

}

 void UCustom_Movement_Component::Measure_Wall()
 {
	//This function is used to calculate the Wall Height, Wall Depth and the Vault Height.
	//The wall height is calculated by getting the delta between the "Wall_Top_Result.Z" and the
	//characters root bone Z location.
	//The wall depth is calculated by getting the delta between the distance of 
	//"Wall_Top_Result" and "Wall_Depth_Result".
	//The vault height is calculated by getting the delta between "Wall_Depth_Result.Z" and
	//"Wall_Vault_Result.Z"

	/*If the first if statement returns false all of the values ("Wall_Height", "Wall_Depth" and "Vault_Height")
	will be set to 0. This is because if these two FHitResults don't return a blocking hit, none of 
	the other FHitResults which are needed to calculate "Wall_Height", "Wall_Depth" and "Vault_Height"
	will be valid. all of the other FHitResults which are needed to calculate "Wall_Height", "Wall_Depth" and "Vault_Height" 
	are dependant on the impact points of the FHitResults being checked in in the first if statement 
	("Initial_Front_Wall_Hit_Result" and "Wall_Top_Result")*/

	//All of the following checks are to ensure that the respective FHitResults which will be used 
	//do indeed have a blocing hit. If there is no blocking hit the respective value that is being calculated, 
	//whether it be "Wall_Height", "Wall_Depth" or "Vault_Height" will be set to 0.
	
	if(Initial_Front_Wall_Hit_Result.bBlockingHit && Wall_Top_Result.bBlockingHit)
	{
		Calculate_Wall_Height();

		Calculate_Wall_Depth();

		Calculate_Vault_Height();

		Set_Network_Wall_Calculations(Calculate_Wall_Height(), Calculate_Wall_Depth(), Calculate_Vault_Height());
	}

	else
	{
		Wall_Height = 0.f;
		Debug::Print(FString("Wall_Height: ") + FString::SanitizeFloat(Wall_Height), FColor::MakeRandomColor(), 1);

		Wall_Depth = 0.f;
		Debug::Print(FString("Wall_Depth: ") + FString::SanitizeFloat(Wall_Depth), FColor::MakeRandomColor(), 1);

		Vault_Height = 0.f;
		Debug::Print(FString("Vault_Height: ") + FString::SanitizeFloat(Vault_Height), FColor::MakeRandomColor(), 1);
	}

 }

void UCustom_Movement_Component::Decide_Parkour_Action()
{
	if(Wall_Top_Result.bBlockingHit)
	{
		if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
		{
			Debug::Print("Shimmy_180_Shimmy_Climb_Or_Hop", FColor::MakeRandomColor(), 7);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);


			Decide_Shimmy_180_Shimmy_Mantle_Or_Hop();	
		}

		else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
		{
			Debug::Print("Wall_Pipe_Mantle_Or_Hop", FColor::MakeRandomColor(), 7);

			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Decide_Wall_Pipe_Maneuver_To_Free_Hang_Mantle_Or_Hop();
		}

		else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
		{
			Debug::Print("Balance_Walk_Hop", FColor::MakeRandomColor(), 7);

			if(Validate_Balance_Walk_Hop())
			{
				//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
				//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
				//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
				const FHitResult Not_Needed_Here{};
				Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
					
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Walk.Automatic.Hop"))));
			}
		}

		else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
		{
			if(bIs_On_Ground)
			{
				if(Wall_Height >= 90 && Wall_Height <= 170)
				{
					if(Wall_Depth >= 0 && Wall_Depth <= 120)
					{
						if(Vault_Height >= 70 && Vault_Height <= 120)
						{
							if(Wall_Depth >= 20 && Wall_Depth <= 30)
							{
								if(UpdatedComponent->GetComponentVelocity().Size() > 20)
								{
									Debug::Print("Parkour_Low_Vault", FColor::MakeRandomColor(), 7);
								}

								else
								{
									Debug::Print("Parkour_Thin_Vault", FColor::MakeRandomColor(), 7);
								}
							}
						
							else if(UpdatedComponent->GetComponentVelocity().Size() > 20)
							{
								Debug::Print("Parkour_Low_Vault", FColor::MakeRandomColor(), 7);
							}

							else
							{
								Debug::Print("Parkour_Mantle", FColor::MakeRandomColor(), 7);
							}

						}

						else if(Vault_Height >= 130 && Vault_Height <= 140)
						{
							if(UpdatedComponent->GetComponentVelocity().Size() > 20)
							{
								Debug::Print("Parkour_High_Vault", FColor::MakeRandomColor(), 7);
							}

							else
							{
								Debug::Print("Parkour_No_Action", FColor::MakeRandomColor(), 7);
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
							}
						
						}

						else
						{
							Debug::Print("Parkour_No_Action", FColor::MakeRandomColor(), 7);
							Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
						}
					}

					else
					{
						Debug::Print("Parkour_Mantle", FColor::MakeRandomColor(), 7);
					}
				}

				else if(Wall_Height >= 190.f && Wall_Height <= 280.f)
				{
					Debug::Print("Parkour_Climb", FColor::MakeRandomColor(), 7);
					Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
					// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
					// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
					// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
					// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
					// a location to begin the next sequence of ray casts.
					Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
					
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					const FHitResult Not_Needed_Here{};
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

					if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
					{
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb"))));
					}
					
					else
					{
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang"))));
					}
				}

				else if(bReady_To_Initialize_Parkour_Wall_Pipe)
				{
					// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
					// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
					// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
					// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
					// a location to begin the next sequence of ray casts.
					Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
					
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);

					Set_Parkour_Action(FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Grounded")))));
					
					bReady_To_Initialize_Parkour_Wall_Pipe = false;
				}

				else
				{
					Debug::Print("No_Action", FColor::MakeRandomColor(), 7);
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
				}
			}

			else
			{
				if(Wall_Height >= 140 && Wall_Height <= 280 && Validate_Can_Start_Shimmying_While_Airborne())
				{								
												// negative value in "Air_Speed" means character is falling  //Ground_Speed >= 700.f || Air_Speed <= -1000.f || Air_Speed <= 20.f
					if(Owning_Player_Character->Get_Is_Jogging() && (Air_Speed >= -200.f || Air_Speed <= 200.f))
					{
						Debug::Print("Parkour_Dynamic_Airorne_Climb", FColor::MakeRandomColor(), 7);

						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						{
							if(Air_Speed <= -500.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))));
							}

							else if(Air_Speed >= -499.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));
							}
						}
						
						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
						{
							if(Validate_Can_Fly_Hanging_Jump())
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb.Hanging.Jump"))));
							}
							
							else
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
							}
						}
					}

					else
					{
						Debug::Print("Parkour_Airorne_Climb", FColor::MakeRandomColor(), 7);
						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));

						else
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
					}

				}

				else if(bReady_To_Initialize_Parkour_Wall_Pipe)
				{
					// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
					// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
					// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
					// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
					// a location to begin the next sequence of ray casts.
					Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
					
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);

					Set_Parkour_Action(FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Airborne")))));
					
					bReady_To_Initialize_Parkour_Wall_Pipe = false;
				}

				else
				{
					Debug::Print("No_Action", FColor::MakeRandomColor(), 7);
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
				}
			}
		}
	}
	
	else
	{
		Debug::Print("Parkour_No_Action", FColor::MakeRandomColor(), 1);
		Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
	}

	//Server_Decide_Parkour_Action();
}

void UCustom_Movement_Component::Reset_Parkour_Variables()
{
	/*This function will be called every "Tick()" within the funtion "Parkour_Call_In_Tick()". The goal of this function is to reset the values stored in 
	the global FHitResults "Initial_Front_Wall_Hit_Result", "Front_Wall_Top_Edge_Best_Hit", "Wall_Top_Result", "Wall_Depth_Result" and 
	"Wall_Vault_Result" as well as the double variables "Wall_Height", "Wall_Depth" and "Vault_Height". The resseting of the values stored in said global variables 
	needs to be completed every tick so that each time "Execute_Parkour_Action()" is called, there will be a new beginning to set the next "Parkour_State" and "Parkour_Action".*/
	Debug::Print("ALL_VARIABLES_ARE_BEING_RESET", FColor::MakeRandomColor(), 15);

	float Reset{1.f};

	Initial_Front_Wall_Hit_Result.Reset(Reset, false);

	Front_Wall_Top_Edge_Best_Hit.Reset(Reset, false);

	Wall_Top_Result.Reset(Reset, false);

	Wall_Depth_Result.Reset(Reset, false);

	Wall_Vault_Result.Reset(Reset, false);

	Front_Wall_Top_Edge_Best_Hop_Hit.Reset(Reset, false);
	
	Hop_Top_Hit_Result.Reset(Reset, false);

	Wall_Height = 0;
	
	Wall_Depth = 0;

	Vault_Height = 0;
}

void UCustom_Movement_Component::Parkour_Call_In_Tick()
{
	/*This function will be called every "Tick()". The goal of this function is to check whether the character is on the ground or not using the function call "Validate_bIs_On_Ground()".
	Depending on the value set on the global bool variable "bIs_On_Ground" within said function another check will be performed to check if the value set to the gameplay tag "Parkour_Action" 
	is equal to "Parkour.Action.No.Action". If this is the case, then the character is on the ground and is not performing any parkour. Therefore a call to reset the values stored in 
	the global FHitResults "Initial_Front_Wall_Hit_Result", "Front_Wall_Top_Edge_Best_Hit", "Wall_Top_Result", "Wall_Depth_Result" and "Wall_Vault_Result" as well as the 
	double variables "Wall_Height", "Wall_Depth" and "Vault_Height". will be made. The resetting of said values will happen within the function call "Reset_Parkour_Variables()".*/

	Validate_bIs_On_Ground();

	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	{
		Stabilize_Movement_While_Free_Roaming();

		Ground_Speed = UKismetMathLibrary::VSizeXY(UpdatedComponent->GetComponentVelocity());
		Air_Speed = UpdatedComponent->GetComponentVelocity().Z;
		
		Debug::Print("Ground_Speed: " + FString::FromInt(Ground_Speed), FColor::Red, 18);
		Debug::Print("Air_Speed: " + FString::FromInt(Air_Speed), FColor::Yellow, 19);
		
		//Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();

		//Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		//Set_Parkour_Wall_Run_Side(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.None"))));

		Execute_Accelerating_Drop_Free_Roam();

	}
	
	if(bIs_On_Ground && Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
	{
		Reset_Parkour_Variables();
		On_Landing_Impact();
	}

	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))) ||
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		Reset_Parkour_Variables();
		Dynamic_IK_Limbs();
	}

	else if(!bIs_Falling && /*Owning_Player_Character->bPressedJump &&*/ 
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	{
		Execute_Parkour_Action();
	}

	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))
	{
		if(Forward_Backward_Movement_Value == 0.f && Right_Left_Movement_Value == 0.f)
		{
			Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
		}
		
		/* if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
		{
			Wall_Run_Detect_Wall(true);
			Parkour_Wall_Run_Handle_Wall_Run_Movement();
		}

		else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
		{
			Wall_Run_Detect_Wall(false);
			Parkour_Wall_Run_Handle_Wall_Run_Movement();
		}

		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) ||
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			
			Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
		} */
	}

}

 /*void UCustom_Movement_Component::Get_Parkour_Data_Asset(UParkour_Action_Data* Parkour_Data_Asset_To_Use)
 {
	//This function uses the global pointer that is passed in via the input argument (which is a UParkour_Action_Data* (child of UPrimaryDataAsset)) and gets the data stored within the 
	//data asset which is located at the address of said pointer (the pointer is a UPROPERTY so the data asset will be developed in the editor then stored within the character Blueprint 
	//class via the "Custom_Movement_Component" component). This happens by using the developed function call "Get_Parkour_Data_Asset_Information()" which located at said pointer's class. By passing in
	//the the global pointer which is passed into this function via the input argument, as the input parameter of the developed function call "Get_Parkour_Data_Asset_Information(), the information
	//stored within the array element of the data asset located at the address of the global pointer which is passed into this function can be obtained (again, the information is set in the editor 
	//via developing a Data Asset that is a child of the class of said pointer (UParkour_Action_Data)).
	
	if(Parkour_Data_Asset == nullptr) return;
	Parkour_Data_Asset->Get_Parkour_Data_Asset_Information(Parkour_Data_Asset_To_Use);
 }*/

void UCustom_Movement_Component::Play_Parkour_Montage(UParkour_Action_Data* Parkour_Data_Asset_To_Use)
{
	/*Safety check to make sure the pointer that is passed in via input argument through the function "Set_Parkour_Action()"
	is not a nullptr. Said pointer holds the address to the Data Asset Object which stores the animation montage to play, the 
	FGameplaytags "In_State" and "Out_State" as well as all the offset values which may need to be used by the MotionWarping 
	Component to offset the location and rotation of the root bone in relation to the impact point of the FHitResult "Wall_Top_Result"
	(this is the FHitResult which will mostly be used to set the location of the Parkour Actions when the character is in "Climb_State").*/
	 
	if(!Parkour_Data_Asset_To_Use) return;

	/*Obtain the FGameplaytag "Parkour_In_State" from within the object of the input argument "UParkour_Action_Data* 
	Parkour_Data_Asset_To_Use" (a Data Asset that is created within the editor which inherits from the developed class 
	"UParkour_Action_Data" (said class Inherits from "UPrimaryDataAsset")), via a getter function and use the value retrieved 
	as an input argument for the function "Set_Parkour_State()" This quickly sets the global FGameplaytag Parkour_State to the
	"In_State" which is set within the object of the input argument (within the editor). */
	Set_Parkour_State(Parkour_Data_Asset_To_Use->Get_Parkour_In_State());
	
	/*Use the "Motion_Warping_Component*" to call the function "AddOrUpdateWarpTargetFromLocationAndRotation()". 
	Said Motion Warping function is called three times because the objects of the input argument "UParkour_Action_Data* 
	Parkour_Data_Asset_To_Use may have up to three sections which may be filled with location values which may need to be used 
	to offset the location of the root bone. If the locations are left blank within said Data Asset objects, then a value of 0 
	will be passed into the input argument of the function "AddOrUpdateWarpTargetFromLocationAndRotation()" and in result the 
	location of root bone won't modified. One of the input parameters for said function is "FVector TargetLocation". For this 
	input argument, a function named "Find_Parkour_Warp_Location()" will be developed to calculate the location to offset the 
	root bone based on the location of the global FHitResult "Wall_Top_Result".*/
	Motion_Warping_Component->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(Parkour_Data_Asset_To_Use->Get_Parkour_Warp_Target_Name_1()),
		Find_Parkour_Warp_Location(
			Wall_Top_Result.ImpactPoint, 
			Reversed_Front_Wall_Normal_Z, 
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_1_X_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_1_Y_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_1_Z_Offset()),
		Reversed_Front_Wall_Normal_Z);

	Motion_Warping_Component->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(Parkour_Data_Asset_To_Use->Get_Parkour_Warp_Target_Name_2()),
		Find_Parkour_Warp_Location(
			Wall_Top_Result.ImpactPoint, 
			Reversed_Front_Wall_Normal_Z, 
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_2_X_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_2_Y_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_2_Z_Offset()),
		Reversed_Front_Wall_Normal_Z);

	Motion_Warping_Component->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(Parkour_Data_Asset_To_Use->Get_Parkour_Warp_Target_Name_3()),
		Find_Parkour_Warp_Location(
			Wall_Top_Result.ImpactPoint, 
			Reversed_Front_Wall_Normal_Z, 
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_3_X_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_3_Y_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_3_Z_Offset()),
		Reversed_Front_Wall_Normal_Z);

	Motion_Warping_Component->AddOrUpdateWarpTargetFromLocationAndRotation(
		FName(Parkour_Data_Asset_To_Use->Get_Parkour_Warp_Target_Name_4()),
		Find_Parkour_Warp_Location(
			Wall_Top_Result.ImpactPoint, 
			Reversed_Front_Wall_Normal_Z, 
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_4_X_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_4_Y_Offset(),
			Parkour_Data_Asset_To_Use->Get_Parkour_Warp_4_Z_Offset()),
		Reversed_Front_Wall_Normal_Z);


	/*After the location to offset the root bone is set, use the "Anim_Instance*" to call the function "Montage_Play(). The input argument 
	"UParkour_Action_Data* Parkour_Data_Asset_To_Use" will be used to call the function "Get_Montage_To_Play()" which is a getter function that returns the 
	Animation Montage that is stored within the Data Asset object of said input argument.*/
	Anim_Instance->Montage_Play(Parkour_Data_Asset_To_Use->Get_Montage_To_Play());

	/*Next, another global pointer of the same type as the input argument "UParkour_Action_Data* Parkour_Data_Asset_To_Use" will be declared 
	"Parkour_Data_Asset", followed by being set to equal same address as the the input argument "UParkour_Action_Data* Parkour_Data_Asset_To_Use". 
	This is because the address of the object that is passed into this function needs to be stored globally so that said address may be passed into 
	the function "Parkour_State()" as an input argument that obtains the FGameplayTag "Out_State" from the Data Asset object which is being used in this 
	function. "Parkour_State()" will be called from within the function "Function_To_Execute_On_Animation_Blending_Out()". "Function_To_Execute_On_Animation_Blending_Out()" 
	will be called when the developed local "FOnMontageEnded Blending_Out_Delegate" is called (when Parkour_Data_Asset_To_Use->Get_Montage_To_Play() is blending out).*/
	Parkour_Data_Asset = Parkour_Data_Asset_To_Use;

	/*Develop a delegate which will call the function "Function_To_Execute_On_Animation_Blending_Out()"" when the animation montage is blending out.
	Within said function the global "FGameplayTag "Parkour_State" will be updated with the FGameplayTag "Out_State" which is set in the object of the 
	input argument to this function "UParkour_Action_Data* Parkour_Data_Asset_To_Use". The address to this object is copied to the global 
	"UParkour_Action_Data* Parkour_Data_Asset". Therefore global "UParkour_Action_Data* Parkour_Data_Asset" is used to obtain the FGameplayTag "Out_State"
	from within the Data Asset Object which is being used in this function.*/
	FOnMontageEnded Blending_Out_Delegate{};
	Blending_Out_Delegate.BindUObject(this, &UCustom_Movement_Component::Function_To_Execute_On_Animation_Blending_Out);
	Anim_Instance->Montage_SetBlendingOutDelegate(Blending_Out_Delegate, Parkour_Data_Asset_To_Use->Get_Montage_To_Play());
	
	/*When this function is done with everything else, set the Parkour Action to "Parkour.Action.No.Action" so that 
	Reset_Parkour_Variables() can be called to reset the values stored in the global FHitResults "Initial_Front_Wall_Hit_Result", 
	"Front_Wall_Top_Edge_Best_Hit", "Wall_Top_Result", "Wall_Depth_Result" and "Wall_Vault_Result" as well as the double variables "Wall_Height", 
	"Wall_Depth" and "Vault_Height".*/
	Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
	
	if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
	Debug::Print("Parkour_Action_Set_To_No_Action", FColor::MakeRandomColor(), 25);
}

FVector UCustom_Movement_Component::Find_Parkour_Warp_Location(const FVector& Impact_Point_To_Use, const FRotator& Direction_For_Character_To_Face, const float& X_Axis_Offset, const float& Y_Axis_Offset, const float& Z_Axis_Offset) const
{
	/*The goal of this function is to take the input parameters "Impact_Point_To_Use", "Direction_For_Character_To_Face", 
	"X_Axis_Offset" and "Z_Axis_Offset" and move the vector forward or backwards using the helper function "Move_Vector_Forward", 
	followed by moving the result of the previous helper function "Move_Vector_Forward" up or down by using the helper function 
	"Move_Vector_Up". The result of the latter will be returned and this will be the location for the Motion Warping Component 
	to place the root bone for the respective Motion Warping anim notify. The input arguments for this function will be filled 
	in with the values from the Data Asset pointer that is passed into the function "Play_Parkour_Montage()".*/

	if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
	{
		const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

		const FVector Warp_Location_First_Edit{Move_Vector_Backward(Impact_Point_To_Use, Direction_Character_Is_Facing, X_Axis_Offset)};
		const FVector Warp_Location_Second_Edit{Move_Vector_Right(Warp_Location_First_Edit, Direction_Character_Is_Facing, Y_Axis_Offset)};
		const FVector Warp_Location_Third_Edit{Move_Vector_Up(Warp_Location_Second_Edit, Z_Axis_Offset)};

		const FVector Destination{Warp_Location_Third_Edit};

		return Destination;
	}

	else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
	{
		const FRotator Direction_Character_Is_Facing{Character_Direction_Arrow->GetActorRotation()};

		const FVector Warp_Location_First_Edit{Move_Vector_Backward(Impact_Point_To_Use, Direction_Character_Is_Facing, X_Axis_Offset)};
		const FVector Warp_Location_Second_Edit{Move_Vector_Right(Warp_Location_First_Edit, Direction_Character_Is_Facing, Y_Axis_Offset)};
		const FVector Warp_Location_Third_Edit{Move_Vector_Up(Warp_Location_Second_Edit, Z_Axis_Offset)};

		const FVector Destination{Warp_Location_Third_Edit};

		return Destination;
	}

	else
	{
		const FVector Warp_Location_First_Edit{Move_Vector_Forward(Impact_Point_To_Use, Direction_For_Character_To_Face, X_Axis_Offset)};
		const FVector Warp_Location_Second_Edit{Move_Vector_Right(Warp_Location_First_Edit, Direction_For_Character_To_Face, Y_Axis_Offset)};
		const FVector Warp_Location_Third_Edit{Move_Vector_Up(Warp_Location_Second_Edit, Z_Axis_Offset)};

		const FVector Destination{Warp_Location_Third_Edit};

		return Destination;
	}

	FVector Null_Vector{};
	return Null_Vector;
}

void UCustom_Movement_Component::Function_To_Execute_On_Animation_Blending_Out(UAnimMontage *Montage, bool bInterrupted)
{
	/*This function will be called from within the function "Play_Parkour_Montage()" to set the global FGameplayTag "Parkour_State" to equal the FGameplayTag "Out_State"
	found within the Data_Asset object which is being used in the function "Play_Parkour_Montage" (Data_Asset object is passed in via the input argument of said function). 
	The "UParkour_Action_Data* Parkour_Data_Asset" is initialized with the address of the Data_Asset which is being used in the function "Play_Parkour_Montage()". Said 
	initialization also happens within the function "Play_Parkour_Montage()" Lastly this function serves as a response to an delegate call back which triggers when the montage 
	in the function "Play_Parkour_Montage()" is blending out.*/ 
	if(Parkour_Data_Asset)
	{
		if(bAccurate_Jump_Destination_Found && !bParkour_Action_Jump_Finish_On_Blending_Out && !Owning_Player_Character->Get_Is_Jogging())
		{
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Accurate.Jump.Finish"))));
			bAccurate_Jump_Destination_Found = false;
			bParkour_Action_Jump_Finish_On_Blending_Out = false;

		}

		else if(bAccurate_Jump_Destination_Found && !bParkour_Action_Jump_Finish_On_Blending_Out && Owning_Player_Character->Get_Is_Jogging())
		{
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Front"))));
			bAccurate_Jump_Destination_Found = false;
			bParkour_Action_Jump_Finish_On_Blending_Out = false;
		}

		/* else if(!bAccurate_Jump_Destination_Found && bParkour_Action_Jump_Finish_On_Blending_Out)
		{
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.Finish"))));
			bAccurate_Jump_Destination_Found = false;
			bParkour_Action_Jump_Finish_On_Blending_Out = false;
		} */
		
		else
		{
			Set_Parkour_State(Parkour_Data_Asset->Get_Parkour_Out_State());
			Debug::Print(TEXT("Animation Blended out and Parkour_State set from Data Asset used."));
			return;
		}
	}
	
	else
	{
		Debug::Print(TEXT("Animation Not Blended out because Parkour_Data_Asset is invalid."));
		return;
	}
}

void UCustom_Movement_Component::Add_Movement_Input(const FVector2D& Scale_Value, const bool& bIs_Forward_Backward_Movement)
{
	//This function is called within the character class in "&ATechnical_Animator_Character::Handle_Ground_Movement_Input_Triggered". It handles the ground locomotion of the 
	//character when the FGameplaytag for the "Parkour_State" is set to "Parkour.State.Free.Roam" as well as the locomotion of the character when the FGameplayTag for the 
	//"Parkour_State" is set to "Parkour.State.Climb".
	
	//This boolean variable is set to true or false within the call to this function within the character blueprint. When it is set to true, the global double variable "Forward_Backward_Movement_Value"
	//is set to be the same value as the the input parameter variable "Scale_Value.Y" when it is set to false the global double variable "Right_Left_Movement_Value" is set to
	//the same value as the input parameter variable "Scale_Value.X". These two variables determine whether the character is moving forward or backward (Forward_Backward_Movement_Value) or left to right (Right_Left_Movement_Value).
	if(bIs_Forward_Backward_Movement)
	Forward_Backward_Movement_Value = Scale_Value.Y;

	else
	Right_Left_Movement_Value = Scale_Value.X;

	Server_Set_Add_Movement_Input_Variables(Forward_Backward_Movement_Value, Right_Left_Movement_Value);
	
	//checking to see the current "Parkour_State" of the character. If the value equals "Parkour.State.Free.Roam" then this means the character should have it's normal ground locomotion.
	//If the "Parkour_State" is set to "Parkour.State.Climb" then a call to handle "Parkour_Climb_Handle_Shimmying_Movement()" should be made if the value set in the global FGameplayTag "Parkour_State"
	//is set to "Parkour.State.Climb" and a further nested check to see if there is an animation montage playing returns false. If there is an animation playing call the funtion 
	//"Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables".
	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	{
		//This variable is set within the character class in one of the calls to this function. One call sets this variable to true the other sets this variable to false. In each respective call to this function the global double variables
		//Forward_Backward_Movement_Value" and "Right_Left_Movement_Value" is set with the correct "Y" and "X" values from the "FInputAction Value"from the character class so the gound locomotion may work as expected. (The "FInputAction Value" is converted to
		//an FVector2D within the character class using "Value.Get<FVector2D>()").
		if(bIs_Forward_Backward_Movement)
		{	
			/*Get the forward vector from the controller by accessing the controller rotation Yaw and getting it's forward vecor. Use said forward vector to "AddMovementInput" to the character using the "Forward_Backward_Movement_Value"*/
			
			//Store the controller rotation in a variable.
			const FRotator& Controller_Rotation{Owning_Player_Character->GetControlRotation()};
			
			//Subtract the Pitch and the Roll from the controller rotation and store the Yaw into a variable.
			const FRotator& Controller_Rotation_Modified{FRotator(Controller_Rotation.Pitch - Controller_Rotation.Pitch, Controller_Rotation.Yaw, Controller_Rotation.Roll - Controller_Rotation.Roll)};
			
			//Store the forward vector of the controller Yaw into a variable.
			const FVector& Forward_Direction{UKismetMathLibrary::GetForwardVector(Controller_Rotation_Modified)};

			//Add movement input to the character using the forward vector of the controller along with the "Forward_Backward_Movement_Value".
			Owning_Player_Character->AddMovementInput(Forward_Direction, Interpolated_Forward_Backward_Movement_Value);
			
			Get_Controller_Direction();
		}

		else
		{
			/*Get the right vector from the controller by accessing the controller rotation Yaw and Roll. Use said right vector to "AddMovementInput" to the character using the "Right_Left_Movement_Value"*/

			//Store the controller rotation in a variable.
			const FRotator& Controller_Rotation{Owning_Player_Character->GetControlRotation()};

			//Subtract the Pitch from the controller rotation and store the Yaw and Roll into a variable.
			const FRotator& Controller_Rotation_Modified{FRotator (Controller_Rotation.Pitch - Controller_Rotation.Pitch, Controller_Rotation.Yaw, Controller_Rotation.Roll)};
			
			//Store the right vector of the controller Yaw and Roll into a variable.
			const FVector& Right_Direction{UKismetMathLibrary::GetRightVector(Controller_Rotation_Modified)};

			//Add movement input to the character using the right vector of the controller along with the "Right_Left_Movement_Value".
			Owning_Player_Character->AddMovementInput(Right_Direction, Interpolated_Right_Left_Movement_Value);

			Get_Controller_Direction();
		}
	}

	//If the Parkour_State is set to "Parkour.State.Climb" check to see if there is an animation playing. If so call "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables()". This call will ensure that the character clamps to the surface
	//of the wall when transitioning the global FGameplayTag "Parkour_State" from "Parkour.State.Free.Roam" to "Parkour.State.Climb" as well as when other montages as played when the global FGameplayTag "Parkour_State" is set to
	//"Parkour.State.Climb". If no animation is playing and the global FGameplayTag "Parkour_Action" is set to "Parkour.Action.No.Action" call the function "Parkour_Shimmy_Handle_Corner_Movement()". This function handles all the logic for the climb movement.
	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		if(Anim_Instance->IsAnyMontagePlaying())
		{
			Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		}
		
		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
		{
			Parkour_Climb_Handle_Shimmying_Movement();
		}
	}

	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))
	{
		if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
		{
			if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
			{
				Wall_Run_Detect_Wall(true);
				Parkour_Wall_Run_Handle_Wall_Run_Movement();
			}

			else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
			{
				Wall_Run_Detect_Wall(false);
				Parkour_Wall_Run_Handle_Wall_Run_Movement();
			}

			if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) ||
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
			{
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			}
		}
	}

	//If the Parkour_State is set to "Parkour.State.Wall.Pipe.Climb" check to see if there is an animation playing. If so call "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables()". This call will ensure that the character clamps to the surface
	//of the wall when transitioning the global FGameplayTag "Parkour_State" from "Parkour.State.Free.Roam" to "Parkour.State.Climb" as well as when other montages as played when the global FGameplayTag "Parkour_State" is set to
	//"Parkour.State.Climb". If no animation is playing and the global FGameplayTag "Parkour_Action" is set to "Parkour.Action.No.Action" call the function "Parkour_Shimmy_Handle_Corner_Movement()". This function handles all the logic for the climb movement.
	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		if(Anim_Instance->IsAnyMontagePlaying())
		{
			Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
		{
			Parkour_Wall_Pipe_Climb_Handle_Pipe_Climbing_Movement();
		}
	}

	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	{
		if(Anim_Instance->IsAnyMontagePlaying())
		{
			Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
		{
			Parkour_Balance_Walk_Handle_Balance_Walking_Movement();
		}
	}
	
}

void UCustom_Movement_Component::Stabilize_Movement_While_Free_Roaming()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))) || 
    Parkour_Action != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")))) 
    return;
	
	const double Delta_Time{UGameplayStatics::GetWorldDeltaSeconds(this)};
	
	Current_Input_Vector = FVector(Right_Left_Movement_Value, Forward_Backward_Movement_Value, 0);

	Current_Input_Rotation = UKismetMathLibrary::MakeRotFromX(Current_Input_Vector);
	
	FVector Interpolated_Direction{};
	
	if(UKismetMathLibrary::Abs(Forward_Backward_Movement_Value) > 0 || UKismetMathLibrary::Abs(Right_Left_Movement_Value) > 0)
    {
        if(Do_Once_1)
        {
            //Reset
			Do_Once_2 = true;
            	
			//On_True
            //Debug::Print("On_True", FColor::Green);
			Target_Input_Rotation = Current_Input_Rotation;
			Target_Input_Rotation = UKismetMathLibrary::RInterpTo_Constant(Target_Input_Rotation, Current_Input_Rotation, Delta_Time, 200.f);
			Interpolated_Direction = UKismetMathLibrary::GetForwardVector(Target_Input_Rotation);
			//UKismetSystemLibrary::DrawDebugCoordinateSystem(this, Owning_Player_Character->GetActorLocation(), Target_Input_Rotation, 150.f, 0.f, 1.f);

            Do_Once_1 = false;
        }

		else
		{
			//While_True
			//Debug::Print("While_True", FColor::Yellow);
			Target_Input_Rotation = UKismetMathLibrary::RInterpTo_Constant(Target_Input_Rotation, Current_Input_Rotation, Delta_Time, 200.f);
			Interpolated_Direction = UKismetMathLibrary::GetForwardVector(Target_Input_Rotation);
			//UKismetSystemLibrary::DrawDebugCoordinateSystem(this, Owning_Player_Character->GetActorLocation(), Target_Input_Rotation, 150.f, 0.f, 1.f);
		}
    }

    else
    {
        if(Do_Once_2)
        {
            //Reset
			Do_Once_1 = true;
            	
            //On_False
            //Debug::Print("On_False", FColor::Red);

            Do_Once_2 = false;
        }

		else
		{
			//While_False
			//Debug::Print("While_False", FColor::Red);
		}
    }

	Interpolated_Forward_Backward_Movement_Value = Interpolated_Direction.Y;

	Interpolated_Right_Left_Movement_Value = Interpolated_Direction.X;
}

void UCustom_Movement_Component::Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables()
{
	//This function is called within the character class in "&ATechnical_Animator_Character::Handle_Ground_Movement_Input_Triggered" as 
	//well as within this class. It resets the values of the global double variables named "Forward_Backward_Movement_Value" and 
	//"Right_Left_Movement_Value" (both set within the "Add_Movement_Input" function). 
	//In this function the Parkour Direction is also set to "Parkour.Direction.None".

	//This function also stops the movement of the character immediately.
	Character_Movement->StopMovementImmediately();
	
	Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))));
}

void UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement()
{
	/*This function handles calling the functions which will validate whether the character can shimmy in the direction of the input which is passed into the gloabal double variable "Right_Left_Movement_Value". 
	within the function "&UCustom_Movement_Component::Add_Movement_Input".*/
	
	//Store the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value". This value will be used to check if the input to move the character to the right or left
	//within the function "&UCustom_Movement_Component::Add_Movement_Input" is above the threshold to accept input.
	const double Forward_Backward_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Forward_Backward_Movement_Value)};
	const double Right_Left_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Right_Left_Movement_Value)};

	//Check to see if the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value" is above the threshold to allow shimmying movement.
	//If the check is passed, check to see if the value is above or below 0. If the value is above 0 the character is moving to the right, if the value is below 0 the character is 
	//moving to the left.
	if(Forward_Backward_Movement_Value_Absolute_Value > .7 || Right_Left_Movement_Value_Absolute_Value > .7)
	{
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value), FColor::MakeRandomColor(), 7);
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))));
			Debug::Print("Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))));
			Debug::Print("Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		//These variables are filled with values within the function "Parkour_Climb_State_Detect_Wall" (they are passsed in as references via the input arguments in said function). 
		//The FHitResult stored within the variables "Parkour_Climbing_Detect_Wall_Hit_Result" and "Parkour_Climbing_Wall_Top_Result" are be used to determine whether there is a wall 
		//in front of the character which the character can shimmy across/ up or down via the functions "Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Hands"  and
		//"Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Body". Said FHitResult variables are passed in as const references via the input arguments (they are filled with data 
		//within the function "Parkour_Climb_State_Detect_Wall").
		FHitResult Parkour_Climbing_Detect_Wall_Hit_Result{};
		FHitResult Parkour_Climbing_Wall_Top_Result{};
		

		//This function executes an algorithm which determines whether there is a wall infront of the character for climb movememnt to happen, as well as determining if the wall
		//in the direction in which the character is moving is too high or low to shimmy across. For example if the character is moving to the left and the surface which the character
		//is shimmying across has a ledge that is too high for the character to reach aka bStartPenetrating is true, (calculated by the maximum number of iterations in the inner for loop), 
		//then "Parkour_Climbing_Wall_Top_Result" will not be calculated and "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" along with "return" will be called 
		//within the function in the appropriate location. Same goes for the "Parkour_Climbing_Detect_Wall_Hit_Result". If there is no blocking hit or "bStartPenetrating" is true 
		//"Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" along with "return" will be called within the function in the appropriate location within the function.
		if(!Parkour_Climb_State_Detect_Wall(Parkour_Climbing_Detect_Wall_Hit_Result, Parkour_Climbing_Wall_Top_Result))
		{
			/*By design choice in corner movemment should only be enabled if the input placed into the controller is that which is consistant with
			the FGameplayTags "Parkour.Direction.Forward.Left" and "Parkour.Direction.Forward.Right". Within &UCustom_Movement_Component::Get_Controller_Direction
			The values stored within the global double variables "Forward_Backward_Movement_Value" and "Right_Left_Movement_Value" are used to determine the direction
			which the character is moving. The result of said calculation is mathced with the appropriate FGameplayTag and said FGameplayTag is returned.*/
			if((Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right")))) && 
			Get_Controller_Direction() != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
			{
				if(Validate_In_Corner_Shimmying())
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Corner.Move"))));
				}
			}

			else
			{
				Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
				return;
			}
		}
		
		//The check Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Hands determines whether there are obstacles on the side of the character's hands which the should stop the character from shimmying any furhther is that direction.
		//The function "Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Hands" uses "Parkour_Climbing_Wall_Top_Result.ImpactPoint" as the starting location (const reference input parameter)
		//of the line traces executed within said function. 

		//The check Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Body determines whether there is a obstacle on the side of the character's body which should deter the character from shimmying any further. The starting location 
		//(const reference input parameter) of the capsule trace executed within the bool function "Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Body" is the impact point of 
		//"Parkour_Climbing_Detect_Wall_Hit_Result".
		else if(Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Hands(Parkour_Climbing_Wall_Top_Result.ImpactPoint) || Parkour_Climb_State_Are_There_Obstacles_On_Sides_Of_Body(Parkour_Climbing_Detect_Wall_Hit_Result.ImpactPoint))
		{
			/*By design choice out corner movemment should only be enabled if the input placed into the controller is that which is consistant with
			the FGameplayTags "Parkour.Direction.Left" and "Parkour.Direction.Right". Within &UCustom_Movement_Component::Get_Controller_Direction
			The values stored within the global double variable "Right_Left_Movement_Value" is used to determine the direction
			which the character is moving. The result of said calculation is mathced with the appropriate FGameplayTag and said FGameplayTag is returned.
			In result "bOut_Corner_Movement" will execute within "&UCustom_Movement_Component::Parkour_Shimmy_Handle_Corner_Movement" automatically if 
			the check "Validate_Out_Corner_Shimmying" returns true*/
			if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
			Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
			{
				if(Validate_Out_Corner_Shimmying())
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Corner.Move"))));
				}
			}

			else
			{
				Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
				return;
			}
		}

		//If this line of code is reached then character has room to move and in result "Calculate_And_Move_Character_To_New_Climb_Position" should be called passing in the FHitResults
		//"Parkour_Climbing_Detect_Wall_Hit_Result" and "Parkour_Climbing_Wall_Top_Result" into the input argument. This function uses the location of said FHitResults to interpolate the
		//character to their location. Considering the locations of said FHitResults are always right infornt of the arrow (offset to the right or left side of the arrow actor depending
		//on whether the character is moving to the right or left side) the character will always be "chasing"	the location to interpolate its location to causeing an infinite interpolation
		//to the location of the FHitResults and in return "Shimmying_Movement" as long as ther is input into the controller.
		else
		{
			Calculate_And_Move_Character_To_New_Climb_Position(Parkour_Climbing_Detect_Wall_Hit_Result, Parkour_Climbing_Wall_Top_Result);
		}
	}

	//If "Right_Left_Movement_Value_Absolute_Value" is not above .7 then this means the minimum threshold to activate "Shimmying_Movement" has not been met by the input from the player's
	//controller. In this case, "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return" should be called.
	else
	{
		Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		return;
	}

}

void UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Climb_Position(const FHitResult& Parkour_Climbing_Detect_Wall_Hit_Result, const FHitResult& Parkour_Climbing_Wall_Top_Result)
{
	/*This function is called calculates the location to interpolate the character to and passes in the said location into the function "&UCustom_Movement_Component::Move_Character_To_New_Climb_Position_Interpolation_Settings"
	as an input argument, well as calling the function which decides the Climb_Style which the character should be in depending on the surface of the wall in which the character is shimmying. */
	
	//Offset value to be used to offset the character backwards from the wall. This is because the impact points found within the input parameter "Parkour_Climbing_Detect_Wall_Hit_Result" is right on
	//the surface of the wall. Therefore the character needs to be moved back so that the animation playing will look realistic and natural. 
	const float Offset_Character_Backwards_From_Wall_Value{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 40.f, 7.f)};
	const FVector Offset_Vector_Backwards_From_Wall{Move_Vector_Backward(Parkour_Climbing_Detect_Wall_Hit_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, Offset_Character_Backwards_From_Wall_Value)};


	//According to the "Parkour_Climb_Style" this is the value to offset the character in the "Z" axis from the location of the "Parkour_Climbing_Wall_Top_Result.ImpactPoint.Z". 
	const float Pick_Climb_Style_Value_Character_Height{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 55.f, 103.f)};
	
	
	/*These values are used to make a custom FVector variable ("Move_Character_To_This_Location").*/

	//Value to use on the "X" axis of the custom FVector "Move_Character_To_This_Location".	
	const double Set_Character_To_This_Position_Perpendicular_From_Wall{Offset_Vector_Backwards_From_Wall.X};			
	//Value to use on the "Y" axis of the custom FVector "Move_Character_To_This_Location".	
	const double Set_Character_To_This_Position_Parallel_From_Wall{Offset_Vector_Backwards_From_Wall.Y};
	//Value to use on the "Z" axis of the custom FVector "Move_Character_To_This_Location".	
	const double Set_Character_To_This_Height_Position{Parkour_Climbing_Wall_Top_Result.ImpactPoint.Z - Pick_Climb_Style_Value_Character_Height};


	//Custom FVector to pass into the function "&UCustom_Movement_Component::Move_Character_To_New_Climb_Position_Interpolation_Settings" as an input argument. This will be the location to interpolate 
	//the character to as long as there is input into the player controller and the validation and checks performed in the function "&UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement"
	//are successful.. 
	const FVector Move_Character_To_This_Location(FVector(Set_Character_To_This_Position_Perpendicular_From_Wall, 
														   Set_Character_To_This_Position_Parallel_From_Wall, 
														   Set_Character_To_This_Height_Position));

	//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
	//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
	//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
	Decide_Climb_Style(Parkour_Climbing_Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

	//This function uses the location which is calculated above (using the variable from the FHitResult input argument "Parkour_Climbing_Detect_Wall_Hit_Result") to interpolate the character to said 
	//FVector. Considering the locations of the custom FVector will always be updating due to it being dependant on the input argument variable "Parkour_Climbing_Detect_Wall_Hit_Result",  
	//the character will always be "chasing" the location to interpolate its location to causeing an infinite interpolation. This is because the impact point of the input argument 
	//"Parkour_Climbing_Detect_Wall_Hit_Result" is offset to the right or left side of the arrow actor (the arrow actor is just above the character) depending on whether the character is moving
	//to the right or left.
	Move_Character_To_New_Climb_Position_Interpolation_Settings(Move_Character_To_This_Location, Reversed_Front_Wall_Normal_Z);

	//const FVector Offset_Decide_Climb_Style_Impact_Point{Move_Vector_Right(Parkour_Climbing_Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, Right_Left_Movement_Value * -10.f)};
	//Decide_Climb_Style(Offset_Decide_Climb_Style_Impact_Point, Reversed_Front_Wall_Normal_Z);

}

void UCustom_Movement_Component::Move_Character_To_New_Climb_Position_Interpolation_Settings(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	if(!Owning_Player_Character)
	return;

	/*Using the location from the input argument "Location_To_Move_Character" which is is calculated in the function "&UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Climb_Position"
	(via said functions const reference FHitResult input argument "Parkour_Climbing_Detect_Wall_Hit_Result"), this function handles interpolating the character to said location. 
	Considering the location of the input parameter FVector "Location_To_Move_Character" will always be updating due to it being dependant on the input argument variable (of the function 
	"&UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Climb_Position") "Parkour_Climbing_Detect_Wall_Hit_Result", the character will always be "chasing" the location to interpolate 
	its location to causeing an infinite interpolation. This is because the impact point of the input argument (of the function "&UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Climb_Position") 
	"Parkour_Climbing_Detect_Wall_Hit_Result" is offset to the right or left side of the arrow actor (the arrow actor is just above the character) depending on whether the character is moving
	to the right or left.*/
	
	//Depending on the "Parkour_Climb_Style" the interpolation speed for the "X" and "Y" axis (speed which the character moves when shimmying forwards/backwards or from the left to right) will be selected.
	const float Pick_Climb_Style_Value_Interpolation_Speed_For_X_And_Y_Axis{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 5.f, 3.5f)};
	
	//Depending on the "Parkour_Climb_Style" the interpolation speed for the "Z" axis (speed which the character moves when shimmying up or down a ledge) will be selected.
	float Pick_Interpolation_Speed_For_Z_Axis{};

	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		Pick_Interpolation_Speed_For_Z_Axis = Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 2.f, 7.f);
	}
	
	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		Pick_Interpolation_Speed_For_Z_Axis = 7.f;
	}

	//Get the UpdatedComponent's current location
	const FVector Updated_Component_Location{UpdatedComponent->GetComponentLocation()};
	
	//"DeltaTime" is needed to fulfill the requirements of the input argument for the funtion "UKismetMathLibrary::FInterpTo". By using DeltaTime there will be a smooth interpolation from the previous location to the new location.
	const double DeltaTime{UGameplayStatics::GetWorldDeltaSeconds(this)};

	/*These variables hold the data of the interpolation from the "UpdatedComponent's" current location to the location which the character needs to move to. The reason why the data is divided into seperate axes is because for 
	each axis, different interpolation values are set based on the "Parkour_Climb_Style".*/
	const double X_Interpolation{UKismetMathLibrary::FInterpTo(Updated_Component_Location.X, Location_To_Move_Character.X, DeltaTime, Pick_Climb_Style_Value_Interpolation_Speed_For_X_And_Y_Axis)};
	const double Y_Interpolation{UKismetMathLibrary::FInterpTo(Updated_Component_Location.Y, Location_To_Move_Character.Y, DeltaTime, Pick_Climb_Style_Value_Interpolation_Speed_For_X_And_Y_Axis)};
	const double Z_Interpolation{UKismetMathLibrary::FInterpTo(Updated_Component_Location.Z, Location_To_Move_Character.Z, DeltaTime, Pick_Interpolation_Speed_For_Z_Axis)};

	//This variable holds the interpolated data of where the caracter needs to move to.
	const FVector Interpolated_Location_To_Move_Character{X_Interpolation, Y_Interpolation, Z_Interpolation};

	//Call "SetActorLocationAndRotation" and pass in the variable which holds the interpolated data where the caracter needs to move to (Interpolated_Location_To_Move_Character).
	Owning_Player_Character->SetActorLocationAndRotation(Interpolated_Location_To_Move_Character, Rotation_For_Character_To_Face);
	
	if(!Owning_Player_Character->HasAuthority())
	{
		Server_Move_Character_To_New_Climb_Position_Interpolation_Settings(Interpolated_Location_To_Move_Character, Rotation_For_Character_To_Face);
	}
}

void UCustom_Movement_Component::Dynamic_IK_Limbs()
{
	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		Parkour_Climb_Dynamic_IK_Hands(true);
		Parkour_Climb_Dynamic_IK_Hands(false);
		Parkour_Climb_Dynamic_IK_Feet(true);
		Parkour_Climb_Dynamic_IK_Feet(false);
	}

	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands(true);
		Parkour_Wall_Pipe_Climb_Dynamic_IK_Hands(false);
		Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet(true);
		Parkour_Wall_Pipe_Climb_Dynamic_IK_Feet(false);
	}
}

void UCustom_Movement_Component::Reset_Parkour_IK_Hands(bool bIs_Left_Hand)
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	return;

	FVector IK_Hand_Location{};
	FRotator IK_Hand_Rotation{};

	if(bIs_Left_Hand == true)
	{
		IK_Hand_Location = Mesh->GetSocketLocation(FName(TEXT("ik_hand_l")));
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Location(Anim_Instance, IK_Hand_Location);

		IK_Hand_Rotation = Mesh->GetSocketRotation(FName(TEXT("ik_hand_l")));
		Parkour_Interface->Execute_Set_Left_Hand_Shimmy_Rotation(Anim_Instance, IK_Hand_Rotation);
	}

	else
	{
		IK_Hand_Location = Mesh->GetSocketLocation(FName(TEXT("ik_hand_r")));
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Location(Anim_Instance, IK_Hand_Location);

		IK_Hand_Rotation = Mesh->GetSocketRotation(FName(TEXT("ik_hand_r")));
		Parkour_Interface->Execute_Set_Right_Hand_Shimmy_Rotation(Anim_Instance, IK_Hand_Rotation);
	}

}

void UCustom_Movement_Component::Reset_Parkour_IK_Feet(bool bIs_Left_Foot)
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	return;

	FVector IK_Foot_Location{};
	FRotator IK_Foot_Rotation{};

	if(bIs_Left_Foot == true)
	{
		IK_Foot_Location = Mesh->GetSocketLocation(FName(TEXT("ik_foot_l")));
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Location(Anim_Instance, IK_Foot_Location);

		IK_Foot_Rotation = Mesh->GetSocketRotation(FName(TEXT("ik_foot_l")));
		Parkour_Interface->Execute_Set_Left_Foot_Shimmy_Rotation(Anim_Instance, IK_Foot_Rotation);
	}

	else
	{
		IK_Foot_Location = Mesh->GetSocketLocation(FName(TEXT("ik_foot_r")));
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Location(Anim_Instance, IK_Foot_Location);

		IK_Foot_Rotation = Mesh->GetSocketRotation(FName(TEXT("ik_foot_r")));
		Parkour_Interface->Execute_Set_Right_Foot_Shimmy_Rotation(Anim_Instance, IK_Foot_Rotation);
	}

}

void UCustom_Movement_Component::Release_From_Shimmying()
{
	/*This function is called within the character class &ATechnical_Animator_Character::On_Parkour_Ended.*/
	
	//This function should only be called when the character is shimmying. Therefore check to see if the character's "Parkour_State" is set to FGameplayTag "Parkour.State.Climb".
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))) || !Owning_Player_Character)
	return;

	//This function will handle releasing the character from shimmying on the current surface. If this is the case "bIs_On_Ground" should be false because when shimmying the character should be above the ground
	//and in result the box trace which is constantly being executed at the location of the root bone should not return a hit result (with no hit result the global bool variable "bIs_On_Ground" is set to false). 
	if(bIs_On_Ground)
	return;

	//"&UCustom_Movement_Component::Handle_Release_From_Shimmying" will handle which animation plays when the call to release the character from the current "Climb_State" is called from the character class 
	//(in "&ATechnical_Animator_Character::On_Parkour_Ended")
	//Handle_Release_From_Shimmying();
	
	const double Forward_Backward_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Forward_Backward_Movement_Value)};
	Server_Handle_Release_From_Shimmying(Parkour_Climb_Style, Get_Controller_Direction(),Forward_Backward_Movement_Value_Absolute_Value);

	//Set the capsule component back to normal size. During shimmying the size may change according to the "Parkour_Climb_Style"
	Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);

	//Set the global bool variable "bIs_Falling" to true. When this variable is true, no other Parkour action will be able to be executed.
	bIs_Falling = true;

	Server_Set_bIs_Falling_To_True();

	Debug::Print("bIs_Falling Set To True");

	//Set a timer to call the function "&UCustom_Movement_Component::Set_bIs_Falling_To_False" which will set the global variable "bIs_Falling" back to false so that Parkour may be re-enabled.
	Owning_Player_Character->GetWorldTimerManager().SetTimer(
		Set_bIs_Falling_To_False_Timer_Handle,
		this,
		&UCustom_Movement_Component::Set_bIs_Falling_To_False,
		Set_bIs_Falling_To_False_Timer_Duration
	);
}

void UCustom_Movement_Component::Handle_Release_From_Shimmying()
{
	/*This function is called within &UCustom_Movement_Component::Release_From_Shimmying and it handles what happens when the
	character wants to be stop shimmying. Depending on the input placed into the character controller via the global double variables
	"Forward_Backward_Movement_Value" and "Right_Left_Movement_Value" and the current "Parkour_Climb_Style" set on the character, a call to execute
	the appropriate montage (which is stored within the object of the global pointer type UParkour_Action_Data* in the form of a Data Asset) will 
	be made via &UCustom_Movement_Component::Play_Parkour_Montage*/

	//Set the "Parkour_State" to "Parkour.State.Free.Roam". When this happens the collisions on the character's capsule component are enabled and the character's movement mode is set to "EMovementMode::Walking". Also,
	//"Character_Movement->StopMovementImmediately" is set to false (this is set to true when the character is shimmying).
	Debug::Print(TEXT("Exited_Parkour"), FColor::MakeRandomColor(), 8);

	//Store the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value". This value will be used to check if the input to move the character to the right or left
	//within the function "&UCustom_Movement_Component::Add_Movement_Input" is above the threshold to accept input.
	const double Forward_Backward_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Forward_Backward_Movement_Value)};
	
	//Check to see if the character's "Climb_Style" is currently set to "Parkour.Climb.Style.Braced.Climb" and there is current input in the player controller to move the character either forwards or backwards. 
	//If so check to see if there is input fromt the player controller to either move the character to the left or the right. Depending on what value is stored in the global double variable "Right_Left_Movement_Value"
	//from the player controller the animation to dismantle the character from "FGameplayTag "Parkour.Climb.Style.Braced.Climb" will play. There are only rotation animations for "Parkour.Climb.Style.Braced.Climb".
	if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))) && Forward_Backward_Movement_Value_Absolute_Value > 0.f)
	{
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			Play_Parkour_Montage(Ledge_Fall_Down_180_R);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
		{
			Play_Parkour_Montage(Ledge_Fall_Down_180_L);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
		
	}

	//If the current "Climb_State" is currently not "Parkour.Climb.Style.Braced.Climb" && there is no input from the player controller the animation will play to release the character from the current "Climb_Style".
	//Said animation will leave the character facing the same direction as when shimmying was enabled. 
	else
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Play_Parkour_Montage(Ledge_Fall_Down);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
		

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Play_Parkour_Montage(Hanging_Drop);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
	}

}

void UCustom_Movement_Component::Set_bIs_Falling_To_False()
{
	/*This function is called within &UCustom_Movement_Component::Release_From_Shimmying.*/
	
	//When the the global bool variable "bIs_Falling" is set to true within &UCustom_Movement_Component::Release_From_Shimmying a cool down timer
	//is started. Until this cool down timer is complete the character will not be able to call the function &UCustom_Movement_Component::Execute_Parkour_Action.
	bIs_Falling = false;

	Server_Set_bIs_Falling_To_False();

	Debug::Print("bIs_Falling Set To False");
}

void UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy()
{
	/*This function is called within the character class &ATechnical_Animator_Character::Jump.*/

	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	return;

	if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
	{
		bool bJump_Forward{};
		
		if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) ||
		Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
		Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
		{
			if(bCan_Jump_From_Braced_Climb)
			{
				bJump_Forward = true;
				if(Validate_Can_Jump_From_Braced_Climb(bJump_Forward))
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Forward"))));
					bCan_Jump_From_Braced_Climb = false;

					if(Owning_Player_Character)
					{
						Owning_Player_Character->GetWorldTimerManager().SetTimer(
							Set_bCan_Jump_From_Braced_Climb_Timer_Handle,
							this,
							&UCustom_Movement_Component::Set_bCan_Jump_From_Braced_Climb_To_True,
							Set_bCan_Jump_From_Braced_Climb_Timer_Duration
						);
					}
				}
			}
		}

		else if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) ||
		Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
		Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
		{
			if(bCan_Jump_From_Braced_Climb)
			{
				bJump_Forward = false;
				if(Validate_Can_Jump_From_Braced_Climb(bJump_Forward))
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Backward"))));
					bCan_Jump_From_Braced_Climb = false;

					if(Owning_Player_Character)
					{
						Owning_Player_Character->GetWorldTimerManager().SetTimer(
							Set_bCan_Jump_From_Braced_Climb_Timer_Handle,
							this,
							&UCustom_Movement_Component::Set_bCan_Jump_From_Braced_Climb_To_True,
							Set_bCan_Jump_From_Braced_Climb_Timer_Duration
						);
					}
				}
			}
		}
	}

	else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
	{
		if(bCan_Jump_From_Free_Hang)
		{
			if(Validate_Can_Jump_From_Free_Hang())
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Exit.Jump"))));
				bCan_Jump_From_Free_Hang = false;

				if(Owning_Player_Character)
				{
					Owning_Player_Character->GetWorldTimerManager().SetTimer(
						Set_bCan_Jump_From_Free_Hang_Timer_Handle,
						this,
						&UCustom_Movement_Component::Set_bCan_Jump_From_Free_Hang_To_True,
						Set_bCan_Jump_From_Free_Hang_Timer_Duration
					);
				}
			}
		}	
	}
}

void UCustom_Movement_Component::Set_bCan_Jump_From_Braced_Climb_To_True()
{
	/*This function is called within &UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy.*/
	
	//When the the global bool variable "bCan_Jump_From_Braced_Climb" is set to false within &UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy a cool down timer
	//is started. Until this cool down timer is complete the character will not be able to call the function &UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy while
	//the FGameplayTag set into the global FGameplayTag variable "Parkour_Climb_Style" is set to "Parkour.Climb.Style.Braced.Climb".
	
	bCan_Jump_From_Braced_Climb = true;
	Debug::Print("bCan_Jump_From_Braced_Climb Set To True");
}

void UCustom_Movement_Component::Set_bCan_Jump_From_Free_Hang_To_True()
{
	/*This function is called within &UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy.*/
	
	//When the the global bool variable "bCan_Jump_From_Free_Hang" is set to false within &UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy a cool down timer
	//is started. Until this cool down timer is complete the character will not be able to call the function &UCustom_Movement_Component::Execute_Jump_Out_Of_Shimmy while
	//the FGameplayTag set into the global FGameplayTag variable "Parkour_Climb_Style" is set to "Parkour.Climb.Style.FreeHang".
	
	bCan_Jump_From_Free_Hang = true;
	Debug::Print("Can_Jump_From_Free_Hang Set To True");
}

bool UCustom_Movement_Component::Validate_Can_Start_Shimmying_While_Airborne() const
{
	/*This function is called wihtin &UCustom_Movement_Component::Decide_Parkour_Action. It determines whether the character can start shimmying  when airborne.
	This is done by getting the location of the global variable "Wall_Top_Result.ImpactPoint" and the  "head" socket location followed by calculating the delta between the two locations. If the 
	character's head is high enough above the gloabal variable "Wall_Top_Result.Impactpoint" "true" is returned otherwise "false" is returned.*/

	//Check to see if the global variable "Wall_Top_Result" has a blocking hit. If not return out of this function. The blocking hit stored in this variable is used
	//to get the global height of the surface which the character will start shimmying if the condition is met.
	if(!Wall_Top_Result.bBlockingHit)
	return false;

	const double Wall_Top_Result_Global_Height{Wall_Top_Result.ImpactPoint.Z};

	//Get the global height of the character's head. This value will be used to get the delta between itself and the height stored in the global variable 
	//"Wall_Top_Result.Z".
	double Characters_Head_Global_Height{};

	if(Mesh)
	Characters_Head_Global_Height = Mesh->GetSocketLocation(FName(TEXT("head"))).Z;

	//Get the delta value between the global height of the character's head socket and the global height of the surface which is being analyzed for shimmying.
	const double Delta_Between_Wall_Top_Result_Global_Height_And_Characters_Head_Global_Height{Wall_Top_Result_Global_Height - Characters_Head_Global_Height};

	//Check to see if the delta between the two locations (Characters_Head_Global_Height && Wall_Top_Result_Global_Height) is greater than 30. If this is true 
	//The character is high enough to start shimmying while airborne. Return true in this case. If this value is not met return false.
	const double Select_Threshold_Value_Based_On_Climb_Style{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 30.f, 35.f)};
	
	if(Delta_Between_Wall_Top_Result_Global_Height_And_Characters_Head_Global_Height <= Select_Threshold_Value_Based_On_Climb_Style)
	{
		Debug::Print("Delta_Between_Wall_Top_Result_Global_Height_And_Characters_Head_Global_Height: " + FString::FromInt(Delta_Between_Wall_Top_Result_Global_Height_And_Characters_Head_Global_Height), FColor::Green, 10);
		return true;
	}
	
	else
	return false;

	//The following line of code is used to for extra precaution.
	return false;
}

FGameplayTag UCustom_Movement_Component::Get_Controller_Direction() const
{
	/*This is similar to the helper functions. In this function the value stored in the two double variables
	"Forward_Backward_Movement_Value" and "Right_Left_Movement_Value" are analyzed. Depending on the result 
	an FGameplayTag which represents the directional input of the controller is returned*/

	if(Forward_Backward_Movement_Value > 0.f && Right_Left_Movement_Value == 0.f)
	{
		Debug::Print("Parkour.Direction.Forward", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward")));
	}

	else if(Forward_Backward_Movement_Value < 0.f && Right_Left_Movement_Value == 0.f)
	{
		Debug::Print("Parkour.Direction.Backward", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward")));
	}

	else if(Forward_Backward_Movement_Value == 0.f && Right_Left_Movement_Value < 0.f)
	{
		Debug::Print("Parkour_Direction_Left", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left")));
	}

	else if(Forward_Backward_Movement_Value == 0.f && Right_Left_Movement_Value > 0.f)
	{
		Debug::Print("Parkour_Direction_Right", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right")));
	}

	else if(Forward_Backward_Movement_Value > 0.f && Right_Left_Movement_Value < 0.f)
	{
		Debug::Print("Parkour.Direction.Forward.Left", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left")));
	}

	else if(Forward_Backward_Movement_Value > 0.f && Right_Left_Movement_Value > 0.f)
	{
		Debug::Print("Parkour.Direction.Forward.Right", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right")));
	}

	else if(Forward_Backward_Movement_Value < 0.f && Right_Left_Movement_Value < 0.f)
	{
		Debug::Print("Parkour.Direction.Backward.Left", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left")));
	}

	else if(Forward_Backward_Movement_Value < 0.f && Right_Left_Movement_Value > 0.f)
	{
		Debug::Print("Parkour.Direction.Backward.Right", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right")));
	}

	else
	{
		Debug::Print("Parkour_Direction_None", FColor::Purple, 1);
		return FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None")));
	}

}

void UCustom_Movement_Component::Parkour_Shimmy_Handle_Corner_Movement()
{
	if(bOut_Corner_Movement)
	{
		Parkour_Shimmy_Corner_Movement(Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result, Parkour_Shimmying_Out_Corner_Wall_Top_Result);
	}

	else if(bIn_Corner_Movement)
	{
		Parkour_Shimmy_Corner_Movement(Parkour_Shimmying_Detect_In_Corner_Wall_Hit_Result, Parkour_Shimmying_In_Corner_Wall_Top_Result);
	}

}

void UCustom_Movement_Component::Parkour_Shimmy_Corner_Movement(const FHitResult& New_Corner_Detect_Wall_Hit_Result, const FHitResult& New_Corner_Wall_Top_Result)
{
	//Safety check to see whether the input argument FHitResult "New_Corner_Detect_Wall_Hit_Result" and "New_Corner_Wall_Top_Result" have a blocking hit.
	if(!New_Corner_Detect_Wall_Hit_Result.bBlockingHit && !New_Corner_Wall_Top_Result.bBlockingHit)
	return;

	//Set a timer to call the function "&UCustom_Movement_Component::Set_bOut_Corner_Movement_To_False" or "&UCustom_Movement_Component::Set_bOut_Corner_Movement_To_False" 
	//which will set the global bool variable "bOut_Corner_Movement" or "bIn_Corner_Movement" back to false so that corner movement may be disabled in "&UCustom_Movement_Component::Parkour_Shimmy_Handle_Corner_Movement" 
	//until the next check approves the next corner movement.
	if(Owning_Player_Character)
	{
		if(bOut_Corner_Movement)
		{
			Owning_Player_Character->GetWorldTimerManager().SetTimer(
			Set_bOut_Corner_Movement_To_False_Timer_Handle,
			this,
			&UCustom_Movement_Component::Set_bOut_Corner_Movement_To_False,
			Set_bOut_Corner_Movement_To_False_Timer_Duration
			);
		}

		else if(bIn_Corner_Movement)
		{
			Owning_Player_Character->GetWorldTimerManager().SetTimer(
			Set_bIn_Corner_Movement_To_False_Timer_Handle,
			this,
			&UCustom_Movement_Component::Set_bIn_Corner_Movement_To_False,
			Set_bIn_Corner_Movement_To_False_Timer_Duration
			);
		}

	}

	
	//Offset value to be used to offset the character backwards from the wall. This is because the impact points found within the input parameter "Parkour_Climbing_Detect_Wall_Hit_Result" is right on
	//the surface of the wall. Therefore the character needs to be moved back so that the animation playing will look realistic and natural. 
	const float& Offset_Character_Backwards_From_Wall_Value{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 44, 20)};
	const FVector Offset_Vector_Backwards_From_Wall{Move_Vector_Backward(New_Corner_Detect_Wall_Hit_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, Offset_Character_Backwards_From_Wall_Value)};

	//According to the "Parkour_Climb_Style" this is the value to offset the character in the "Z" axis from the location of the "Parkour_Climbing_Wall_Top_Result.ImpactPoint.Z". 
	const float& Pick_Climb_Style_Value_Character_Height{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 55, 110)};
	
	/*These values are used to make a custom FVector variable ("Move_Character_To_This_Location").*/

	//Value to use on the "X" axis of the custom FVector "Move_Character_To_This_Location".	
	const double& Set_Character_To_This_Position_Perpendicular_From_Wall{Offset_Vector_Backwards_From_Wall.X};			
	//Value to use on the "Y" axis of the custom FVector "Move_Character_To_This_Location".	
	const double& Set_Character_To_This_Position_Parallel_From_Wall{Offset_Vector_Backwards_From_Wall.Y};
	//Value to use on the "Z" axis of the custom FVector "Move_Character_To_This_Location".	
	const double& Set_Character_To_This_Height_Position{New_Corner_Wall_Top_Result.ImpactPoint.Z - Pick_Climb_Style_Value_Character_Height};


	//Custom FVector to pass into the function "&UCustom_Movement_Component::Move_Character_To_New_Climb_Position_Interpolation_Settings" as an input argument. This will be the location to interpolate 
	//the character to as long as there is input into the player controller and the validation and checks performed in the function "&UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement"
	//are successful.. 
	const FVector& Move_Character_To_This_Location(FVector(Set_Character_To_This_Position_Perpendicular_From_Wall, 
														   Set_Character_To_This_Position_Parallel_From_Wall, 
														   Set_Character_To_This_Height_Position));

	// float Value_To_Offset_X{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 44, -20)};
	// float Value_To_Offset_Y{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 44, 10)};
	// float Value_To_Offset_Z{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, 55, 110)};

	//Use the FHitResults "New_Corner_Wall_Top_Result" and the "New_Corner_Detect_Wall_Hit_Result" to get the location to translate the character to durint corner movement. /*Remember to use local space for consistent results*/
	//const FVector Location_To_Translate_Character_To_During_Out_Corner_Movement{FVector(New_Corner_Detect_Wall_Hit_Result.ImpactPoint.X - Value_To_Offset_X, Parkour_Shimmying_Detect_Out_Corner_Wall_Hit_Result.ImpactPoint.Y + Value_To_Offset_Y, Wall_Top_Result.ImpactPoint.Z - Value_To_Offset_Z)};

	//Call the function "Set_Parkour_Direction". This function will set the FGameplayTag "Parkour_Direction" which is decalred within this UCustom_Movement_Component and within the UCharacter_Animation_Instance via the interface to the direction 
	//which is being input into the controller. Said direction is calculated within the function &UCustom_Movement_Component::Get_Controller_Direction by analyzing the value stored within the global double variables "Forward_Backward_Movement_Value" and 
	//"Right_Left_Movement_Value". The values stored within these two variables are calculated within the function & UCustom_Movement_Component::Add_Movement_Input which is called from the character class within the function 
	//&ATechnical_Animator_Character::Handle_Ground_Movement_Input_Triggered when input is being placed into the controller. 
	// Set_Parkour_Direction(Get_Controller_Direction());

	// FHitResult* Hit = (FHitResult *)0;
	// EMoveComponentFlags MoveFlags = MOVECOMP_NoFlags;
	// ETeleportType Teleport = ETeleportType::ResetPhysics;

	// //Call the function "MoveComponentTo" to set the new rotation and location of the character.
	// UpdatedComponent->MoveComponent(Location_To_Translate_Character_To_During_Out_Corner_Movement - Location_To_Translate_Character_To_During_Out_Corner_Movement,
	// Reversed_Front_Wall_Normal_Z,
	// true,
	// Hit,
	// MoveFlags,
	// Teleport
	// );

	//Depending on the "Parkour_Climb_Style" the interpolation speed will be selected.
	const float& Pick_Climb_Style_Value_Interpolation_Speed{Select_Value_Based_On_Climb_Style(Parkour_Climb_Style, .5, .5)};
	
	Corner_Movement_Latent_Action_Info.CallbackTarget = this;

	UKismetSystemLibrary::MoveComponentTo(UpdatedComponent,
						  Move_Character_To_This_Location,
						  Reversed_Front_Wall_Normal_Z,
						  false,
						  false,
						  Pick_Climb_Style_Value_Interpolation_Speed,
						  false,
						  EMoveComponentAction::Move,
						  Corner_Movement_Latent_Action_Info
						  );

	Decide_Climb_Style(New_Corner_Detect_Wall_Hit_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

	//Call the function "Set_Parkour_Direction" and pass in the FGameplayTag "Parkour.Direction.None". When this is called the animation instance will revieve this new direction to set into it's FGameplayTag "Parkour_Direction". This needs to happen so that
	//the animation state can switch from "Out_Coner_Movement" back to "Idle" from Idle. Depending on whether there is directional input being placed into the controller the animation state will change from Idle to the corresponding shimmy direction. 
	// Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))));

	//Call the function "Set_Parkour_Action" and pass in the FGameplayTag "Parkour.Action.No.Action". This needs to happen because in &UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement the FGameplayTag "Parkour_Action" is set to "Parkour.Action.Corner.Move" and
	//in result &UCustom_Movement_Component::Parkour_Shimmy_Handle_Corner_Movement is called which then calls this function if the global bool variable "bOut_Corner_Movement" is set to true (within the algorithm found in &UCustom_Movement_Component::Validate_Out_Corner_Shimmying).
	//So setting the FGameplayTah "Parkour_Action" back to "Parkour.Action.No.Action" will disable the call to this function from it's source as well as reset the variable back to its default value.
	//Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
}

void UCustom_Movement_Component::Set_bOut_Corner_Movement_To_False()
{
	/*This function is called within &UCustom_Movement_Component::Release_From_Shimmying.*/
	
	//When the the global bool variable "bOut_Corner_Movement" is set to true within &UCustom_Movement_Component::Validate_Out_Corner_Shimmying, Out Corner Shimmying is enabled.
	//Once the Out Corner shimmy movement is complete the global bool variable "bOut_Corner_Movement" is set back to false once the timer is complete (to disable out corner shimmy movement).
	
	bOut_Corner_Movement = false;
	Debug::Print("bOut_Corner_Movement Set To False");
}

void UCustom_Movement_Component::Set_bIn_Corner_Movement_To_False()
{
	/*This function is called within &UCustom_Movement_Component::Release_From_Shimmying.*/
	
	//When the the global bool variable "bIn_Corner_Movement" is set to true within &UCustom_Movement_Component::Validate_In_Corner_Shimmying, In Corner Shimmying is enabled.
	//Once the In Corner shimmy movement is complete the global bool variable "bIn_Corner_Movement" is set back to false once the timer is complete (to disable In corner shimmy movement).
	
	bIn_Corner_Movement = false;
	Debug::Print("bIn_Corner_Movement Set To False");
}

void UCustom_Movement_Component::Decide_Shimmy_180_Shimmy_Mantle_Or_Hop()
{
	//This funtion should only be perfomred if the player is "Climbing".
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))) && Parkour_Action != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
	return;
	
	//Check to see if the input placed by the controller is equal to "Forward". If this is the case then the character has the option to either mantle up on the surface which
	//which is being shimmied (if there is room) or to perform a hop action (if there is no room to perform a mantle and a hop destination is confirmed).
	if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
	{
		if(Validate_Shimmy_180_Shimmy())
		{
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
			// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
			// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
			// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
			// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
			// a location to begin the next sequence of ray casts.
			Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;

			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

			if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Shimmy.180.Shimmy"))));
			}

			else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Shimmy.180.Shimmy"))));
			}
		}

		else if(Validate_Can_Mantle())
		{
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
			// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
			// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
			// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
			// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
			// a location to begin the next sequence of ray casts.
			Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;

			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

			if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Climb.Up"))));
			}

			else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Climb.Up"))));
			}
		}
	}

	else if(Realize_And_Validate_Hop_Destnation_And_Action())
	{
		Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
		
		//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
		//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
		//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
		const FHitResult Not_Needed_Here{};
		Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
		
		Set_Parkour_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()));
	}

}

void UCustom_Movement_Component::Select_Random_Montage_To_Execute(TArray<UParkour_Action_Data*>& Array_To_Select_From)
{
	//Get a random element from the array that is passed in via the input argument.
	int Random_Element_From_Array_To_Select_From{UKismetMathLibrary::RandomIntegerInRange(0, Array_To_Select_From.Num() - 1)};

	//Make sure the random Index which was selected is a valid element in the respecive array. If it isn't, return.
	if(!Array_To_Select_From.IsValidIndex(Random_Element_From_Array_To_Select_From))
	return;

	//Store the "UParkour_Action_Data*" which is in the element of the array which is selected from the line of code above into a local "UParkour_Action_Data*" variable. This variable will be used 
	//to play the "Climbing Up" montage from the respective climb style that the character is in.
	UParkour_Action_Data* Random_Data_Asset_To_Use{Array_To_Select_From[Random_Element_From_Array_To_Select_From]};

	//Check to see if the local variable UParkour_Action_Data* Random_Data_Asset_To_Use is valid. If it isn't valid return safely.
	if(!Random_Data_Asset_To_Use)
	return;

	//Call "&UCustom_Movement_Component::Play_Parkour_Montage" passing in "UParkour_Action_Data* Random_Data_Asset_To_Use".
	Play_Parkour_Montage(Random_Data_Asset_To_Use);

	//After using the random Data Asset object stored within the element of the array that is passed in via the input argument remove said "UParkour_Action_Data*" from the array so the object is 
	//not used again within the respective cycle (the object will be added back once the array is empty). Also, check if the Index which is about to be used to access the "UParkour_Action_Data*" 
	//that is to be removed is indeed a valid Index. If it is not return.
	if(Array_To_Select_From.IsValidIndex(Random_Element_From_Array_To_Select_From))
	Array_To_Select_From.RemoveAt(Random_Element_From_Array_To_Select_From);

	else
	return;
}

void UCustom_Movement_Component::Execute_Random_Montage(TArray<UParkour_Action_Data*>& Array_To_Select_From)
{
	//Check to see if the "TArray<UParkour_Action_Data*>& Array_To_Select_From" that is passed in via the input argument 
	//is empty (when an element of the array is used, said element is removed to prevent the same action from happening 
	//twice in a cycle). If it is indeed empty, emplace all the UParkour_Action_Data* which were removed back into said array 
	//then return.
	if(Array_To_Select_From.IsEmpty())
	{
		if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Shimmy.180.Shimmy"))))
		{
			Array_To_Select_From.Emplace(Climb_Shimmy_To_Shimmy_180_Vault);
			Array_To_Select_From.Emplace(Ledge_Turn_L_Vault);
			Array_To_Select_From.Emplace(Ledge_Turn_R_Vault);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Shimmy.180.Shimmy"))))
		{
			Array_To_Select_From.Emplace(Hanging_180_L);
			Array_To_Select_From.Emplace(Hanging_180_R);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Climb.Up"))))
		{
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Reverse); 
			Array_To_Select_From.Emplace(Ledge_Climb_Up_TwoHand_L); 
			Array_To_Select_From.Emplace(Ledge_Climb_Up_TwoHand_R);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Monkey);
			Array_To_Select_From.Emplace(Climb_Up_The_Ledge);

			Select_Random_Montage_To_Execute(Array_To_Select_From);
			
			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Climb.Up"))))
		{
			Array_To_Select_From.Emplace(Hanging_Climb_Up);
			Array_To_Select_From.Emplace(Free_Hang_Climb_Up);

			Select_Random_Montage_To_Execute(Array_To_Select_From);
			
			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Up"))))
		{
			Array_To_Select_From.Emplace(Braced_Hang_Hop_Up);
			Array_To_Select_From.Emplace(Ledge_Jump_Up_Power);								  
			Array_To_Select_From.Emplace(Ledge_Jump_Up); 	
			Array_To_Select_From.Emplace(Climb_Shimmy_Long_L_Up);
			Array_To_Select_From.Emplace(Climb_Shimmy_Long_R_Up);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left"))))
		{
			Array_To_Select_From.Emplace(Braced_Hang_Hop_Left);
			Array_To_Select_From.Emplace(Ledge_Jump_L_Short);
			Array_To_Select_From.Emplace(Ledge_Jump_L);
			Array_To_Select_From.Emplace(Climb_Shimmy_Long_L_Left);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right"))))
		{
			Array_To_Select_From.Emplace(Braced_Hang_Hop_Right);
			Array_To_Select_From.Emplace(Ledge_Jump_R_Short);
			Array_To_Select_From.Emplace(Ledge_Jump_R); 
			Array_To_Select_From.Emplace(Climb_Shimmy_Long_R_Right);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Up"))))
		{
			//Array_To_Select_From.Emplace(Braced_Hang_Hop_Left_Up);
			Array_To_Select_From.Emplace(Climb_Shimmy_Long_L_Up_Left);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Up"))))
		{
			//Array_To_Select_From.Emplace(Braced_Hang_Hop_Right_Up);
			Array_To_Select_From.Emplace(Climb_Shimmy_Long_R_Up_Right);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Down"))))
		{
			Array_To_Select_From.Emplace(Braced_Hang_Hop_Down);
			Array_To_Select_From.Emplace(Ledge_Jump_Down);
			Array_To_Select_From.Emplace(Climb_Leap_Down_To_Ledge);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Forward"))))
		{
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Monkey_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Reverse_L_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Reverse_R_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Safety_L_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Safety_R_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Thief_L_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_Thief_R_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_TwoHand_L_Vault);
			Array_To_Select_From.Emplace(Ledge_Climb_Up_TwoHand_R_Vault);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}
		
		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Backward"))))
		{
			Array_To_Select_From.Emplace(Exit_Ledge_Jump_Backward_L);
			Array_To_Select_From.Emplace(Exit_Ledge_Jump_Backward_R);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Accelerating.Drop"))))
		{
			Array_To_Select_From.Emplace(Accelerating_Drop_Ledge_L);
			Array_To_Select_From.Emplace(Accelerating_Drop_Ledge_R);
			Array_To_Select_From.Emplace(Accelerating_Drop_Slide_Ledge_L);
			Array_To_Select_From.Emplace(Accelerating_Drop_Slide_Ledge_R);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Accelerating.Drop"))))
		{
			Array_To_Select_From.Emplace(Accelerating_Drop_Hanging_L);
			Array_To_Select_From.Emplace(Accelerating_Drop_Hanging_R);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Front"))))
		{
			Array_To_Select_From.Emplace(Landing_Front_L);
			Array_To_Select_From.Emplace(Landing_Front_R);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Roll"))))
		{
			Array_To_Select_From.Emplace(Landing_Roll_A_L);
			Array_To_Select_From.Emplace(Landing_Roll_A_R);
			Array_To_Select_From.Emplace(Landing_Roll_B_L);
			Array_To_Select_From.Emplace(Landing_Roll_B_R);

			Select_Random_Montage_To_Execute(Array_To_Select_From);

			return;
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Free.Roam.Accelerating.Drop"))))
		{
			Free_Roam_Accelerating_Drop_Array.Emplace(Dash_Drop);
			//Free_Roam_Accelerating_Drop_Array.Emplace(Jump_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Monkey_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Reverse_L_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Reverse_R_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Speed_L_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Speed_R_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Two_Hand_L_Drop);
			Free_Roam_Accelerating_Drop_Array.Emplace(Two_Hand_R_Drop);
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Walk.Automatic.Hop"))))
		{
			Balance_Walk_Automatic_Hop_Array.Emplace(Balance_Walk_Jump_Front);
			Balance_Walk_Automatic_Hop_Array.Emplace(Jump_One_L);
			Balance_Walk_Automatic_Hop_Array.Emplace(Jump_One_R);
			
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.On"))))
		{
			Running_Wall_Vault_On_Array.Emplace(Wall_Monkey_On);
			Running_Wall_Vault_On_Array.Emplace(Wall_Two_Hand_L_On);
			Running_Wall_Vault_On_Array.Emplace(Wall_Two_Hand_R_On);
			
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.Over"))))
		{
			Running_Wall_Vault_Over_Array.Emplace(Wall_Monkey_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Reverse_L_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Reverse_R_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Safety_L_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Safety_R_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Thief_L_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Thief_R_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Two_Hand_L_Vault);
			Running_Wall_Vault_Over_Array.Emplace(Wall_Two_Hand_R_Vault);
			
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.Over.180.Shimmy"))))
		{
			Running_Wall_Vault_Over_180_Shimmy_Array.Emplace(Wall_L_Turn);												   
			Running_Wall_Vault_Over_180_Shimmy_Array.Emplace(Wall_R_Turn);
			
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Idle"))))
		{
			Tic_Tac_L_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_L_Jump_On);
			Tic_Tac_L_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_L_Vault_On);
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Idle"))))
		{
			Tic_Tac_R_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_R_Jump_On);
			Tic_Tac_R_On_Front_Wall_To_Idle_Array.Emplace(Tic_Tac_R_Vault_On);
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Front.Wall"))))
		{
			Tic_Tac_L_Over_Front_Wall_Array.Emplace(Tic_Tac_L_Jump_Over);
			Tic_Tac_L_Over_Front_Wall_Array.Emplace(Tic_Tac_L_Reverse_Over);
			Tic_Tac_L_Over_Front_Wall_Array.Emplace(Tic_Tac_L_Speed_Over);
		}

		else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Front.Wall"))))
		{
			Tic_Tac_R_Over_Front_Wall_Array.Emplace(Tic_Tac_R_Jump_Over);
			Tic_Tac_R_Over_Front_Wall_Array.Emplace(Tic_Tac_R_Reverse_Over);
			Tic_Tac_R_Over_Front_Wall_Array.Emplace(Tic_Tac_R_Speed_Over);
		}

	}

	else
	{
		Select_Random_Montage_To_Execute(Array_To_Select_From);
	}
	
}

FGameplayTag UCustom_Movement_Component::Get_Hop_Action_Based_On_Parkour_Direction(const FGameplayTag& Current_Parkour_Direction) const
{
	/*This is similar to the helper functions. In this function the corresponding FGameplayTag that stores the hop direction that is aimed in the same
	direction as the directional input passed into to the controller (via &UCustom_Movement_Component::Get_Controller_Direction followed by setting the global
	FGameplayTag "Parkour_Direction" within &UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement) is used to return the hop appropriate hop action 
	via the local FGameplayTag "Parkour_Hop_Direction"*/

	FGameplayTag Parkour_Hop_Action{};

	if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
	{
		Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Up")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Down")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Down")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Left")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Right")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Up")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Up")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Down")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
		}
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Down")));
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Parkour_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
		}
	}

	return Parkour_Hop_Action;
}

void UCustom_Movement_Component::Set_Horizontal_Hop_Distance_Value_Based_On_Parkour_Direction(const FGameplayTag& Current_Parkour_Direction)
{
	if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
	{
		Horizontal_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
	{
		Horizontal_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
	{
		Horizontal_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
	{
		Horizontal_Hop_Distance = -250.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
	{
		Horizontal_Hop_Distance = 250.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
	{
		Horizontal_Hop_Distance = -125.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
	{
		Horizontal_Hop_Distance = 125.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
	{
		Horizontal_Hop_Distance = -150.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		Horizontal_Hop_Distance = 150.f;
	}
}

void UCustom_Movement_Component::Set_Vertical_Hop_Distance_Value_Based_On_Parkour_Direction(const FGameplayTag& Current_Parkour_Direction)
{
	if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
	{
		Vertical_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
	{
		Vertical_Hop_Distance = 120.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
	{
		Vertical_Hop_Distance = -210.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
	{
		Vertical_Hop_Distance = -30.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
	{
		Vertical_Hop_Distance = -30.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
	{
		Vertical_Hop_Distance = 120.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
	{
		Vertical_Hop_Distance = 105.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
	{
		Vertical_Hop_Distance = -210.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		Vertical_Hop_Distance = -210.f;
	}

}

float UCustom_Movement_Component::Select_Value_Based_On_Parkour_State(const FGameplayTag& Current_Parkour_State, const float& Parkour_State_Free_Roam_Value_To_Return, 
																	  const float& Parkour_State_Ready_To_Climb_Value_To_Return, 
																	  const float& Parkour_State_Climb_Value_To_Return) const
{
	const float Null_Value{};
	
	if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	return Parkour_State_Free_Roam_Value_To_Return;

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Ready.To.Climb"))))
	return Parkour_State_Ready_To_Climb_Value_To_Return;

	else //if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	return Parkour_State_Climb_Value_To_Return;
	
	// else
	// return Null_Value;
}

double UCustom_Movement_Component::Get_Characters_Highest_Hand_Height() const
{
	FVector Left_Hand_Height{Mesh->GetSocketLocation(FName(TEXT("hand_l")))};

	FVector Right_Hand_Height{Mesh->GetSocketLocation(FName(TEXT("hand_r")))};

	if(Left_Hand_Height.Z >= Right_Hand_Height.Z)
	{
		Debug::Print(FString::FromInt(Left_Hand_Height.Z), FColor::MakeRandomColor(), 15);
		return Left_Hand_Height.Z - 10;
	}
	
	else
	{
		Debug::Print(FString::FromInt(Right_Hand_Height.Z), FColor::MakeRandomColor(), 15);
		return Right_Hand_Height.Z - 10;
	}

}

bool UCustom_Movement_Component::Realize_And_Validate_Hop_Destnation_And_Action()
{
	if(!Wall_Top_Result.bBlockingHit)
	return false;
	
	Set_Horizontal_Hop_Distance_Value_Based_On_Parkour_Direction(Get_Controller_Direction());
	Set_Vertical_Hop_Distance_Value_Based_On_Parkour_Direction(Get_Controller_Direction());

	const FVector Offset_Vector_1{Move_Vector_Right(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z, Horizontal_Hop_Distance)};
	const FVector Offset_Vector_2{Move_Vector_Up(Offset_Vector_1, Vertical_Hop_Distance)};

	Hop_Grid_Scan_For_Hop_Hit_Result(Offset_Vector_2, Reversed_Front_Wall_Normal_Z, 10.f, 10.f);

	//"Grid_Scan_Hit_Traces" array is filled by the function call "Grid_Scan_For_Hit_Results()". 
	if(Grid_Scan_Hop_Hit_Traces.Num() != 0)
	{
		//This function analyzes the FHitResults stored in the array "Grid_Scan_Hit_Traces" for the line traces which are just under the top edge on the front side of the wall.
		//Said line traces are stored in the array "Front_Wall_Top_Edge_Traces".
		Analyze_Hop_Grid_Scan_For_Front_Wall_Top_Edge_Hits();
		
		//Empty the array because the information in it has been analyzed in "&UCustom_Movement_Component::Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()".
		//It needs to be empty so it can be filled with new information the next time it needs to be used.
		Grid_Scan_Hop_Hit_Traces.Empty();
	}
	
	//Front_Wall_Top_Edge_Traces are filled by the function call "Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()".
	if(Front_Wall_Top_Edge_Hop_Traces.Num() != 0)
	{
		//This function analyzes the line traces stored in the array "Front_Wall_Top_Edge_Hop_Traces" for the line trace which is closes to the character's current location.
		//Said line trace is stored in the global variable "Front_Wall_Top_Edge_Best_Hit{}".
		Realize_Front_Wall_Top_Edge_Best_Hop_Hit();

		//Empty the array because the information in it has been analyzed in "&UCustom_Movement_Component::Realize_Front_Wall_Top_Edge_Best_Hop_Hit()".
		//It needs to be empty so it can be filled with new information the next time it needs to be used.
		Front_Wall_Top_Edge_Hop_Traces.Empty();
	}

	//Global FhitResult variable "Front_Wall_Top_Edge_Best_Hit" is filled with the line trace that is just under the top edge on the front wall in the function call "Realize_Front_Wall_Top_Edge_Best_Hop_Hit()".
	//This check is to make sure said FHitResult does indeed have a blocking hit and no initial overlap is active. This FHitResult is used to analyze the top surface of the wall which in result will enable
	//the calculation of the location which the character will land on when vaulting.
	if(Front_Wall_Top_Edge_Best_Hop_Hit.bBlockingHit && !Front_Wall_Top_Edge_Best_Hop_Hit.bStartPenetrating)
	{
		if(Get_Hop_Top_Result())
		{
			if(Validate_Absence_Of_Obstacles_Before_Hopping())
			{
				Debug::Print("Hop_Location_Found", FColor::Green, 17);
				Initialize_Parkour_IK_Limbs_Hit_Result = Hop_Top_Hit_Result;
				Wall_Top_Result = Hop_Top_Hit_Result;
				return true;
			}

			else
			{
				Debug::Print("Hop_Location_Not_Found", FColor::Red, 17);
				return false;
			}
		}

		else
		{
			Debug::Print("Hop_Location_Not_Found", FColor::Red, 17);
			return false;
		}
	}

	else
	{
		Debug::Print("Hop_Location_Not_Found", FColor::Red, 17);
		return false;
	}

}

void UCustom_Movement_Component::Perform_Hop_Action(const FGameplayTag& Hop_Action)
{
	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Up"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Execute_Random_Montage(Hop_Up_Array);
		}
	
		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Execute_Random_Montage(Braced_And_Ledge_Hop_Left_Array);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Execute_Random_Montage(Braced_And_Ledge_Hop_Right_Array);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Up"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Execute_Random_Montage(Braced_And_Adventure_Hop_Up_Left_Array);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Up"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Execute_Random_Montage(Braced_And_Adventure_Hop_Up_Right_Array);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Down"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Execute_Random_Montage(Braced_And_Ledge_Hop_Down_Array);
		}
	
		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Down"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Play_Parkour_Montage(Climb_Shimmy_Long_L_Down_Left);
		}
	
		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Down"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Play_Parkour_Montage(Climb_Shimmy_Long_R_Down_Right);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Left"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Play_Parkour_Montage(Free_Hang_Hop_Left);
		}
	
		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Right"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Play_Parkour_Montage(Free_Hang_Hop_Right);
		}
	
		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Down"))))
		{
			// Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			Play_Parkour_Montage(Hanging_Drop);
		}
	}

	else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Up"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Up);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Down"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Down);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Left"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Left);
		}

		else if(Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Right"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Right);
		}
	}
}

void UCustom_Movement_Component::Execute_Drop_Into_Shimmy()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	return;
	
	if(Ground_Speed >= 500.f && Air_Speed == 0.f)
	{
		const int Distance_To_Check_For_Drop{7};

		if(Validate_Drop_To_Shimmy(Distance_To_Check_For_Drop))
		{
			if(Wall_Top_Result.bBlockingHit)
			{
				/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
				FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
				"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
				after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
				a location to begin the next sequence of ray casts.*/
				Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
				
				//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
				//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
				//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
				const FHitResult Not_Needed_Here{};
				Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

				Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

				if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Accelerating.Drop"))));

				else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Accelerating.Drop"))));

				Debug::Print(TEXT("Executing_Accelerating_Drop_To_Shimmy"), FColor::MakeRandomColor(), 8);
			}
		}
	}

	else if(Ground_Speed <= 300.f && Air_Speed == 0.f)
	{
		const int Distance_To_Check_For_Drop{2};

		if(Validate_Drop_To_Shimmy(Distance_To_Check_For_Drop))
		{
			if(Wall_Top_Result.bBlockingHit)
			{
				/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
				FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
				"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
				after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
				a location to begin the next sequence of ray casts.*/
				Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
				
				//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
				//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
				//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
				const FHitResult Not_Needed_Here{};
				Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
				
				Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

				if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Normal.Drop"))));

				else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Normal.Drop"))));

				Debug::Print(TEXT("Executing_Normal_Drop_To_Shimmy"), FColor::MakeRandomColor(), 8);
			}
		}
	}
}

void UCustom_Movement_Component::On_Landing_Impact()
{
	// if(Ground_Speed <= 250.f && (Air_Speed <= -700.f && Air_Speed >= -1000.f))
	// Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Light"))));
	
	if(Ground_Speed < 240.f && ((Air_Speed <= -800.f && Air_Speed >= -900.f) || Air_Speed <= -1100.f && Air_Speed >= -1200.f))
	{
		Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Impact"))));
	}
	
	else if(((Ground_Speed >= 200.f && Ground_Speed <= 250.f) || Ground_Speed >=  499.f) && ((Air_Speed <= -800.f && Air_Speed >= -900.f) || (Air_Speed <= -1000.f && Air_Speed >= -1300.f) || (Air_Speed <= -1200.f && Air_Speed > -1300.f)))
	{
		Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Front"))));
	}
	
	else if((Ground_Speed >=  499.f && Air_Speed <= -1300.f) || (Ground_Speed <= 499.f && Air_Speed <= -1300.f))
	{
		Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Roll"))));
	}

}

void UCustom_Movement_Component::Execute_Parkour_Action()
{
	if(Parkour_Action != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))) || 
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))) ||
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))) ||
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Pipe.Climb"))) || 
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Balance.Walk"))) ||
	Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	return;

	Parkour_Detect_Wall();
	
	if(Initial_Front_Wall_Hit_Result.bBlockingHit)
	{
		FVector Location_To_Exectute_Grid_Scan_For_Hit_Results{Initial_Front_Wall_Hit_Result.ImpactPoint.X,
															   Initial_Front_Wall_Hit_Result.ImpactPoint.Y,
															   Select_Value_Based_On_Parkour_State(Parkour_State, Initial_Front_Wall_Hit_Result.ImpactPoint.Z, 0.f, Get_Characters_Highest_Hand_Height())};
		
		Grid_Scan_For_Hit_Results(Location_To_Exectute_Grid_Scan_For_Hit_Results, 
								  Reverse_Wall_Normal_Rotation_Z(Initial_Front_Wall_Hit_Result.ImpactNormal), 
								  Select_Value_Based_On_Parkour_State(Parkour_State, 4.f, 0.f, 2.f), 
								  Select_Value_Based_On_Parkour_State(Parkour_State, 30.f, 0.f, 8.f));
	}

	//"Grid_Scan_Hit_Traces" array is filled by the function call "Grid_Scan_For_Hit_Results()". 
	if(Grid_Scan_Hit_Traces.Num() != 0)
	{
		//This function analyzes the FHitResults stored in the array "Grid_Scan_Hit_Traces" for the line traces which are just under the top edge on the front side of the wall.
		//Said line traces are stored in the array "Front_Wall_Top_Edge_Traces".
		Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits();
		
		//Empty the array because the information in it has been analyzed in "&UCustom_Movement_Component::Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()".
		//It needs to be empty so it can be filled with new information the next time it needs to be used.
		Grid_Scan_Hit_Traces.Empty();
	}
	
	//Front_Wall_Top_Edge_Traces are filled by the function call "Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()".
	if(Front_Wall_Top_Edge_Traces.Num() != 0)
	{
		//This function analyzes the line traces stored in the array "Front_Wall_Top_Edge_Traces" for the line trace which is closes to the character's current location.
		//Said line trace is stored in the global variable "Front_Wall_Top_Edge_Best_Hit{}".
		Realize_Front_Wall_Top_Edge_Best_Hit();

		//Empty the array because the information in it has been analyzed in "&UCustom_Movement_Component::Realize_Front_Wall_Top_Edge_Best_Hit()".
		//It needs to be empty so it can be filled with new information the next time it needs to be used.
		Front_Wall_Top_Edge_Traces.Empty();
	}

	//Global FhitResult variable "Front_Wall_Top_Edge_Best_Hit" is filled with the line trace that is just under the top edge on the front wall in the function call "Realize_Front_Wall_Top_Edge_Best_Hit()".
	//This check is to make sure said FHitResult does indeed have a blocking hit and no initial overlap is active. This FHitResult is used to analyze the top surface of the wall which in result will enable
	//the calculation of the location which the character will land on when vaulting.
	if(Front_Wall_Top_Edge_Best_Hit.bBlockingHit && !Front_Wall_Top_Edge_Best_Hit.bStartPenetrating)
	{
		Analyze_Wall_Top_Surface();
		Calculate_Wall_Vault_Location();
	}

	//Using the global FHitResults Wall_Top_Result (The first sphere trace (Index == 0) executed in the for loop found within function call "Analyze_Wall_Top_Surface()"), 
	//Wall_Depth_Result (The last sphere trace (Index != 0) executed in the for loop found within function call "Analyze_Wall_Top_Surface()" and Wall_Vault_Result (the sphere trace executed
	//in the function call "Calculate_Wall_Vault_Location()"), calculations are made to obtain the Wall_Height, Wall_Depth and Vault_Height.
	Measure_Wall();

	//Check to see if the global pointer "Wall_Vault_Actor" is valid. If it is indeed valid then this means the player is overlapping with a "AWall_Vault_Actor" and the 
	//appropriate action should be executed within "&UCustom_Movement_Component::Decide_Wall_Vault_Parkour_Action". 
	
	if(Wall_Vault_Actor)
	{
		Decide_Wall_Vault_Parkour_Action();
	}

	//Check to see if the global pointer "Tic_Tac_Actor" is valid. If it is indeed valid then this means the player is overlapping with a "ATic_Tac_Actor" and the 
	//appropriate action should be executed within "&UCustom_Movement_Component::Decide_Tic_Tac_Parkour_Action".
	else if(Tic_Tac_Actor)
	{
		Decide_Tic_Tac_Parkour_Action();
	}
	
	//Otherwise call "&UCustom_Movement_Component::Decide_Parkour_Action" to execute the actions which are enabled by default to the character.
	else
	{
		//Decide the Parkour Action to execute based on the current value of the global gameplaytag "Parkour_State" as well as the values stored in the global double variables
		//"Wall_Height", "Wall_Depth" and "Vault_Height".
		Decide_Parkour_Action();
	}

}

void UCustom_Movement_Component::Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam()
{
	const float In_Time{1.f};

	Set_Parkour_State(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))));

	Set_Parkour_Wall_Run_Side(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.None"))));

	Realize_Wall_Run_Left_Side_Hit_Result.Reset(In_Time, false);

	Realize_Wall_Run_Right_Side_Hit_Result.Reset(In_Time, false);

	Wall_Run_Hit_Result.Reset(In_Time, false);

	Direction_To_Wall_Run = FVector(0, 0, 0);

	Location_To_Move_Charater_During_Wall_Run = FVector(0, 0, 0);
	
	Direction_To_Wall_Run_Scalar_Value_Multiplier = 1.f;

	Dynamic_Wall_Run_Arc_Value = 0.f;
}

bool UCustom_Movement_Component::Analyze_And_Validate_Wall_Run_Surface()
{
	/*This function takes care of making sure the wall is perpendicular to the character (just to be safe), analyzing the wall to get the direction which the character will be wall running in,  
	and making sure the character is attempting to start wall running from the appropriate angle ranges.*/

	//Make sure the character is perpendicular to the wall.
	double Dot_Product_Of_Wall_Run_Surface_And_Character{};

	if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
	{
		Dot_Product_Of_Wall_Run_Surface_And_Character = FVector::DotProduct(Wall_Run_Hit_Result.ImpactNormal, UpdatedComponent->GetUpVector());
		Debug::Print("Wall_Run_Left_Side_Dot_Product_Of_Wall_Run_Surface_And_Character: " + FString::SanitizeFloat(Dot_Product_Of_Wall_Run_Surface_And_Character), FColor::Yellow, 22);
		
	}

	else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
	{
		Dot_Product_Of_Wall_Run_Surface_And_Character = FVector::DotProduct(Wall_Run_Hit_Result.ImpactNormal, UpdatedComponent->GetUpVector());
		Debug::Print("Wall_Run_Right_Side_Dot_Product_Of_Wall_Run_Surface_And_Character: " + FString::SanitizeFloat(Dot_Product_Of_Wall_Run_Surface_And_Character), FColor::Yellow, 22);
	}

	if(Dot_Product_Of_Wall_Run_Surface_And_Character != 0)
	{
		Debug::Print("Wall_Is_Not_Perpendicular_To_Character", FColor::Red, 23);
		Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
		return false;
	}

	else if(Dot_Product_Of_Wall_Run_Surface_And_Character == 0)
	{
		Debug::Print("Wall_Is_Perpendicular_To_Character", FColor::Green, 23);
	}

	//Get the direction the charcter will be running in by getting the cross product of the character's respective vector and the impact normal of the wall and store it in the global FVector "Direction_To_Wall_Run"

	if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
	{
		Direction_To_Wall_Run = FVector::CrossProduct(Wall_Run_Hit_Result.ImpactNormal, UpdatedComponent->GetUpVector());
		Direction_To_Wall_Run = Direction_To_Wall_Run.GetSafeNormal();
		Debug::Print("Wall_Run_Left_Side_Direction_To_Wall_Run: " + Direction_To_Wall_Run.ToCompactString(), FColor::Green, 24);
	}

	else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
	{
		Direction_To_Wall_Run = FVector::CrossProduct(Wall_Run_Hit_Result.ImpactNormal, -UpdatedComponent->GetUpVector());
		Direction_To_Wall_Run = Direction_To_Wall_Run.GetSafeNormal();
		Debug::Print("Wall_Run_Right_Side_Direction_To_Wall_Run: " + Direction_To_Wall_Run.ToCompactString(), FColor::Green, 24);
	}

	//Make sure the character is attempting to start wall running from the appropriate angle ranges.
	double Angle_Attempting_To_Enter_Wall_Run_SIDE{};
	
	if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
	{
		Angle_Attempting_To_Enter_Wall_Run_SIDE = UKismetMathLibrary::DegAcos(Direction_To_Wall_Run.Dot(UpdatedComponent->GetRightVector()));
		
		if(Angle_Attempting_To_Enter_Wall_Run_SIDE > 90.f)
		{
			Debug::Print("Angle_Which_The_Character_Is_Attempting_Start_LEFT_Wall_Running_Is_Invalid: " + 
			FString::SanitizeFloat(Angle_Attempting_To_Enter_Wall_Run_SIDE), 
			FColor::Red, 25);

			Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();

			return false;
		}

		else
		{
			Debug::Print("Angle_Which_The_Character_Is_Attempting_Start_LEFT_Wall_Running_Is_Valid: " + 
			FString::SanitizeFloat(Angle_Attempting_To_Enter_Wall_Run_SIDE), 
			FColor::Green, 25);
		}
	}

	else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
	{
		Angle_Attempting_To_Enter_Wall_Run_SIDE = UKismetMathLibrary::DegAcos(Direction_To_Wall_Run.Dot(-UpdatedComponent->GetRightVector()));

		if(Angle_Attempting_To_Enter_Wall_Run_SIDE > 90)
		{
			Debug::Print("Angle_Which_The_Character_Is_Attempting_Start_RIGHT_Wall_Running_Is_Invalid: " + 
			FString::SanitizeFloat(Angle_Attempting_To_Enter_Wall_Run_SIDE), 
			FColor::Red, 25);

			Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();

			return false;
		}

		else
		{
			Debug::Print("Angle_Which_The_Character_Is_Attempting_Start_RIGHT_Wall_Running_Is_Valid: " + 
			FString::SanitizeFloat(Angle_Attempting_To_Enter_Wall_Run_SIDE), 
			FColor::Green, 25);
		}
	}

	
	double Angle_Attempting_To_Enter_Wall_Run_FRONT{UKismetMathLibrary::DegAcos(Direction_To_Wall_Run.Dot(UpdatedComponent->GetForwardVector()))};

	if(Angle_Attempting_To_Enter_Wall_Run_FRONT < 0 /* 10 */ || Angle_Attempting_To_Enter_Wall_Run_FRONT > 90)
	{
		Debug::Print("Character_Has_To_Face_The_Wall_More_In_Order_To_Begin_Wall_Running: " + 
		FString::SanitizeFloat(Angle_Attempting_To_Enter_Wall_Run_FRONT), 
		FColor::Red, 27);

		Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
		return false;
	}

	else
	{
		Debug::Print("Charater_IS_READY_To_Begin_Wall_Running: " + 
		FString::SanitizeFloat(Angle_Attempting_To_Enter_Wall_Run_FRONT), 
		FColor::Green, 27);

		//The global FHitResult "Wall_Top_Result" is used by the function "&UCustom_Movement_Component::Play_Parkour_Montage". Therefore it will be used to play specicific montages from the Wall_Run
		//when necessary.
		Wall_Top_Result = Wall_Run_Hit_Result;
		Reversed_Front_Wall_Normal_Z = UKismetMathLibrary::MakeRotFromX(Direction_To_Wall_Run);

		//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
		//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
		//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
		const FHitResult Not_Needed_Here{};
		Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

		return true;

	}

	//This line is to meet the requirement type of this function
	return false;
}

void UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Wall_Run_Position(const FHitResult& Wall_Running_Hit_Result)
{
	//Check to see if there is a valid blocking hit in the FHitResult that's passed in via the input argument. If there is no valid blocking hit
	//call the funtion "&UCustom_Movement_Component::Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam"
	if(!Wall_Run_Hit_Result.bBlockingHit)
	{
		Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
		return;
	}

	//Direction for the character to face while wall running
	const FRotator Direction_For_The_Character_To_Face{UKismetMathLibrary::MakeRotFromX(Direction_To_Wall_Run)};
	
	//Depending on which side the wall is in Offset the vector so the character is not clipping through the wall.
	FVector Offset_Vector{};

	if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
	Offset_Vector = Move_Vector_Right(Wall_Running_Hit_Result.ImpactPoint, Direction_For_The_Character_To_Face, 50.f);

	else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
	Offset_Vector = Move_Vector_Left(Wall_Running_Hit_Result.ImpactPoint, Direction_For_The_Character_To_Face, 50.f);

	//Set the value in the global FVector "Location_To_Move_Charater_During_Wall_Run" to equal the Offset Vector's location + "(Direction_To_Wall_Run * Direction_To_Wall_Run_Scalar_Value_Multiplier)".
	//The scalar value will be incremented every tick within the function "&UCustom_Movement_Component::Move_Character_To_New_Wall_Run_Position_Interpolation_Settings"
	//while wall running. This will cause an effect of the character "chasing" the new location (current location multipled by the scalar value) until the wall run is over. 
	//In short the magnitude of the global FVector "Direction_To_Wall_Run" will increse every tick when the value set in the global float "Direction_To_Wall_Run_Scalar_Value_Multiplier" is incremented
	//within "&UCustom_Movement_Component::Move_Character_To_New_Wall_Run_Position_Interpolation_Settings".
	Location_To_Move_Charater_During_Wall_Run = (Direction_To_Wall_Run * Direction_To_Wall_Run_Scalar_Value_Multiplier) + Offset_Vector;

	//Call the function "&UCustom_Movement_Component::Move_Character_To_New_Wall_Run_Position_Interpolation_Settings" to interpolate the characters current location to the one identified in this function.
	Move_Character_To_New_Wall_Run_Position_Interpolation_Settings(Location_To_Move_Charater_During_Wall_Run, Direction_For_The_Character_To_Face);

	//Call the function "&UCustom_Movement_Component::Dynamic_Wall_Run_Arc_Path" which handles offseting the Capsule Component's world Z location when wall running to give an arc like look.
	Dynamic_Wall_Run_Arc_Path();
	
	return;
}

void UCustom_Movement_Component::Move_Character_To_New_Wall_Run_Position_Interpolation_Settings(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	if(!Owning_Player_Character)
	return;

	//Get the current location of the character and dismantle the vector.
	const FVector Characters_Current_Location{Owning_Player_Character->GetActorLocation()};

	//Store DeltaTime in a variable so that it may be used to interpolate the character.
	const double DeltaTime{UGameplayStatics::GetWorldDeltaSeconds(this)};

	//Store the Interpolation speed into a local double variable.
	const double Interploation_Speed{400.f};

	//Store the values of the interpolation where the character will be when the interpolation is over in local double variables.
	const double Interpolated_Location_X{UKismetMathLibrary::FInterpTo_Constant(Characters_Current_Location.X, Location_To_Move_Character.X, DeltaTime, Interploation_Speed)};
	const double Interpolated_Location_Y{UKismetMathLibrary::FInterpTo_Constant(Characters_Current_Location.Y, Location_To_Move_Character.Y, DeltaTime, Interploation_Speed)};
	double Interpolated_Location_Z{};

	if(Owning_Player_Character->HasAuthority())
	{
		Interpolated_Location_Z = Characters_Current_Location.Z + Dynamic_Wall_Run_Arc_Value_Degrees/2.f;
	}

	else
	{
		Interpolated_Location_Z = Characters_Current_Location.Z + Dynamic_Wall_Run_Arc_Value_Degrees/2.5f;
	}

	//Store the location where the character will be into a local FVector.
	const FVector New_Location_To_Move_Charater_During_Wall_Run{FVector(Interpolated_Location_X, Interpolated_Location_Y, Interpolated_Location_Z)};

	//Call the function "Owning_Player_Character->SetActorLocationAndRotation()" and pass in the new location to move the character every tick.
	Owning_Player_Character->SetActorLocationAndRotation(New_Location_To_Move_Charater_During_Wall_Run, Rotation_For_Character_To_Face);
	
	//Increment the global double variable "Direction_To_Wall_Run_Scalar_Value_Multiplier".
	Direction_To_Wall_Run_Scalar_Value_Multiplier++;

	if(!Owning_Player_Character->HasAuthority())
	{
		Server_Move_Character_To_New_Wall_Run_Position_Interpolation_Settings(New_Location_To_Move_Charater_During_Wall_Run, Rotation_For_Character_To_Face);
	}

	return;
}

void UCustom_Movement_Component::Dynamic_Wall_Run_Arc_Path()
{
	//Store DeltaTime into a variable
	const double DeltaTime{UGameplayStatics::GetWorldDeltaSeconds(this)};

	//Interpolate the value which will be set into the global float variable "Dynamic_Wall_Run_Arc_Value" between 0 and 180. This will be used alongside DegSin to generate the Sine degree direction to set the capsule component.
	Dynamic_Wall_Run_Arc_Value = UKismetMathLibrary::FInterpTo_Constant(Dynamic_Wall_Run_Arc_Value, 180.f, DeltaTime, 45.f);

	Debug::Print(FString::SanitizeFloat(Dynamic_Wall_Run_Arc_Value), FColor::Red, 30);

	//Check to see if the value set within the global float variable "Dynamic_Wall_Run_Arc_Value" is greater than or equal to 179. If this is the case then the character has performed a wall run which has exhausted the arc path set within this function. 
	//Therefore set the variable within the global FGameplayTag "Parkour_Action" to the appropriate gameplay tag via "&UCustom_Movement_Component::Set_Parkour_Action" so the respective montage may be played. Otherwise, generate the arc path.
	if(Dynamic_Wall_Run_Arc_Value >= 179.f)
	{
		if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
		{
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.L.Finish"))));
			Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			return;
		}

		else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
		{
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.R.Finish"))));
			Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			return;
		}
	}

	else
	{
		//Check to see if the value set within the global float variable "Dynamic_Wall_Run_Arc_Value" is greater than or equal to 90. If this is the case then the character has reached the climax of the arc and the decent slope of the arch should be generated and executed.
		//Otherwise the upward slope of the arc should be generated and executed.
		if(Dynamic_Wall_Run_Arc_Value >= 90)
		{
			//Generate the degrees for the arc by multiplying the value stored into the global float variable "Dynamic_Wall_Run_Arc_Value" by -1 (because the arc is now on the downward slope).
			Dynamic_Wall_Run_Arc_Value_Degrees = UKismetMathLibrary::DegSin(Dynamic_Wall_Run_Arc_Value * -1);

			//Store the offset location back into the local FVector variable "Location_To_Offset_Updated_Component"
			const FVector Location_To_Offset_Updated_Component{0, 0, Dynamic_Wall_Run_Arc_Value_Degrees};

			//Call "Capsule_Component->AddWorldOffset()" and pass in the offset location.
			UpdatedComponent->AddWorldOffset(Location_To_Offset_Updated_Component);

			return;
		}

		else if(Dynamic_Wall_Run_Arc_Value < 90)
		{
			//Generate the degrees for the arc by usung the value stored into the global float variable "Dynamic_Wall_Run_Arc_Value".
			Dynamic_Wall_Run_Arc_Value_Degrees = UKismetMathLibrary::DegSin(Dynamic_Wall_Run_Arc_Value);

			//Store the offset location back into the local FVector variable "Location_To_Offset_Updated_Component"
			const FVector Location_To_Offset_Updated_Component{0, 0, Dynamic_Wall_Run_Arc_Value_Degrees};

			//Call "Capsule_Component->AddWorldOffset()" and pass in the offset location.
			UpdatedComponent->AddWorldOffset(Location_To_Offset_Updated_Component);

			return;
		}
	}
}

void UCustom_Movement_Component::Parkour_Wall_Run_Handle_Wall_Run_Movement()
{
	// //Store the absolute value of the global double variable "Right_Left_Movement_Value"
	const double Right_Left_Movement_Absolute_Value{UKismetMathLibrary::Abs(Right_Left_Movement_Value)};

	//Check to see if the value stored in the global double variable "Forward_Backward_Movement_Value" is greater than .7, If so this means there is forward movement into the controller 
	//and the character should continue wall running.
	if(Forward_Backward_Movement_Value > .7f || Right_Left_Movement_Absolute_Value > .7f)
	{
		//Set the variable within the global FGameplayTag variable Parkour_Direction to be the same as the input placed into the controller by the player. During wall running there should only be forward, left and right movement.
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
		{
			Set_Parkour_Direction(Get_Controller_Direction());
			Debug::Print("Wall_Run_Direction_Set_To_FORWARD", FColor::Cyan, 29);

			Calculate_And_Move_Character_To_New_Wall_Run_Position(Wall_Run_Hit_Result);

			return;
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
		{
			Set_Parkour_Direction(Get_Controller_Direction());
			Debug::Print("Wall_Run_Direction_Set_To_LEFT", FColor::Cyan, 29);
			
			if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
			{
				Calculate_And_Move_Character_To_New_Wall_Run_Position(Wall_Run_Hit_Result);
			}

			else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Right.Jump.90.L"))));
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			}

			return;
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
		{
			Set_Parkour_Direction(Get_Controller_Direction());

			if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
			{
				Calculate_And_Move_Character_To_New_Wall_Run_Position(Wall_Run_Hit_Result);
			}

			else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Right.Jump.90.L"))));
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			}

			Debug::Print("Wall_Run_Direction_Set_To_FORWARD_LEFT", FColor::Cyan, 29);
			return;
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
		{
			Set_Parkour_Direction(Get_Controller_Direction());
			Debug::Print("Wall_Run_Direction_Set_To_RIGHT", FColor::Cyan, 29);

			if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Left.Jump.90.R"))));
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			}

			else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
			{
				Calculate_And_Move_Character_To_New_Wall_Run_Position(Wall_Run_Hit_Result);
			}

			return;
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
		{
			Set_Parkour_Direction(Get_Controller_Direction());

			if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Left.Jump.90.R"))));
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
			}

			else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
			{
				Calculate_And_Move_Character_To_New_Wall_Run_Position(Wall_Run_Hit_Result);
			}

			Debug::Print("Wall_Run_Direction_Set_To_FORWARD_RIGHT", FColor::Cyan, 29);
			return;
		}
	}

	/* else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))) || 
	Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) ||
	Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
	Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
	} */

	else
	{
		Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
	}
}

void UCustom_Movement_Component::Set_bCan_Jump_From_Wall_Run_To_True()
{
	/*This function is called within &UCustom_Movement_Component::Execute_Exit_Wall_Run_With_Jump_Forward.*/
	
	//When the the global bool variable "bCan_Jump_From_Wall_Run" is set to false within &UCustom_Movement_Component::Execute_Exit_Wall_Run_With_Jump_Forward a cool down timer
	//is started. Until this cool down timer is complete the character will not be able to call the function &UCustom_Movement_Component::Execute_Exit_Wall_Run_With_Jump_Forward while
	//the FGameplayTag set into the global FGameplayTag variable "Parkour_State" is set to "Parkour.State.Wall.Run".
	
	bCan_Jump_From_Wall_Run = true;
	Debug::Print("bCan_Jump_From_Wall_Run Set To True");
}

void UCustom_Movement_Component::Execute_Exit_Wall_Run_With_Jump_Forward()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))
	return;

	else if(bCan_Jump_From_Wall_Run)
	{
		if(Validate_Can_Jump_From_Wall_Run())
		{
			if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.L.Jump.F"))));
				bCan_Jump_From_Wall_Run = false;
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();

				if(Owning_Player_Character)
				{
					Owning_Player_Character->GetWorldTimerManager().SetTimer(
						Set_bCan_Jump_From_Wall_Run_Timer_Handle,
						this,
						&UCustom_Movement_Component::Set_bCan_Jump_From_Wall_Run_To_True,
						Set_bCan_Jump_From_Wall_Run_Timer_Duration
					);
				}
			}

			else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.R.Jump.F"))));
				bCan_Jump_From_Wall_Run = false;
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();

				if(Owning_Player_Character)
				{
					Owning_Player_Character->GetWorldTimerManager().SetTimer(
						Set_bCan_Jump_From_Wall_Run_Timer_Handle,
						this,
						&UCustom_Movement_Component::Set_bCan_Jump_From_Wall_Run_To_True,
						Set_bCan_Jump_From_Wall_Run_Timer_Duration
					);
				}
			}
		}
	}
}

void UCustom_Movement_Component::Execute_Wall_Run()
{
	//If the FGameplayTag set into the global FGameplayTag variable "Parkour_State" is not "Parkour.State.Free.Roam" or the character's velocity set into the global float  variable "Ground_Speed" is less
	//than 500.f return. Wall Running should only be possible when the character is at a decent speed and is free roaming.
	if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))) && Ground_Speed > 490.f)
	{
		if(bIs_On_Ground)
		{
			//If the character is grounded when the wall run is initiated the arc path should start from its lowest point (0 degrees).
			Dynamic_Wall_Run_Arc_Value = 0.f;

			//Call the function "Realize_Wall_Run_Surfaces" to perform ray scans on either side of the character to determine if there are walls which can be ran across.
			Realize_Wall_Run_Surfaces(true);
			Realize_Wall_Run_Surfaces(false);

			//Analyze the hit result(s) stored into the global FHitResult varables "Realize_Wall_Run_Left_Side_Hit_Result" and "Realize_Wall_Run_Right_Side_Hit_Result" (if there are any) and determine which one is closest to the character.
			if(Realize_Wall_Run_Left_Side_Hit_Result.bBlockingHit || Realize_Wall_Run_Right_Side_Hit_Result.bBlockingHit)
			{
				Assign_Wall_Run_Hit_Result(Realize_Wall_Run_Left_Side_Hit_Result, Realize_Wall_Run_Right_Side_Hit_Result);
			}
	
			else
			{
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
				return;
			}

			//The global FHitResult "Wall_Run_Hit_Result" has now been assigned a valid HitResult. It's now time to analyze the wall and get the direction which the character will be wall running in,
			//make sure the wall is perpendicular to the character (just to be safe) and make sure the character is attempting to start wall running from the appropriate angle ranges.
			if(Analyze_And_Validate_Wall_Run_Surface())
			{
				//Set the value within the global FGameplayTag "Parkour_Wall_Run_Side" to the appropriate value (Calculated within "&UCustom_Movement_Component::Assign_Wall_Run_Hit_Result"). 
				//This will initiate the respective montage to play which will place the character at the location stored in the FHitResult "Wall_Run_Hit_Result 
				//(location transfered to the global FHitResult variable "Wall_Top_Result" within "&UCustom_Movement_Component::Analyze_And_Validate_Wall_Run_Surface").
				//After these lines of code are executed the value set in the global FGameplayTag Parkour_State will be set to "Parkour.State.Wall.Run" via the UParkour_Action_Data object that stores the respective information. 
				//Therefore the fucntions called within &UCustom_Movement_Component::Parkour_Call_In_Tick within "else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))" 
				//will handle the rest of the wall run execution.
				if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Left"))));
					Debug::Print("Parkour_Action_Set_To_Wall_Run_Start_Left", FColor::Emerald, 28);
				}

				else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Right"))));
					Debug::Print("Parkour_Action_Set_To_Wall_Run_Start_Right", FColor::Emerald, 28);
				}
			}
	
			else
			{
				Debug::Print("Wall_Run_Analyzation_And_Validation_FAILED", FColor::Red, 28);
				return;
			}
		}

		else if(!bIs_On_Ground)
		{
			if(Air_Speed >= 0.f)
			{
				//If the character is not grounded and is on an upward trajectory when the wall run is initiated the arc path should start closer to its origin.
				Dynamic_Wall_Run_Arc_Value = UKismetMathLibrary::RandomFloatInRange(0.f, 45.f);
			}

			else if(Air_Speed < 0.f)
			{
				//If the character is not grounded and is on a downward trajectory when the wall run is initiated the arc path should start closer to its climax.
				Dynamic_Wall_Run_Arc_Value = UKismetMathLibrary::RandomFloatInRange(45.f, 90.f);
			}

			//Call the function "Realize_Wall_Run_Surfaces" to perform ray scans on either side of the character to determine if there are walls which can be ran across.
			Realize_Wall_Run_Surfaces(true);
			Realize_Wall_Run_Surfaces(false);

			//Analyze the hit result(s) stored into the global FHitResult varables "Realize_Wall_Run_Left_Side_Hit_Result" and "Realize_Wall_Run_Right_Side_Hit_Result" (if there are any) and determine which one is closest to the character.
			if(Realize_Wall_Run_Left_Side_Hit_Result.bBlockingHit || Realize_Wall_Run_Right_Side_Hit_Result.bBlockingHit)
			{
				Assign_Wall_Run_Hit_Result(Realize_Wall_Run_Left_Side_Hit_Result, Realize_Wall_Run_Right_Side_Hit_Result);
			}
	
			else
			{
				Reset_Wall_Run_Variables_And_Set_Parkour_State_To_Free_Roam();
				return;
			}

			//The global FHitResult "Wall_Run_Hit_Result" has now been assigned a valid HitResult. It's now time to analyze the wall and get the direction which the character will be wall running in,
			//make sure the wall is perpendicular to the character (just to be safe) and make sure the character is attempting to start wall running from the appropriate angle ranges.
			if(Analyze_And_Validate_Wall_Run_Surface())
			{
				//Since the character is not grounded there is no reason to set the FGameplayTag within the global variable "Parkour_Action" to anything. Instead the global FGameplayTag "Parkour_State" will be set
				//to "Parkour.State.Wall.Run" and "Tick" will handle the rest of the wall run.
				Set_Parkour_State(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))));
			}
		}
	}
	
	else 
	return;
}

void UCustom_Movement_Component::Execute_Accelerating_Drop_Free_Roam()
{
	if(!Owning_Player_Character)
	return;
	
	if(Owning_Player_Character->Get_Is_Jogging() && !Owning_Player_Character->Get_bDrop_To_Shimmy() && Ground_Speed >= 499.f)
	{
		if(bIs_On_Ground)
		{
			if(Validate_Drop_Off_Ledge_While_Sprinting())
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Free.Roam.Accelerating.Drop"))));
			}
		}		
	}
}

void UCustom_Movement_Component::Move_Character_To_Front_Of_Pipe()
{
	/*This function interpolates the character's position and rotation to the front of the "AWall_Pipe_Actor" object which is being climbed. This needs to happen because the
	character may try to begin climbing the pipe from any side of the object. Regardless of which side of the "Wall_Pipe" object the character is attempting to begin climbing, said character's location will be interpolated to the location at the start of the line trace which said 
	"Wall_Pipe" object is generating every tick (look within the class "AWall_Pipe_Actor"). This line trace is being generated from the front side of the actor so that the character will always climb the 
	"Wall_Pipe from the correct side. The rotation of the character is also interpolated to the new rotation set within the global variable "Reversed_Front_Wall_Normal_Z"*/
	
	//Get DeltaTime
	const double DeltaTime{UGameplayStatics::GetWorldDeltaSeconds(this)};
	
	//Set the interpolation speed.
	const double Interpolation_Speed{140.f};

	//Interpolate the characters current location (X and Y) to the same location (X and Y) of the wall pipe.
	const double Interpolated_Location_X{UKismetMathLibrary::FInterpTo(UpdatedComponent->GetComponentLocation().X, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result.TraceStart.X + 25, DeltaTime, Interpolation_Speed)};
	const double Interpolated_Location_Y{UKismetMathLibrary::FInterpTo(UpdatedComponent->GetComponentLocation().Y,  Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result.TraceStart.Y, DeltaTime, Interpolation_Speed)};

	//combine the interpolated attributes into a single variable.
	const FVector New_Location_To_Move_Charater_During_Wall_Pipe_Climb{FVector(Interpolated_Location_X, Interpolated_Location_Y, UpdatedComponent->GetComponentLocation().Z)};

	//Call the function "Owning_Player_Character->SetActorLocationAndRotation()" and pass in the new location to move the character every tick.
	Owning_Player_Character->SetActorLocationAndRotation(New_Location_To_Move_Charater_During_Wall_Pipe_Climb, Reversed_Front_Wall_Normal_Z);
	
	if(Owning_Player_Character->IsLocallyControlled())
	{
		Set_Network_Move_Character_To_Front_Of_Pipe(New_Location_To_Move_Charater_During_Wall_Pipe_Climb, Reversed_Front_Wall_Normal_Z);
	}
}

void UCustom_Movement_Component::Release_From_Parkour_Wall_Pipe_Climb()
{
	/*This function enables the character to release from clampiing onto the "AWall_Pipe_Actor" object.*/

	//If the value set within the global FGameplayTag Parkour_State is not set to "Parkour.State.Wall.Pipe.Climb" return safely.
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return;

	//"Set_Parkour_Action" to be the FGameplayTag "Parkour.Action.Wall.Pipe.Fall.Down". This will execute the corresponding montage to play using the object of the UParkour_Action_Data pointer
	//"Wall_Pipe_Fall_Down"
	Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Fall.Down"))));
	Wall_Pipe_Actor = nullptr;
}

void UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb()
{
	/*The character should only be able to start climbing the "Wall_Pipe" object if the current value set 
	within the global FGameplayTag Parkour_State is set to "Parkour.State.Free.Roam",*/
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	return;

	//Check to see if there is a valid "AWall_Pipe_Actor" object in range of the character.
	if(Realize_Wall_Pipe_Surfaces())
	{
		//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
		//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
		//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
		Set_Network_Variables(Realize_Wall_Pipe_Hit_Result, Reverse_Wall_Normal_Rotation_Z(Wall_Pipe_Forward_Vector), Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);
		
		Debug::Print("Character_Ready_To_Begin_Wall_Pipe_Climb", FColor::Green, 42);
		
		//Call &UCustom_Movement_Component::Decide_Parkour_Action so the parkour action can be set. This will activate the corresponding montage to play witnin &UCustom_Movement_Component::Set_Parkour_Action.
		Decide_Parkour_Action();
		
		//Call &UCustom_Movement_Component::Move_Character_To_Front_Of_Pipe so that regardless of which side of the "Wall_Pipe" object the character is attempting to begin climbing, said character's location 
		//interpolated to the location at the start of the line trace which said "Wall_Pipe" object is generating every tick (look within the class "AWall_Pipe_Actor"). This line trace is being generated from the front side of the actor so that the 
		//character will always climb the "Wall_Pipe from the correct side. The rotation of the character is also interpolated to the new rotation set within the global variable "Reversed_Front_Wall_Normal_Z" (look up two lines of code)."  
		Move_Character_To_Front_Of_Pipe();
	}
}

void UCustom_Movement_Component::Parkour_Wall_Pipe_Climb_Handle_Pipe_Climbing_Movement()
{
	/*This function handles calling the functions which will validate whether the character can climb in the direction of the input which is passed into the global double variable "Forward_Backward_Movement_Value" && . 
	"Right_Left_Movement_Value" within the function "&UCustom_Movement_Component::Add_Movement_Input".*/
	
	//Store the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value" && "Forward_Backward_Movement_Value". This value will be used to check if the input to move the character to the right or left
	//within the function "&UCustom_Movement_Component::Add_Movement_Input" is above the threshold to accept input.
	const double Right_Left_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Right_Left_Movement_Value)};
	const double Forward_Backward_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Forward_Backward_Movement_Value)};

	//Check to see if the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value" is above the threshold to allow shimmying movement.
	//If the check is passed, check to see if the value is above or below 0. If the value is above 0 the character is moving to the right, if the value is below 0 the character is 
	//moving to the left.
	if(Right_Left_Movement_Value_Absolute_Value > .7 || Forward_Backward_Movement_Value_Absolute_Value > .7)
	{
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value), FColor::MakeRandomColor(), 7);
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))));
			Debug::Print("Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))));
			Debug::Print("Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}

		//This local variable is filled with values within the function "Parkour_Wall_Pipe_Climb_State_Detect_Pipe" (it is passsed in as references via the input argument in said function). 
		//The FHitResult stored within "Parkour_Wall_Pipe_Climbing_Wall_Top_Result" is used to determine whether there is a wall pipe in front of the character which the character can climb 
		//up or down via the function "Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body". Said FHitResult is passed in as const references via the input argument 
		//(it is filled with data within the function "Parkour_Wall_Pipe_Climb_State_Detect_Pipe").
		FHitResult Parkour_Wall_Pipe_Climbing_Wall_Top_Result{};
		
		//This function determines the location on the wall pipe for climbing movememnt to set the location of the character to. 
		if(!Parkour_Wall_Pipe_Climb_State_Detect_Wall_Pipe(Parkour_Wall_Pipe_Climbing_Wall_Top_Result))
		{
			Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
			return;
		}

		//This function checks to see if there is an obstacle ontop or below the character.
		else if(Parkour_Wall_Pipe_Climb_State_Are_There_Obstacles_Ontop_Or_Below_Body(Parkour_Wall_Pipe_Climbing_Wall_Top_Result.ImpactPoint) || !Parkour_Wall_Pipe_Climb_Detect_End_Of_Wall_Pipe(Parkour_Wall_Pipe_Climbing_Wall_Top_Result.ImpactPoint))
		{
			Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
			return;
		}

		else
		{
			Calculate_And_Move_Character_To_New_Wall_Pipe_Climb_Position(Parkour_Wall_Pipe_Climbing_Wall_Top_Result);
		}
	}

	else
	{
		Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		return;
	}
}

void UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Wall_Pipe_Climb_Position(const FHitResult& Parkour_Wall_Pipe_Climbing_Wall_Top_Result_Reference)
{
	/*This function is called calculates the location to interpolate the character to and passes in the said location into the function "&UCustom_Movement_Component::Move_Character_To_New_Climb_Position_Interpolation_Settings"
	as an input argument.*/
	
	//Offset value to be used to offset the character backwards from the wall pipe. This is because the impact point found within the input parameter "Parkour_Wall_Pipe_Climbing_Wall_Top_Result_Reference" is right on
	//the surface of the wall. Therefore the character needs to be moved back so that the animation playing will look realistic and natural. 
	const float Offset_Character_Backwards_From_Wall_Pipe_Value{25.f};
	const FVector Offset_Vector_Backwards_From_Wall_Pipe{Move_Vector_Backward(Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result.TraceStart, Reversed_Front_Wall_Normal_Z, Offset_Character_Backwards_From_Wall_Pipe_Value)};

	/*These values are used to make a custom FVector variable ("Move_Character_To_This_Location").*/

	//Value to use on the "X" axis of the custom FVector "Move_Character_To_This_Location".	
	const double Set_Character_To_This_Position_Parallel_From_Wall_Pipe{Offset_Vector_Backwards_From_Wall_Pipe.X};			
	//Value to use on the "Y" axis of the custom FVector "Move_Character_To_This_Location".	
	const double Set_Character_To_This_Position_Perpendicular_From_Wall_Pipe{Offset_Vector_Backwards_From_Wall_Pipe.Y};
	//Value to use on the "Z" axis of the custom FVector "Move_Character_To_This_Location".	
	const double Set_Character_To_This_Height_Position{Parkour_Wall_Pipe_Climbing_Wall_Top_Result_Reference.ImpactPoint.Z};


	//Custom FVector to pass into the function "&UCustom_Movement_Component::Move_Character_To_New_Climb_Position_Interpolation_Settings" as an input argument. This will be the location to interpolate 
	//the character to as long as there is input into the player controller and the validation and checks performed in the function "&UCustom_Movement_Component::Parkour_Climb_Handle_Shimmying_Movement"
	//are successful.. 
	const FVector Move_Character_To_This_Location(FVector(Set_Character_To_This_Position_Parallel_From_Wall_Pipe, 
														   Set_Character_To_This_Position_Perpendicular_From_Wall_Pipe, 
														   Set_Character_To_This_Height_Position));
	
	//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
	//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
	//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
	const FHitResult Not_Needed_Here{};
	Set_Network_Variables(Parkour_Wall_Pipe_Climbing_Wall_Top_Result_Reference, Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);

	//This function uses the location which is calculated above (using the variable from the FHitResult input argument "Parkour_Climbing_Detect_Wall_Hit_Result") to interpolate the character to said 
	//FVector. Considering the locations of the custom FVector will always be updating due to it being dependant on the input argument variable "Parkour_Climbing_Detect_Wall_Hit_Result",  
	//the character will always be "chasing" the location to interpolate its location to causeing an infinite interpolation. This is because the impact point of the input argument 
	//"Parkour_Climbing_Detect_Wall_Hit_Result" is offset to the right or left side of the arrow actor (the arrow actor is just above the character) depending on whether the character is moving
	//to the right or left.
	Move_Character_To_New_Climb_Position_Interpolation_Settings(Move_Character_To_This_Location, Reversed_Front_Wall_Normal_Z);
}

void UCustom_Movement_Component::Decide_Wall_Pipe_Maneuver_To_Free_Hang_Mantle_Or_Hop() 
{
	//This funtion should only be perfomred if the player is "Wall_Pipe_Climbing".
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))) && Parkour_Action != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
	return;
	
	//Check to see if the input placed by the controller is equal to "Forward". If this is the case then the character has the option to either mantle up on the surface
	//which is being climbed (if there is room), do a maneuver to a FreeHang or perform a hop action (if there is no room to perform a mantle and a hop destination is confirmed).
	if(Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
	{
		/* if(Validate_Can_Maneuver_To_Free_Hang())
		{
			// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
			// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
			// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
			// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
			// a location to begin the next sequence of ray casts.
			Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;

			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))));
		} */

		if(Validate_Wall_Pipe_Can_Mantle())
		{
			// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
			// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
			// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
			// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
			// a location to begin the next sequence of ray casts.
			Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
				
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Climb.Up.2.Hand"))));

			Wall_Pipe_Actor = nullptr;
		}

		else if(Validate_Wall_Pipe_Climb_Hop_Destnation_And_Action())
		{
			// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
			// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
			// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
			// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
			// a location to begin the next sequence of ray casts.
			Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);
			
			Set_Parkour_Action(Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()));
		}
	}
	
	else if(Validate_Wall_Pipe_Climb_Hop_Destnation_And_Action())
	{
		// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
		// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
		// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
		// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
		// a location to begin the next sequence of ray casts.
		Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
			
		//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
		//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
		//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
		Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);
			
		Set_Parkour_Action(Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()));
	}
	
}

bool UCustom_Movement_Component::Validate_Wall_Pipe_Climb_Hop_Destnation_And_Action()
{
	Set_Horizontal_Wall_Pipe_Hop_Distance_Value_Based_On_Parkour_Direction(Get_Controller_Direction());

	Set_Vertical_Wall_Pipe_Hop_Distance_Value_Based_On_Parkour_Direction(Get_Controller_Direction());

	if(Validate_Wall_Pipe_Hop_Location(Horizontal_Hop_Distance, Vertical_Hop_Distance))
	{
		Debug::Print("Wall_Pipe_Hop_Location_Validated", FColor::Green, 70);
		return true;
	}

	else
	{
		Debug::Print("Wall_Pipe_Hop_Location_Not_Validated", FColor::Red, 70);
		return false;
	}
}

FGameplayTag UCustom_Movement_Component::Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(const FGameplayTag& Current_Parkour_Direction)
{
	FGameplayTag Wall_Pipe_Hop_Action{};

	if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
	{
		return Wall_Pipe_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Up")));
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
	{
		return Wall_Pipe_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Right")));
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
	{
		return Wall_Pipe_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Left")));
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		return Wall_Pipe_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Down")));
	}

	else
	{
		return Wall_Pipe_Hop_Action = FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action")));
	}
}

void UCustom_Movement_Component::Set_Horizontal_Wall_Pipe_Hop_Distance_Value_Based_On_Parkour_Direction(const FGameplayTag& Current_Parkour_Direction)
{
	if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
	{
		Horizontal_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
	{
		Horizontal_Hop_Distance = 330.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
	{
		Horizontal_Hop_Distance = -330.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		Horizontal_Hop_Distance = 0.f;
	}

	else
	{
		Horizontal_Hop_Distance = 0.f;
	}

}

void UCustom_Movement_Component::Set_Vertical_Wall_Pipe_Hop_Distance_Value_Based_On_Parkour_Direction(const FGameplayTag& Current_Parkour_Direction)
{
	if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
	{
		Vertical_Hop_Distance = 350.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
	{
		Vertical_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
	{
		Vertical_Hop_Distance = 0.f;
	}

	else if(Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
	Current_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
	{
		Vertical_Hop_Distance = -300.f;
	}

	else
	{
		Vertical_Hop_Distance = 0.f;
	}
}

void UCustom_Movement_Component::Execute_Parkour_Wall_Pipe_Climb_Action()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	return;
	
	Decide_Parkour_Action();
}

void UCustom_Movement_Component::Execute_Start_Running()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))) || !Owning_Player_Character)
	return;

	/* MaxWalkSpeed = 500.f;
	MaxAcceleration = 1000.f;
	Owning_Player_Character->Set_Is_Jogging(true); */

	Server_Execute_Start_Running();
}

void UCustom_Movement_Component::Execute_Stop_Running()
{
	/* MaxWalkSpeed = 240.f;
	MaxAcceleration = 270.f;
	Owning_Player_Character->Set_Is_Jogging(false); */

	Server_Execute_Stop_Running();
}

void UCustom_Movement_Component::Execute_Parkour_Jump()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))) || !Owning_Player_Character)
	return;

	//Check to see if the character is on the ground.
	if(bIs_On_Ground)
	{
		//Check to see if the character is stationary.
		if(Ground_Speed == 0.f)
		{
			//Check to make sure both feet have contact with the ground.
			if(Validate_Foot_Contact_With_Ground(true) && Validate_Foot_Contact_With_Ground(false))
			{
				Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.Up"))));
			}
		}

		//Check to see if the character is walking.
		else if(Ground_Speed > 0.f && !Owning_Player_Character->Get_Is_Jogging())
		{
			//Check to see if the left foot has contact with the gorund.
			if(Validate_Foot_Contact_With_Ground(true))
			{
				//Check to see if there is a valid location for the character to land when jumping. The result of this check will determine how the character lands when jumping.
				if(Validate_Jumping_Destination_Ground_Surface(true))
				{
					bAccurate_Jump_Destination_Found = true;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}
				
				else
				{
					/* bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = true; */

					bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}

				
				if(!Wall_Top_Result.bBlockingHit)
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Accurate.Jump.Start.L"))));
				}
				
				else
				{
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					const FHitResult Not_Needed_Here{};
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Accurate.Jump.Start.L.Warp"))));
				}

			}

			//Check to see if the right foot has contact with the gorund.
			else if(Validate_Foot_Contact_With_Ground(false))
			{
				//Check to see if there is a valid location for the character to land when jumping. The result of this check will determine how the character lands when jumping.
				if(Validate_Jumping_Destination_Ground_Surface(true))
				{
					bAccurate_Jump_Destination_Found = true;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}
			
				else
				{
					/* bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = true; */

					bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}

				
				if(!Wall_Top_Result.bBlockingHit)
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Accurate.Jump.Start.R"))));
				}
				
				else
				{
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					const FHitResult Not_Needed_Here{};
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
					
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Accurate.Jump.Start.R.Warp"))));
				}
			}
		}

		//Check to see if the character is running.
		else if(Owning_Player_Character->Get_Is_Jogging())
		{
			//Check to see if the left foot has contact with the gorund.
			if(Validate_Foot_Contact_With_Ground(true))
			{
				//Check to see if there is a valid location for the character to land when jumping. The result of this check will determine how the character lands when jumping.
				if(Validate_Jumping_Destination_Ground_Surface(false))
				{
					bAccurate_Jump_Destination_Found = true;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}
				
				else
				{
					/* bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = true; */

					bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}
				

				if(!Wall_Top_Result.bBlockingHit)
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.Front.L.Start"))));
				}
				
				else
				{
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					const FHitResult Not_Needed_Here{};
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
					
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.Front.L.Start.Warp"))));
				}
				
			}

			//Check to see if the right foot has contact with the gorund.
			else if(Validate_Foot_Contact_With_Ground(false))
			{
				if(Validate_Jumping_Destination_Ground_Surface(false))
				{
					bAccurate_Jump_Destination_Found = true;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}
				
				else
				{
					/* bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = true; */

					bAccurate_Jump_Destination_Found = false;
					bParkour_Action_Jump_Finish_On_Blending_Out = false;
				}
				
				
				if(!Wall_Top_Result.bBlockingHit)
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.Front.R.Start"))));
				}
				
				else
				{
					//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
					//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
					//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
					const FHitResult Not_Needed_Here{};
					Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
					
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.Front.R.Start.Warp"))));
				}
			}
		}
	}
}

void UCustom_Movement_Component::Execute_Balance_Traversal(ABalance_Traversal_Actor* Balance_Traversal_Actor_Reference)
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	return;
	
	Balance_Traversal_Actor = Balance_Traversal_Actor_Reference;
	
	if(Owning_Player_Character && Balance_Traversal_Actor && bIs_On_Ground)
	{
		Debug::Print("Balance_Traversal_Possible", FColor::Green, 70);

		//Make sure the character is attempting to Execute_Balance_Traversal from the approprate angle.
		const double Dot_Product{FVector::DotProduct(Owning_Player_Character->GetActorForwardVector(),
		Balance_Traversal_Actor->GetActorForwardVector())};

		Debug::Print("Balance_Traversal_Actor_To_Character_Dot_Product:" + FString::SanitizeFloat(Dot_Product), FColor::Yellow, 71);
		
		if(Dot_Product >= .3)
		{
			Debug::Print("Balance_Traversal_Initialized", FColor::Green, 72);
			
			Detect_Balance_Traversal_Actors(Balance_Traversal_Actor->Get_Balance_Traversal_Actor_Forward_Vector(), Balance_Traversal_Actor->GetActorRotation(), 15, 10);

			if(Detect_Balance_Traversal_Actors_Hit_Traces.Num() > 0)
			{
				Analyze_Detect_Balance_Traversal_Actors_Hit_Traces_For_Best_Hit();
				Detect_Balance_Traversal_Actors_Hit_Traces.Empty();
			}

			if(Balance_Traversal_Actors_Best_Hit.bBlockingHit)
			{
				if(Validate_Balance_Traversal_Location() && Owning_Player_Character->Get_Is_Jogging())
				{
					Debug::Print("Character_Is_Executing_Balance_Traversal", FColor::Green, 100);
				
					if(Validate_Foot_Contact_With_Ground(true))
					{
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
						
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.One.L"))));
					}

					else if(Validate_Foot_Contact_With_Ground(false))
					{
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
						
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Jump.One.R"))));
					}
				}
			}
		}

		else
		{
			Debug::Print("Balance_Traversal_Not_Initialized", FColor::Red, 72);
		}
	}

	/* else
	{
		Debug::Print("Balance_Traversal_Not_Possible", FColor::Red, 70);
		return;
	} */

}

void UCustom_Movement_Component::Parkour_Balance_Walk_Handle_Balance_Walking_Movement()
{
	/*This function handles calling the functions which will validate whether the character can balance walk in soley the forward direction of the input which is passed into the global double variable "Forward_Backward_Movement_Value". 
	within the function "&UCustom_Movement_Component::Add_Movement_Input".*/
	
	//Store the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value". This value will be used to check if the input to move the character to the right or left
	//within the function "&UCustom_Movement_Component::Add_Movement_Input" is above the threshold to accept input.
	const double Forward_Backward_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Forward_Backward_Movement_Value)};
	const double Right_Left_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Right_Left_Movement_Value)};

	//Check to see if the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value" is above the threshold to allow shimmying movement.
	//If the check is passed, check to see if the value is above or below 0. If the value is above 0 the character is moving to the right, if the value is below 0 the character is 
	//moving to the left.
	if(Forward_Backward_Movement_Value_Absolute_Value > .7 || Right_Left_Movement_Value_Absolute_Value > .7)
	{
		if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))) ||
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))) ||
		Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value), FColor::MakeRandomColor(), 7);

			/* Handle Moving Character Forward */

			//These variables are filled with values within the function "Parkour_Balance_Walk_Detect_Balance_Surface" (they are passsed in as references via the input arguments in said function). 
			//The FHitResult stored within the variables "Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result" and "Parkour_Balance_Walk_Balance_Surface_Top_Result" are be used to determine whether there is a Balance Surface 
			//in front of the character which the character can walk forward on via the functions "Parkour_Balance_Walk_State_Are_There_Obstacles_In_Front_Of_Feet"  and
			//"Parkour_Balance_Walk_State_Are_There_Obstacles_In_Front_Of_Body". Said FHitResult variables are passed in as const references via the input arguments (they are filled with data 
			//within the function "Parkour_Balance_Walk_Detect_Balance_Surface").
			FHitResult Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result{};
			FHitResult Parkour_Balance_Walk_Balance_Surface_Top_Result{};
		

			//This function executes an algorithm which determines whether there is a Balance Surface in front of the character which the character can walk on during Balance Walk movememnt. 
			//Also, the determination of if the Balance Walk surface in the direction in which the character is moving is too high or low to walk across (too big of a stair to step up on). 
			//For example when the character is moving forward and the surface which the character is walking across has a step that is too high for the character to step up on aka bStartPenetrating is true, 
			//(calculated by the maximum number of iterations in the inner for loop), then "Parkour_Balance_Walk_Balance_Surface_Top_Result" will not be calculated and "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" 
			//along with "return" will be called within the function in the appropriate location. Same goes for the "Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result". If there is no blocking hit or "bStartPenetrating" is true 
			//"Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" along with "return" will be called within the function in the appropriate location within the function.
			if(!Parkour_Balance_Walk_Detect_Balance_Surface(Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result, Parkour_Balance_Walk_Balance_Surface_Top_Result))
			{
				Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
				return;
			}
		
			//The check Parkour_Balance_Walk_State_Are_There_Obstacles_In_Front_Of_Feet determines whether there are obstacles in front of the character's feet which should stop the character from Balance Walking forward any furhther.
			//The function "Parkour_Balance_Walk_State_Are_There_Obstacles_In_Front_Of_Feet" uses "Parkour_Balance_Walk_Balance_Surface_Top_Result.ImpactPoint" as the starting location (const reference input parameter)
			//of the line traces executed within said function. 

			//The check Parkour_Balance_Walk_State_Are_There_Obstacles_In_Front_Of_Body determines whether there is a obstacle in front of the character's body which should deter the character from Balance Walking forward any further. 
			//The starting location (const reference input parameter) of the capsule trace executed within the bool function "Parkour_Balance_Walk_State_Are_There_Obstacles_In_Front_Of_Body" is the impact point of 
			//"Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result".
			else if(/* Parkour_Balance_Walk_Are_There_Obstacles_In_Front_Of_Feet(Parkour_Balance_Walk_Balance_Surface_Top_Result) || */ Parkour_Balance_Walk_Are_There_Obstacles_In_Front_Of_Body(Parkour_Balance_Walk_Balance_Surface_Top_Result))
			{
				Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
				return;
			}

			//If this line of code is reached then character's movement has been validated and in result "Calculate_And_Move_Character_To_New_Balance_Walk_Position" should be called passing in the FHitResults
			//"Parkour_Balance_Walk_Detect_Balance_Surface_Hit_Result" and "Parkour_Balance_Walk_Balance_Surface_Top_Result" into the input argument. This function uses the location of said FHitResults to interpolate the
			//character to their location. Considering the locations of said FHitResults are always right in fornt of the arrow actor (offset down to the level of where the character's feet are. The character will always be "chasing" 
			//the location of the FHitResult "Parkour_Balance_Walk_Balance_Surface_Top_Result" to interpolate the character to its location (causeing an infinite interpolation to said location).
			else
			{
				Calculate_And_Move_Character_To_New_Balance_Walk_Position(Parkour_Balance_Walk_Balance_Surface_Top_Result);
			}
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))))
		{
			/* Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))));
			Debug::Print("Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7); */

			//Set Parkour_Action to "Parkour.Action.Balance.90.L"
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Walk.90.L"))));
		}
		
		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))))
		{
			/* Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))));
			Debug::Print("Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7); */

			//Set Parkour_Action to "Parkour.Action.Balance.90.R"
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Walk.90.R"))));
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))))
		{
			/* Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value), FColor::MakeRandomColor(), 7); */

			//Set Parkour_Action to "Parkour.Action.Balance.180"
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Walk.180"))));
		}

		else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))) || 
				Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))) || 
				Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))))
		{
			Set_Parkour_Direction(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.None"))));
			Debug::Print("Forward_Backward_Movement_Value: " + FString::FromInt(Forward_Backward_Movement_Value) + " Right_Left_Movement_Value: " + FString::FromInt(Right_Left_Movement_Value), FColor::MakeRandomColor(), 7);
		}
	}

	//If "Right_Left_Movement_Value_Absolute_Value" is not above .7 then this means the minimum threshold to activate "Balance_Walking_Movement" has not been met by the input from the player's
	//controller. In this case, "Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables" and "return" should be called.
	else
	{
		Stop_Parkour_Movement_Immediately_And_Reset_Movement_Input_Variables();
		return;
	}

}

void UCustom_Movement_Component::Execute_Balance_Walk_Hop()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	return;
	
	Decide_Parkour_Action();
}

void UCustom_Movement_Component::Calculate_And_Move_Character_To_New_Balance_Walk_Position(const FHitResult& Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference)
{
	/*This function is called calculates the location to interpolate the character to and passes in the said location into the function "&UCustom_Movement_Component::Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings"
	as an input argument.*/

	//This value is used to offset the local FVector "Move_Character_To_This_Location" up 94 units. This is because when the actor's location is obtained the vector that is returned is set in the absolue middle of the actor (around the hips).
	//Therefore the height of the FVector must be increased by the half height of the actors capsule component.
	const float Value_To_Offset_Character_Up{94.f};
	
	//Custom FVector to pass into the function "&UCustom_Movement_Component::Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings" as an input argument. This will be the location to interpolate 
	//the character to as long as there is input into the player controller and the validation and checks performed in the function "&UCustom_Movement_Component::Parkour_Balance_Walk_Handle_Balance_Walking_Movement"
	//are successful. 
	const FVector& Move_Character_To_This_Location(FVector(Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.ImpactPoint.X, 
														   Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.ImpactPoint.Y, 
														   Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference.ImpactPoint.Z + 94));
	
	//Add a rotaion of 90 degrees to the global FRotator "Reversed_Front_Wall_Normal_Z" so that the charater is always facing forwards, heading in the same direction of the surface in which it is walking on.
	const FRotator Custom_Rotation{Reversed_Front_Wall_Normal_Z.Pitch, Add_Rotator(Reversed_Front_Wall_Normal_Z, -90).Yaw, Reversed_Front_Wall_Normal_Z.Roll};
	Reversed_Front_Wall_Normal_Z = Custom_Rotation;

	/* //Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
	//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
	//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
	const FHitResult Not_Needed_Here{};
	Set_Network_Variables(Parkour_Balance_Walk_Balance_Surface_Top_Result_Reference, Reversed_Front_Wall_Normal_Z, Not_Needed_Here); */

	//This function uses the location which is calculated above (using the variable from the FHitResult input argument "Parkour_Climbing_Detect_Wall_Hit_Result") to interpolate the character to said 
	//FVector. Considering the locations of the custom FVector will always be updating due to it being dependant on the input argument variable "Parkour_Climbing_Detect_Wall_Hit_Result",  
	//the character will always be "chasing" the location to interpolate its location to causeing an infinite interpolation. This is because the impact point of the input argument 
	//"Parkour_Climbing_Detect_Wall_Hit_Result" is offset to the right or left side of the arrow actor (the arrow actor is just above the character) depending on whether the character is moving
	//to the right or left.
	Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings(Move_Character_To_This_Location, Reversed_Front_Wall_Normal_Z);
}

void UCustom_Movement_Component::Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	if(!Owning_Player_Character)
	return;
	
	//Get the character's location.
	const FVector Characters_Location{Owning_Player_Character->GetActorLocation()};
	
	//Get DeltaTime
	const double DeltaTime{UGameplayStatics::GetWorldDeltaSeconds(this)};

	//Develop double variables for each of the axis in which the character will be interpolated to.
	const double Interpolate_Character_To_This_X_Axis{UKismetMathLibrary::FInterpTo(Characters_Location.X, Location_To_Move_Character.X, DeltaTime, 3.f)};
	const double Interpolate_Character_To_This_Y_Axis{UKismetMathLibrary::FInterpTo(Characters_Location.Y, Location_To_Move_Character.Y, DeltaTime, 3.f)};
	const double Interpolate_Character_To_This_Z_Axis{UKismetMathLibrary::FInterpTo(Characters_Location.Z, Location_To_Move_Character.Z, DeltaTime, 1.f)};

	//Combine the double variables above into a single FVector struct.
	const FVector Interpolated_Location_To_Move_Character{Interpolate_Character_To_This_X_Axis, Interpolate_Character_To_This_Y_Axis, Interpolate_Character_To_This_Z_Axis};

	//Call the function "SetActorLocationAndRotation" and pass in the calculated results.
	Owning_Player_Character->SetActorLocationAndRotation(Interpolated_Location_To_Move_Character, Rotation_For_Character_To_Face);

	if(!Owning_Player_Character->HasAuthority())
	{
		Server_Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings(Interpolated_Location_To_Move_Character, Rotation_For_Character_To_Face);
	}
}

bool UCustom_Movement_Component::Validate_Balance_Walk_Hop()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	return false;
	
	Balance_Walk_Hop_Detect_Wall();

	if(Initial_Front_Wall_Hit_Result.bBlockingHit)
	{
		Grid_Scan_For_Hit_Results(Initial_Front_Wall_Hit_Result.ImpactPoint, Reverse_Wall_Normal_Rotation_Z(Initial_Front_Wall_Hit_Result.ImpactNormal), 7, 8);
	}

	//"Grid_Scan_Hit_Traces" array is filled by the function call "Grid_Scan_For_Hit_Results()". 
	if(Grid_Scan_Hit_Traces.Num() != 0)
	{
		//This function analyzes the FHitResults stored in the array "Grid_Scan_Hit_Traces" for the line traces which are just under the top edge on the front side of the wall.
		//Said line traces are stored in the array "Front_Wall_Top_Edge_Traces".
		Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits();
		
		//Empty the array because the information in it has been analyzed in "&UCustom_Movement_Component::Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()".
		//It needs to be empty so it can be filled with new information the next time it needs to be used.
		Grid_Scan_Hit_Traces.Empty();
	}

	//Front_Wall_Top_Edge_Traces are filled by the function call "Analyze_Grid_Scan_For_Front_Wall_Top_Edge_Hits()".
	if(Front_Wall_Top_Edge_Traces.Num() != 0)
	{
		//This function analyzes the line traces stored in the array "Front_Wall_Top_Edge_Traces" for the line trace which is closes to the character's current location.
		//Said line trace is stored in the global variable "Front_Wall_Top_Edge_Best_Hit{}".
		Realize_Front_Wall_Top_Edge_Best_Hit();

		//Empty the array because the information in it has been analyzed in "&UCustom_Movement_Component::Realize_Front_Wall_Top_Edge_Best_Hit()".
		//It needs to be empty so it can be filled with new information the next time it needs to be used.
		Front_Wall_Top_Edge_Traces.Empty();
	}

	//Global FhitResult variable "Front_Wall_Top_Edge_Best_Hit" is filled with the line trace that is just under the top edge on the front wall in the function call "Realize_Front_Wall_Top_Edge_Best_Hit()".
	//This check is to make sure said FHitResult does indeed have a blocking hit and no initial overlap is active. This FHitResult is used to analyze the top surface of the wall which in result will enable
	//the calculation of the location which the character will land on when vaulting.
	if(Front_Wall_Top_Edge_Best_Hit.bBlockingHit && !Front_Wall_Top_Edge_Best_Hit.bStartPenetrating)
	{
		if(Balance_Walk_Hop_Calculate_Wall_Top_Surface())
		{
			Debug::Print("Execute_Balance_Walk_Automatic_Hop_Validated", FColor::Green, 81);
			
			if(Validate_Balance_Walk_Automatic_Hop_Location())
			{
				Debug::Print("Executing_Balance_Walk_Automatic_Hop", FColor::Green, 85);

				Wall_Top_Result = Balance_Walk_Automatic_Hop_Top_Result;
				Reversed_Front_Wall_Normal_Z = Reverse_Wall_Normal_Rotation_Z(Front_Wall_Top_Edge_Best_Hit.ImpactNormal);

				return true;
			}

			else
			{
				Debug::Print("Unable_To_Execute_Balance_Walk_Automatic_Hop", FColor::Red, 85);
				return false;
			}
		}

		else
		{
			Debug::Print("Execute_Balance_Walk_Automatic_Hop_Not_Validated", FColor::Red, 81);
			return false;
		}
	}

	else
	{
		return false;
	}
	
	//The following line of code is to meet the return requirement of this function type.
	return false;
}

void UCustom_Movement_Component::Execute_Exit_Balance_Traversal()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	return;

	Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Front"))));
}

void UCustom_Movement_Component::Execute_Balance_Drop_Hanging()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	return;

	const int Balance_Drop_Hanging_Random_Number{UKismetMathLibrary::RandomIntegerInRange(1, 2)};

	if(Balance_Drop_Hanging_Random_Number == 1)
	{
		if(Validate_Balance_Drop_Hanging(true))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Drop.L.Hanging"))));
		}

		else if(Validate_Balance_Drop_Hanging(false))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Drop.R.Hanging"))));
		}

		else
		{
			Debug::Print("Executing_Balance_Drop_Hanging Not_Possible", FColor::Red, 75);
		}
	}

	else if(Balance_Drop_Hanging_Random_Number == 2)
	{
		if(Validate_Balance_Drop_Hanging(false))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Drop.R.Hanging"))));
		}

		else if(Validate_Balance_Drop_Hanging(true))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Balance.Drop.L.Hanging"))));
		}

		else
		{
			Debug::Print("Executing_Balance_Drop_Hanging Not_Possible", FColor::Red, 75);
		}
	}

}

void UCustom_Movement_Component::Execute_Free_Hang_To_Balanced_Walk()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))) &&
	Parkour_Climb_Style != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
	return;
	
	const int Free_Hang_To_Balanced_Walk_Random_Number{UKismetMathLibrary::RandomIntegerInRange(1, 2)};

	if(Free_Hang_To_Balanced_Walk_Random_Number == 1)
	{
		if(Validate_Free_Hang_To_Balanced_Walk(true))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.L"))));
		}

		else if(Validate_Free_Hang_To_Balanced_Walk(false))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.R"))));
		}

		else
		{
			Debug::Print("Executing_Free_Hang_To_Balanced_Walk Not_Possible", FColor::Red, 75);
		}
	}

	else if(Free_Hang_To_Balanced_Walk_Random_Number == 2)
	{
		if(Validate_Free_Hang_To_Balanced_Walk(false))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.R"))));
		}

		else if(Validate_Free_Hang_To_Balanced_Walk(true))
		{
			//Call the function "&UCustom_Movement_Component::Decide_Climb_Style" to determine which "Parkour_Climb_Style" to set the character to. This function uses an offset location units below the 
			//Wall Top Result passed into it to generate ray casts at the level of the characters feet when said character is in the braced climb style. If there is no blocking hit on the ray cast
			//then the characters "Parkour_Climb_Style" will be set to "Parkour.Climb.Style.FreeHang".
			Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
			
			//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
			//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
			//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
			const FHitResult Not_Needed_Here{};
			Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
			
			Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.L"))));
		}

		else
		{
			Debug::Print("Executing_Free_Hang_To_Balanced_Walk Not_Possible", FColor::Red, 75);
		}
	}

}

void UCustom_Movement_Component::Execute_Wall_Vault(AWall_Vault_Actor* Wall_Vault_Actor_Reference)
{
	if(Wall_Vault_Actor_Reference)
	{
		/*Setting the global pointer of type AWall_Vault_Actor "Wall_Vault_Actor" to equal the save value address of the pointer of input argument "Wall_Vault_Actor_Reference" will allow the character to perform
		Wall_Vault actions. This pointer is checked to see if it is valid within &UCustom_Movement_Component::Execute_Parkour_Action. If it is valid, &UCustom_Movement_Component::Decide_Wall_Vault_Parkour_Action
		will be called which will ultimately decide which "Wall_Vault" action the character should execute.*/
		
		Wall_Vault_Actor = Wall_Vault_Actor_Reference;
		Debug::Print("Wall_Vault_Actor_Is_Valid_Proceeding_To_Execute_Wall_Vault", FColor::Green, 75);
	}

	else
	{
		Wall_Vault_Actor = nullptr;
		Debug::Print("Wall_Vault_Actor_Is_Not_Valid", FColor::Red, 75);
	}
}

void UCustom_Movement_Component::Decide_Wall_Vault_Parkour_Action()
{
	if(!Wall_Vault_Actor)
	return;
	
	Debug::Print("Decide_Wall_Vault_Parkour_Action_Called", FColor::Green, 77);
	
	if(Wall_Top_Result.bBlockingHit)
	{
		if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
		{
			if(bIs_On_Ground)
			{
				if(Ground_Speed <= 250.f)
				{
					if(Wall_Vault_Actor->Get_bEnable_Wall_Under_Bar())
					{
						Debug::Print("Character_Needs_More_Speed_For_Running_Wall_Under_Bar", FColor::Red, 78);
					}

					else if(Wall_Vault_Actor->Get_bEnable_Over_Wall_180_Shimmy())
					{
						Debug::Print("Character_Needs_More_Speed_For_Over_180_Shimmy", FColor::Red, 78);
					}

					else
					{
						if(Wall_Depth >= 70.f)
						{
							Debug::Print("Idle_Wall_Vault_On", FColor::MakeRandomColor(), 78);

							//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
							//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
							//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
							const FHitResult Not_Needed_Here{};
							Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
							
							Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Idle.Wall.Vault.On"))));
						}

						else
						{
							if(Owning_Player_Character->Get_Is_Jogging())
							{
								Debug::Print("Idle_Wall_Vault_Over", FColor::MakeRandomColor(), 78);

								//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
								//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
								//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
								const FHitResult Not_Needed_Here{};
								Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
								
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Idle.Wall.Vault.Over"))));
							}
						}
					}
				}
				
				else if(Ground_Speed > 250.f)
				{
					if(Wall_Vault_Actor->Get_bEnable_Wall_Under_Bar())
					{
						Debug::Print("Running_Wall_Under_Bar", FColor::MakeRandomColor(), 78);
						
						//Set_Parkour_Action and use custom Wall_Top_Result from "AWall_Vault_Actor".
						Wall_Top_Result = Wall_Vault_Actor->Get_Wall_Vault_Actor_Wall_Top_Result();

						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.Under.Bar"))));
					}

					else if(Wall_Vault_Actor->Get_bEnable_Over_Wall_180_Shimmy())
					{
						Debug::Print("Over_Wall_180_Shimmy", FColor::MakeRandomColor(), 78);
						
						Reversed_Front_Wall_Normal_Z = FRotator(Reversed_Front_Wall_Normal_Z.Pitch, Reversed_Front_Wall_Normal_Z.Yaw - 180.f, Reversed_Front_Wall_Normal_Z.Roll);

						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
						
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.Over.180.Shimmy"))));
					}
					
					else
					{
						if(Wall_Depth >= 70.f)
						{
							Debug::Print("Running_Wall_Vault_On", FColor::MakeRandomColor(), 78);

							//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
							//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
							//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
							const FHitResult Not_Needed_Here{};
							Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

							Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.On"))));
						}

						else
						{
							if(Owning_Player_Character->Get_Is_Jogging())
							{
								Debug::Print("Running_Wall_Vault_Over", FColor::MakeRandomColor(), 78);

								//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
								//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
								//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
								const FHitResult Not_Needed_Here{};
								Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
								
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.Over"))));
							}

							else
							{
								Debug::Print("Running_Wall_Vault_Over_180_Shimmy", FColor::MakeRandomColor(), 78);

								//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
								//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
								//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
								const FHitResult Not_Needed_Here{};
								Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Running.Wall.Vault.Over.180.Shimmy"))));
							}
						}
					}
				}

			}

			else
			{
				if(Wall_Height >= 140 && Wall_Height <= 280 && Validate_Can_Start_Shimmying_While_Airborne())
				{								
												// negative value in "Air_Speed" means character is falling  //Ground_Speed >= 700.f || Air_Speed <= -1000.f || Air_Speed <= 20.f
					if(Owning_Player_Character->Get_Is_Jogging() && (Air_Speed >= -200.f || Air_Speed <= 200.f))
					{
						Debug::Print("Parkour_Dynamic_Airorne_Climb", FColor::MakeRandomColor(), 7);

						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						{
							if(Air_Speed <= -500.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))));
							}

							else if(Air_Speed >= -499.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));
							}
						}
						
						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
						{
							if(Validate_Can_Fly_Hanging_Jump())
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb.Hanging.Jump"))));
							}
							
							else
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
							}
						}
					}

					else
					{
						Debug::Print("Parkour_Airorne_Climb", FColor::MakeRandomColor(), 7);
						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));

						else
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
					}

				}

				else
				{
					Debug::Print("No_Action", FColor::MakeRandomColor(), 7);
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
				}

			}
		}

	}

}

void UCustom_Movement_Component::Execute_Tic_Tac(ATic_Tac_Actor* Tic_Tac_Actor_Reference, const int& Tic_Tac_Area_Box_ID)
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	return;
	
	//While facing the "Tic_Tac" actor from it's operational side "Tic_Tac_Area_Box_ID == 2" is the left Area_Box and "Tic_Tac_Area_Box_ID == 3" is the right Area_Box.
	if(Tic_Tac_Actor_Reference && (Tic_Tac_Area_Box_ID == 2 || Tic_Tac_Area_Box_ID == 3))
	{
		//Make sure the character is attempting to perform a Tic_Tac action from the approprate angle.
		const double Dot_Product{FVector::DotProduct(-Owning_Player_Character->GetActorForwardVector(),
		Tic_Tac_Actor_Reference->GetActorForwardVector())};

		if(Dot_Product > 0.3f)
		{
			/* Setting the global pointer of type ATic_Tac_Actor "Tic_Tac_Actor" to equal the save value address of the pointer of input argument "Tic_Tac_Actor_Reference" will allow the character to perform
			Tic_Tac actions. This pointer is checked to see if it is valid within &UCustom_Movement_Component::Execute_Parkour_Action. If it is valid, &UCustom_Movement_Component::Decide_Tic_Tac_Parkour_Action
			will be called which will ultimately decide which "Tic_Tac" action the character should execute. */

			Tic_Tac_Actor = Tic_Tac_Actor_Reference;
			Tic_Tac_Actor_Area_Box_ID = Tic_Tac_Area_Box_ID;
			Debug::Print("Tic_Tac_Actor & Character DOT Product: " + FString::SanitizeFloat(Dot_Product), FColor::Green, 74);
			Debug::Print("Tic_Tac_Actor_Is_Valid_Proceeding_To_Execute_Tic_Tac, ID: " + FString::FromInt(Tic_Tac_Actor_Area_Box_ID), FColor::Green, 75);
		}
		
		else
		{
			Tic_Tac_Actor = nullptr;
			Tic_Tac_Actor_Area_Box_ID = 0;
			Debug::Print("Tic_Tac_Actor & Character DOT Product: " + FString::SanitizeFloat(Dot_Product), FColor::Red, 74);
		}

	}

	else
	{
		
		Tic_Tac_Actor = nullptr;
		Tic_Tac_Actor_Area_Box_ID = 0;
		Debug::Print("Tic_Tac_Actor_Is_Not_Valid " + FString::FromInt(Tic_Tac_Actor_Area_Box_ID), FColor::Red, 75);
	}
}

void UCustom_Movement_Component::Decide_Tic_Tac_Parkour_Action()
{
	Debug::Print("Decide_Tic_Tac_Parkour_Action_Called", FColor::Green, 77);
	
	if(!Tic_Tac_Actor)
	{
		return;
	}
	
	else if(Wall_Top_Result.bBlockingHit)
	{
		if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
		{
			if(bIs_On_Ground)
			{
				const bool bCan_Tic_Tac_Over_Front_Wall{Tic_Tac_Actor->Get_bCan_Tic_Tac_Over_Front_Wall()};
				
				//Character can perform "Tic_Tac" action which goes over the front facing "Tic_Tac_Actor" wall.
				if(bCan_Tic_Tac_Over_Front_Wall)
				{
					//Overlapping wall on left side.
					if(Tic_Tac_Actor_Area_Box_ID == 2) 
					{
						if(Ground_Speed > 250.f)
						{
							if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 180.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over front wall from left side.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Front.Wall")))); 
								}
							}

							else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
							Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(Get_Controller_Direction(), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 90.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over right wall from left wall.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Right.Wall")))); 
								}	
							}
						}
					}

					//Overlapping wall on right side.
					else if(Tic_Tac_Actor_Area_Box_ID == 3)
					{
						if(Ground_Speed > 250.f)
						{
							if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 180.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over front wall from right side.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Front.Wall")))); 
								}	
							}

							else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))) || 
							Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(Get_Controller_Direction(), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw + 90.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over left wall from right wall.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Left.Wall")))); 
								}	
							}
						}
					}
				}

				//Character can no perform "Tic_Tac" action which goes over the front facing "Tic_Tac_Actor" wall and in result the charater has to perform a "Tic_Tac" action which lands on the front
				//facing "Tic_Tac_Actor" wall.
				else if(!bCan_Tic_Tac_Over_Front_Wall)
				{
					//Overlapping wall on left side.
					if(Tic_Tac_Actor_Area_Box_ID == 2) 
					{
						if(Ground_Speed < 250.f)
						{
							if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 180.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									//Jump on front wall from left side and transition into idle.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Idle")))); 
								}
							}

							else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
							Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(Get_Controller_Direction(), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 90.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over right wall from left wall.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Right.Wall"))));
								}
							}
						}

						else if(Ground_Speed >= 250.f)
						{
							if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 180.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump on front wall from left side and transition into running.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Run")))); 
								}
							}

							else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
							Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(Get_Controller_Direction(), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 90.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over right wall from left wall
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Right.Wall"))));
								}
							}
						}
					}

					//Overlapping wall on right side.
					else if(Tic_Tac_Actor_Area_Box_ID == 3)
					{
						if(Ground_Speed < 250.f)
						{
							if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 180.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump on front wall from right side and transition into idle.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Idle")))); 
								}
							}

							else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Left"))) || 
							Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Left"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(Get_Controller_Direction(), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw + 90.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over left wall from right wall.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Right.Wall"))));
								}
							}
						}
						
						else if(Ground_Speed >= 250.f)
						{
							if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward"))), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw - 180.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump on front wall from right side and transition into running.
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Run")))); 
								}
							}

							else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Right"))) || 
							Get_Controller_Direction() == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Forward.Right"))))
							{
								if(Validate_Tic_Tac_Destination_And_Lack_Of_Obstacles(Get_Controller_Direction(), bCan_Tic_Tac_Over_Front_Wall))
								{
									const FRotator Direction_For_Character_To_Face{FRotator(Tic_Tac_Actor->GetActorRotation().Pitch, Tic_Tac_Actor->GetActorRotation().Yaw + 90.f, Tic_Tac_Actor->GetActorRotation().Roll)};
									
									Reversed_Front_Wall_Normal_Z = Direction_For_Character_To_Face;
									
									//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
									//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
									//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
									const FHitResult Not_Needed_Here{};
									Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);
									
									
									//Jump over left wall from right wall
									Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Left.Wall")))); 
								}
							}
						}
					}
				}
			}

			else
			{
				if(Wall_Height >= 140 && Wall_Height <= 280 && Validate_Can_Start_Shimmying_While_Airborne())
				{								
												// negative value in "Air_Speed" means character is falling  //Ground_Speed >= 700.f || Air_Speed <= -1000.f || Air_Speed <= 20.f
					if(Owning_Player_Character->Get_Is_Jogging() && (Air_Speed >= -200.f || Air_Speed <= 200.f))
					{
						Debug::Print("Parkour_Dynamic_Airorne_Climb", FColor::MakeRandomColor(), 7);

						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						{
							if(Air_Speed <= -500.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))));
							}

							else if(Air_Speed >= -499.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));
							}
						}
						
						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
						{
							if(Validate_Can_Fly_Hanging_Jump())
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb.Hanging.Jump"))));
							}
							
							else
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
							}
						}
					}

					else
					{
						Debug::Print("Parkour_Airorne_Climb", FColor::MakeRandomColor(), 7);
						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
						// The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						// FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						// "&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						// after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						// a location to begin the next sequence of ray casts.
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						//Set the global FHitResult Wall_Top_Result and the global FRotator Reversed_Fron_Wall_Normal_Z on the server and on all clients using the function &UCustom_Movement_Component::Set_Network_Variables.
						//Said function takes the variables which need to be set for network replication via the input arguments. If a variable is not needed for replication when this function is called a local variable which matches 
						//the repsective variable type (that's not needed) is declared with the name "Not_Needed_Here" and is called in in the appropriate location in the input field.
						const FHitResult Not_Needed_Here{};
						Set_Network_Variables(Wall_Top_Result, Reversed_Front_Wall_Normal_Z, Not_Needed_Here);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));

						else
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
					}

				}

				else
				{
					Debug::Print("No_Action", FColor::MakeRandomColor(), 7);
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
				}

			}

		}
	}

	else
	{
		
		return;
	}

}


#pragma region Set_Network_Variables

void UCustom_Movement_Component::Set_Network_Wall_Calculations(const double& Network_Wall_Height, const double& Network_Wall_Depth, const double& Network_Vault_Height)
{
	Server_Set_Network_Wall_Calculations(Network_Wall_Height, Network_Wall_Depth, Network_Vault_Height);
}

void UCustom_Movement_Component::Set_Network_Variables(const FHitResult& Network_Wall_Top_Result, const FRotator& Network_Reversed_Front_Wall_Normal_Z, const FHitResult& Custom_Wall_Pipe_Forward_Vector)
{
	/* //Set the global FHitResult "Wall_Top_Result" to equal the global FHitResult "Network_Wall_Top_Result". "Network_Wall_Top_Result" is set within the function 
	//&UCustom_Movement_Component::Realize_Wall_Pipe_Surfaces.
	Wall_Top_Result = Network_Wall_Top_Result;
		
	//Set the value within the global variable "Reversed_Front_Wall_Normal_Z" to eqaul the the  gloabl "Wall_Pipe_Forward_Vector" (using the helper function "Reverse_Wall_Normal_Rotation_Z()")
	//which is set within /&UCustom_Movement_Component::Realize_Wall_Pipe_Surfaces.
	Reversed_Front_Wall_Normal_Z = Network_Reversed_Front_Wall_Normal_Z;

	Initialize_Parkour_IK_Limbs_Hit_Result = Network_Wall_Top_Result;

	Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result = Custom_Wall_Pipe_Forward_Vector; */

	Server_Set_Network_Variables(Network_Wall_Top_Result, Network_Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Forward_Vector);
}

void UCustom_Movement_Component::Set_Network_Move_Character_To_Front_Of_Pipe(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	Server_Move_Character_To_Front_Of_Pipe(Location_To_Move_Character, Rotation_For_Character_To_Face);
}


#pragma endregion

#pragma endregion

#pragma region Network


#pragma region Set_Network_Variables

void UCustom_Movement_Component::Server_Set_Add_Movement_Input_Variables_Implementation(const double& Network_Forward_Backward_Movement_Value, const double& Network_Right_Left_Movement_Value)
{
	Forward_Backward_Movement_Value = Network_Forward_Backward_Movement_Value;

	Right_Left_Movement_Value = Network_Right_Left_Movement_Value;
}

void UCustom_Movement_Component::On_Replication_Set_Add_Movement_Input_Variables()
{
	Forward_Backward_Movement_Value = Forward_Backward_Movement_Value;

	Right_Left_Movement_Value = Right_Left_Movement_Value;
}

void UCustom_Movement_Component::Server_Set_Network_Wall_Calculations_Implementation(const double& Network_Wall_Height, const double& Network_Wall_Depth, const double& Network_Vault_Height)
{
	Multicast_Set_Network_Wall_Calculations(Network_Wall_Height, Network_Wall_Depth, Network_Vault_Height);
}

void UCustom_Movement_Component::Multicast_Set_Network_Wall_Calculations_Implementation(const double& Network_Wall_Height, const double& Network_Wall_Depth, const double& Network_Vault_Height)
{
	Wall_Height = Network_Wall_Height;

	Wall_Depth = Network_Wall_Depth;

	Vault_Height = Network_Vault_Height;
}

void UCustom_Movement_Component::Server_Set_Network_Variables_Implementation(const FHitResult& Network_Wall_Top_Result, const FRotator& Network_Reversed_Front_Wall_Normal_Z, const FHitResult& Custom_Wall_Pipe_Forward_Vector)
{
	Multicast_Set_Network_Variables(Network_Wall_Top_Result, Network_Reversed_Front_Wall_Normal_Z, Custom_Wall_Pipe_Forward_Vector);
}

void UCustom_Movement_Component::Multicast_Set_Network_Variables_Implementation(const FHitResult& Network_Wall_Top_Result, const FRotator& Network_Reversed_Front_Wall_Normal_Z, const FHitResult& Custom_Wall_Pipe_Forward_Vector)
{
	//Set the global FHitResult "Wall_Top_Result" to equal the global FHitResult "Network_Wall_Top_Result". "Network_Wall_Top_Result" is set within the function 
	//&UCustom_Movement_Component::Realize_Wall_Pipe_Surfaces.
	Wall_Top_Result = Network_Wall_Top_Result;
		
	//Set the value within the global variable "Reversed_Front_Wall_Normal_Z" to eqaul the the  gloabl "Wall_Pipe_Forward_Vector" (using the helper function "Reverse_Wall_Normal_Rotation_Z()")
	//which is set within /&UCustom_Movement_Component::Realize_Wall_Pipe_Surfaces.
	Reversed_Front_Wall_Normal_Z = Network_Reversed_Front_Wall_Normal_Z;

	Initialize_Parkour_IK_Limbs_Hit_Result = Network_Wall_Top_Result;

	Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result = Custom_Wall_Pipe_Forward_Vector;
}

void UCustom_Movement_Component::Server_Set_bIs_Falling_To_True_Implementation()
{
	bIs_Falling = true;
}

void UCustom_Movement_Component::Server_Set_bIs_Falling_To_False_Implementation()
{
	bIs_Falling = false;
}


#pragma endregion

#pragma region Network_Core

void UCustom_Movement_Component::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UCustom_Movement_Component, Forward_Backward_Movement_Value);

	DOREPLIFETIME(UCustom_Movement_Component, Right_Left_Movement_Value);

	DOREPLIFETIME(UCustom_Movement_Component, Ground_Speed);
	
	DOREPLIFETIME(UCustom_Movement_Component, Air_Speed );


	
	DOREPLIFETIME(UCustom_Movement_Component, Parkour_State);

	DOREPLIFETIME(UCustom_Movement_Component, Parkour_Climb_Style);

	DOREPLIFETIME(UCustom_Movement_Component, Parkour_Wall_Run_Side);

	DOREPLIFETIME(UCustom_Movement_Component, Parkour_Direction);

	DOREPLIFETIME(UCustom_Movement_Component, Parkour_Action);

	
	
	DOREPLIFETIME(UCustom_Movement_Component, Wall_Height);

	DOREPLIFETIME(UCustom_Movement_Component, Wall_Depth);

	DOREPLIFETIME(UCustom_Movement_Component, Vault_Height);


	
	DOREPLIFETIME(UCustom_Movement_Component, Wall_Top_Result);

	DOREPLIFETIME(UCustom_Movement_Component, Reversed_Front_Wall_Normal_Z);
	
	DOREPLIFETIME(UCustom_Movement_Component, Initialize_Parkour_IK_Limbs_Hit_Result);
	
	DOREPLIFETIME(UCustom_Movement_Component, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result);


	DOREPLIFETIME(UCustom_Movement_Component, Random_Montage_To_Play);

	DOREPLIFETIME(UCustom_Movement_Component, Last_Random_Montage_Played);

	DOREPLIFETIME(UCustom_Movement_Component, bIs_Falling);

}

void UCustom_Movement_Component::Server_Execute_Start_Running_Implementation()
{
	Multicast_Execute_Start_Running();
}

void UCustom_Movement_Component::Multicast_Execute_Start_Running_Implementation()
{
	if(Parkour_State != FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))) || !Owning_Player_Character)
	return;

	MaxWalkSpeed = 500.f;
	MaxAcceleration = 1000.f;
	Owning_Player_Character->Set_Is_Jogging(true);
}

void UCustom_Movement_Component::Server_Execute_Stop_Running_Implementation()
{
	Multicast_Execute_Stop_Running();
}

void UCustom_Movement_Component::Multicast_Execute_Stop_Running_Implementation()
{
	if(!Owning_Player_Character)
	return;

	MaxWalkSpeed = 240.f;
	MaxAcceleration = 270.f;
	Owning_Player_Character->Set_Is_Jogging(false);
}

void UCustom_Movement_Component::Server_Set_Parkour_State_Attributes_Implementation(const FGameplayTag& Current_Parkour_State)
{
	Multicast_Set_Parkour_State_Attributes(Current_Parkour_State);
}

void UCustom_Movement_Component::Multicast_Set_Parkour_State_Attributes_Implementation(const FGameplayTag& Current_Parkour_State)
{
	if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
	Parkour_State_Settings(ECollisionEnabled::QueryAndPhysics, EMovementMode::MOVE_Walking, false, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Jump"))))
	Parkour_State_Settings(ECollisionEnabled::QueryAndPhysics, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Mantle"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Vault"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Ready.To.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Run"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Wall.Pipe.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Initialize.Balance.Walk"))))
	Parkour_State_Settings(ECollisionEnabled::NoCollision, EMovementMode::MOVE_Flying, true, true);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Balance.Walk"))))
	Parkour_State_Settings(ECollisionEnabled::QueryOnly, EMovementMode::MOVE_Flying, true, false);

	else if(Current_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Tic.Tac"))))
	Parkour_State_Settings(ECollisionEnabled::QueryAndPhysics, EMovementMode::MOVE_Flying, true, true);

}

void UCustom_Movement_Component::Server_Set_Parkour_State_Implementation(const FGameplayTag& New_Parkour_State)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_State != New_Parkour_State)
	{
		Parkour_State = New_Parkour_State;
		Parkour_Interface->Execute_Set_Parkour_State(Anim_Instance, Parkour_State);
		//Set_Parkour_State_Attributes(Parkour_State);
	}

	else return;
	
	//Multicast_Set_Parkour_State(New_Parkour_State);
}

void UCustom_Movement_Component::On_Replication_Parkour_State()
{
	if(!Parkour_Interface)
	return;
	
	Parkour_Interface->Execute_Set_Parkour_State(Anim_Instance, Parkour_State);
	//Set_Parkour_State_Attributes(Parkour_State);
}

void UCustom_Movement_Component::Multicast_Set_Parkour_State_Implementation(const FGameplayTag& New_Parkour_State)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_State != New_Parkour_State)
	{
		Parkour_State = New_Parkour_State;
		Parkour_Interface->Execute_Set_Parkour_State(Anim_Instance, Parkour_State);
		Set_Parkour_State_Attributes(Parkour_State);
	}

	else return;
}

void UCustom_Movement_Component::Server_Set_Parkour_Climb_Style_Implementation(const FGameplayTag& New_Parkour_Climb_Style)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_Climb_Style != New_Parkour_Climb_Style)
	{
		Parkour_Climb_Style = New_Parkour_Climb_Style;
		Parkour_Interface->Execute_Set_Parkour_Climb_Style(Anim_Instance, Parkour_Climb_Style);
	}
	
	else return;
	
	//Multicast_Set_Parkour_Climb_Style(New_Parkour_Climb_Style);
}

void UCustom_Movement_Component::On_Replication_Parkour_Climb_Style()
{
	if(!Parkour_Interface)
	return;
	
	Parkour_Interface->Execute_Set_Parkour_Climb_Style(Anim_Instance, Parkour_Climb_Style);
}

void UCustom_Movement_Component::Multicast_Set_Parkour_Climb_Style_Implementation(const FGameplayTag& New_Parkour_Climb_Style)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_Climb_Style != New_Parkour_Climb_Style)
	{
		Parkour_Climb_Style = New_Parkour_Climb_Style;
		Parkour_Interface->Execute_Set_Parkour_Climb_Style(Anim_Instance, Parkour_Climb_Style);
	}
	
	else return;
}

void UCustom_Movement_Component::Server_Set_Parkour_Wall_Run_Side_Implementation(const FGameplayTag& New_Wall_Run_Side)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_Wall_Run_Side != New_Wall_Run_Side)
	{
		Parkour_Wall_Run_Side = New_Wall_Run_Side;
		Parkour_Interface->Execute_Set_Parkour_Wall_Run_Side(Anim_Instance, New_Wall_Run_Side);
	}

	else return;
	
	//Multicast_Set_Parkour_Wall_Run_Side(New_Wall_Run_Side);
}

void UCustom_Movement_Component::On_Replication_Parkour_Wall_Run_Side()
{
	if(!Parkour_Interface)
	return;
	
	Parkour_Interface->Execute_Set_Parkour_Wall_Run_Side(Anim_Instance, Parkour_Wall_Run_Side);

}

void UCustom_Movement_Component::Multicast_Set_Parkour_Wall_Run_Side_Implementation(const FGameplayTag& New_Wall_Run_Side)
{
	if(!Parkour_Interface)
	return;
	
	if(Parkour_Wall_Run_Side != New_Wall_Run_Side)
	{
		Parkour_Wall_Run_Side = New_Wall_Run_Side;
		Parkour_Interface->Execute_Set_Parkour_Wall_Run_Side(Anim_Instance, New_Wall_Run_Side);
	}

	else return;
}

void UCustom_Movement_Component::Server_Set_Parkour_Direction_Implementation(const FGameplayTag& New_Parkour_Direction)
{
	if(!Parkour_Interface)
	return;

	if(Parkour_Direction != New_Parkour_Direction)
	{
		Parkour_Direction = New_Parkour_Direction;
		Parkour_Interface->Execute_Set_Parkour_Direction(Anim_Instance, Parkour_Direction);
	}

	else return;
	
	//Multicast_Set_Parkour_Direction(New_Parkour_Direction);
}

void UCustom_Movement_Component::On_Replication_Parkour_Direction()
{
	if(!Parkour_Interface)
	return;

	Parkour_Interface->Execute_Set_Parkour_Direction(Anim_Instance, Parkour_Direction);
}

void UCustom_Movement_Component::Multicast_Set_Parkour_Direction_Implementation(const FGameplayTag& New_Parkour_Direction)
{
	if(!Parkour_Interface)
	return;

	if(Parkour_Direction != New_Parkour_Direction)
	{
		Parkour_Direction = New_Parkour_Direction;
		Parkour_Interface->Execute_Set_Parkour_Direction(Anim_Instance, Parkour_Direction);
	}

	else return;
}

void UCustom_Movement_Component::Server_Handle_Release_From_Shimmying_Implementation(const FGameplayTag& Network_Parkour_Climb_Style, const FGameplayTag& Network_Parkour_Direction, const double& Network_Forward_Backward_Movement_Value_Absolute_Value)
{
	Multicast_Handle_Release_From_Shimmying(Network_Parkour_Climb_Style, Network_Parkour_Direction, Network_Forward_Backward_Movement_Value_Absolute_Value);
}

void UCustom_Movement_Component::Multicast_Handle_Release_From_Shimmying_Implementation(const FGameplayTag& Network_Parkour_Climb_Style, const FGameplayTag& Network_Parkour_Direction, const double& Network_Forward_Backward_Movement_Value_Absolute_Value)
{
	/*This function is called within &UCustom_Movement_Component::Release_From_Shimmying and it handles what happens when the
	character wants to be stop shimmying. Depending on the input placed into the character controller via the global double variables
	"Forward_Backward_Movement_Value" and "Right_Left_Movement_Value" and the current "Parkour_Climb_Style" set on the character, a call to execute
	the appropriate montage (which is stored within the object of the global pointer type UParkour_Action_Data* in the form of a Data Asset) will 
	be made via &UCustom_Movement_Component::Play_Parkour_Montage*/

	//Set the "Parkour_State" to "Parkour.State.Free.Roam". When this happens the collisions on the character's capsule component are enabled and the character's movement mode is set to "EMovementMode::Walking". Also,
	//"Character_Movement->StopMovementImmediately" is set to false (this is set to true when the character is shimmying).
	Debug::Print(TEXT("Exited_Parkour"), FColor::MakeRandomColor(), 8);

	/* //Store the absolute value of the value which is passed into the global double variable "Right_Left_Movement_Value". This value will be used to check if the input to move the character to the right or left
	//within the function "&UCustom_Movement_Component::Add_Movement_Input" is above the threshold to accept input.
	const double Forward_Backward_Movement_Value_Absolute_Value{UKismetMathLibrary::Abs(Forward_Backward_Movement_Value)}; */
	
	//Check to see if the character's "Climb_Style" is currently set to "Parkour.Climb.Style.Braced.Climb" and there is current input in the player controller to move the character either forwards or backwards. 
	//If so check to see if there is input fromt the player controller to either move the character to the left or the right. Depending on what value is stored in the global double variable "Right_Left_Movement_Value"
	//from the player controller the animation to dismantle the character from "FGameplayTag "Parkour.Climb.Style.Braced.Climb" will play. There are only rotation animations for "Parkour.Climb.Style.Braced.Climb".
	if(Network_Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))) && Network_Forward_Backward_Movement_Value_Absolute_Value > 0.f)
	{
		if(Network_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Right"))))
		{
			Play_Parkour_Montage(Ledge_Fall_Down_180_R);
			Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
		
		else if(Network_Parkour_Direction == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Direction.Backward.Left"))))
		{
			Play_Parkour_Montage(Ledge_Fall_Down_180_L);
			Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
		
	}

	//If the current "Climb_State" is currently not "Parkour.Climb.Style.Braced.Climb" && there is no input from the player controller the animation will play to release the character from the current "Climb_Style".
	//Said animation will leave the character facing the same direction as when shimmying was enabled. 
	else
	{
		if(Network_Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
		{
			Play_Parkour_Montage(Ledge_Fall_Down);
			Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
		

		else if(Network_Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
		{
			Play_Parkour_Montage(Hanging_Drop);
			Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Set_Parkour_Climb_Style(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.None"))));
		}
	}
}

void UCustom_Movement_Component::Server_Set_Parkour_Action_Implementation(const FGameplayTag& New_Parkour_Action, const int& Network_Random_Montage_To_Play)
{
	Multicast_Set_Parkour_Action(New_Parkour_Action, Network_Random_Montage_To_Play);
}

void UCustom_Movement_Component::Multicast_Set_Parkour_Action_Implementation(const FGameplayTag& New_Parkour_Action, const int& Network_Random_Montage_To_Play)
{
	/*The goal of this function is to use the FGameplayTag input argument passed into it via the function 
	"Decide_Parkour_Action()" to check if the global FGameplaytag variable "Parkour_Action" has the same FGameplaytag 
	value. If said global variable does not have the same FGameplaytag  as what is passed in via the input argument 
	then said FGameplaytag  should be set to equal the value of what is passed in via the input argument.This is followed 
	by setting the "Parkour_Action" in the interface by using the pointer to said interface and calling the generated 
	(at compile) interface function which begins with the prefix "Execute_" "Set_Parkour_Action()". Lastly, there 
	are "if '' and "else if" checks which need to be analyzed to check whether the FGameplaytag global variable which 
	has just been set equals specific Gameplay tags. If the global FGameplaytag == "Parkour.Action.No.Action" then the 
	function "Reset_Parkour_Variables()" should be called. Otherwise other "else if" statements should follow to check 
	whether said global FGameplaytag variable "Parkour_Action"   == any of the other Parkour Action gameplay tags. 
	Whichever tag said global variable equals the function "Play_Parkour_Montage()" should be called, passing in the 
	"UParkour_Action_Data*"which holds the address to the Asset Data object that is stored inside the character Blueprint 
	within the Custom_Movement_Component.*/

	if(!Parkour_Interface || !Owning_Player_Character || !CharacterOwner)
	return;

	if(Parkour_Action != New_Parkour_Action)
	Parkour_Action = New_Parkour_Action;

	Parkour_Interface->Execute_Set_Parkour_Action(Anim_Instance, Parkour_Action);

	if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))))
	{
		//Reset_Parkour_Variables();
	}
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb"))))
	{
		Play_Parkour_Montage(Braced_Jump_To_Climb);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.FreeHang"))
	{
		Play_Parkour_Montage(Free_Hang_Jump_To_Climb);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Braced.Climb.Falling.Climb"))
	{
		// Execute_Random_Montage(Airborne_To_Braced_Climb_Array);
		Play_Parkour_Montage(Braced_Jump_To_Climb_Airborne);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))
	{
		Play_Parkour_Montage(Leap_Entry_To_Climb_Hang_Idle);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.FreeHang.Falling.Climb"))
	{
		Play_Parkour_Montage(Free_Hang_Jump_To_Climb_Airborne);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb.Hanging.Jump"))))
	{
		Play_Parkour_Montage(Fly_Hanging_Jump);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Corner.Move"))
	{
		Parkour_Shimmy_Handle_Corner_Movement();

		if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag("Parkour.Climb.Style.Braced.Climb"))
		{
			if(bOut_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Left"))
				{
					Play_Parkour_Montage(Ledge_Corner_Outer_L);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Right"))
				{
					Play_Parkour_Montage(Ledge_Corner_Outer_R);
				}
			}

			else if(bIn_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Left"))
				{
					Play_Parkour_Montage(Ledge_Corner_Inner_L);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Right"))
				{
					Play_Parkour_Montage(Ledge_Corner_Inner_R);
				}
			}
		}

		else if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag("Parkour.Climb.Style.FreeHang"))
		{
			if(bOut_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Left"))
				{
					Play_Parkour_Montage(Hanging_Corner_Outer_L);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Right"))
				{
					Play_Parkour_Montage(Hanging_Corner_Outer_R);
				}
			}

			else if(bIn_Corner_Movement)
			{
				if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Left"))
				{
					Play_Parkour_Montage(Hanging_Corner_Inner_L);
				}

				else if(Get_Controller_Direction() == FGameplayTag::RequestGameplayTag("Parkour.Direction.Forward.Right"))
				{
					Play_Parkour_Montage(Hanging_Corner_Inner_R);
				}
			}
		}
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Shimmy.180.Shimmy"))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Climb_Shimmy_To_Shimmy_180_Vault);
			break;

			case 2:
			Play_Parkour_Montage(Ledge_Turn_L_Vault);
			break;

			case 3:
			Play_Parkour_Montage(Ledge_Turn_R_Vault);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			break;
		}

		//Execute_Random_Montage(Braced_And_Ledge_Shimmy_180_Shimmy_Array);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Shimmy.180.Shimmy"))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Hanging_180_L);
			break;

			case 2:
			Play_Parkour_Montage(Hanging_180_R);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			break;
		}
		
		//Execute_Random_Montage(Hanging_Shimmy_180_Shimmy_Array);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.Braced.Climb.Climb.Up"))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Ledge_Climb_Up_Reverse);
			break;

			case 2:
			Play_Parkour_Montage(Ledge_Climb_Up_TwoHand_L);
			break;

			case 3:
			Play_Parkour_Montage(Ledge_Climb_Up_TwoHand_R);
			break;

			case 4:
			Play_Parkour_Montage(Ledge_Climb_Up_Monkey);
			break;

			case 5:
			Play_Parkour_Montage(Climb_Up_The_Ledge);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			break;
		}

		/* Execute_Random_Montage(Ledge_Climb_Up_Array); */
		
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag("Parkour.Action.FreeHang.Climb.Up"))
	{
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Hanging_Climb_Up);
			break;

			case 2:
			Play_Parkour_Montage(Free_Hang_Climb_Up);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			break;
		}
		
		/* Execute_Random_Montage(Hanging_Climb_Up_Array); */
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	/* else if(Parkour_Action == Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()))
	{
		Server_Perform_Hop_Action(Get_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()));
	} */
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Forward"))))
	{
		/* int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 9)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		//Execute a switch statement to play the random montage. */
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Monkey_Vault);
			break;

			case 2:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Reverse_L_Vault);
			break;

			case 3:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Reverse_R_Vault);
			break;

			case 4:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Safety_L_Vault);
			break;

			case 5:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Safety_R_Vault);
			break;

			case 6:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Thief_L_Vault);
			break;

			case 7:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_Thief_R_Vault);
			break;

			case 8:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_TwoHand_L_Vault);
			break;

			case 9:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Ledge_Climb_Up_TwoHand_R_Vault);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Exit_Jump_Forward_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);

		}
		
		/* Execute_Random_Montage(Exit_Ledge_Jump_Forward_Array); */
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Exit.Jump.Backward"))))
	{
		/* int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 2)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		//Execute a switch statement to play the random montage. */
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Exit_Ledge_Jump_Backward_L);
			break;

			case 2:
			CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
			Play_Parkour_Montage(Exit_Ledge_Jump_Backward_R);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Exit_Jump_Forward_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);

		}
		
		/* Execute_Random_Montage(Exit_Ledge_Jump_Backward_Array); */
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Exit.Jump"))))
	{
		Play_Parkour_Montage(Exit_Hanging_Jump);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Accelerating.Drop"))))
	{
		/* //Select a random integer from the specified range. This integer will be used to play the respective random montage via a switch statement.
		int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 4)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play;

		//Execute a switch statement to play the random montage. */
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Accelerating_Drop_Ledge_L);
			break;

			case 2:
			Play_Parkour_Montage(Accelerating_Drop_Ledge_R);
			break;

			case 3:
			Play_Parkour_Montage(Accelerating_Drop_Slide_Ledge_L);
			break;

			case 4:
			Play_Parkour_Montage(Accelerating_Drop_Slide_Ledge_R);
			break;

			default:
			Debug::Print("Parkour_Action_Braced_Climb_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			break;
		}

		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);

		/* Execute_Random_Montage(Drop_Ledge_Array); */
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Accelerating.Drop"))))
	{
		/* int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 2)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play; */

		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Accelerating_Drop_Hanging_L);
			break;

			case 2:
			Play_Parkour_Montage(Accelerating_Drop_Hanging_R);
			break;

			default:
			Debug::Print("Parkour_Action_FreeHang_Accelerating_Drop_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
		
		//Execute_Random_Montage(Drop_Hanging_Array);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Normal.Drop"))))
	{
		Play_Parkour_Montage(Braced_Drop_Down);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Normal.Drop"))))
	{
		Play_Parkour_Montage(FreeHang_Drop_Down);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Left"))))
	{
		Play_Parkour_Montage(Wall_Run_L_Start);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Right"))))
	{
		Play_Parkour_Montage(Wall_Run_R_Start);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.L.Jump.F"))))
	{
		Play_Parkour_Montage(Wall_Run_L_Jump_F);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.R.Jump.F"))))
	{
		Play_Parkour_Montage(Wall_Run_R_Jump_F);
	}
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Left.Jump.90.R"))))
	{
		Play_Parkour_Montage(Wall_Run_L_Jump_90_R);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Right.Jump.90.L"))))
	{
		Play_Parkour_Montage(Wall_Run_R_Jump_90_L);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.L.Finish"))))
	{
		Play_Parkour_Montage(Wall_Run_L_Finish);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.R.Finish"))))
	{
		Play_Parkour_Montage(Wall_Run_R_Finish);
	}
	
	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Light"))))
	{
		Play_Parkour_Montage(Landing_Down_Light);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Impact"))))
	{
		Play_Parkour_Montage(Landing_Down_Impact);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Front"))))
	{
		/* int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 2)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play; */

		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Landing_Front_L);
			break;

			case 2:
			Play_Parkour_Montage(Landing_Front_R);
			break;

			default:
			Debug::Print("Parkour_Landing_Down_Front_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
		
		//Execute_Random_Montage(Landing_Down_Front_Array);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Landing.Down.Roll"))))
	{
		/* int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 2)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play; */

		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Landing_Roll_A_L);
			break;

			case 2:
			Play_Parkour_Montage(Landing_Roll_A_R);
			break;

			case 3:
			Play_Parkour_Montage(Landing_Roll_B_L);
			break;

			case 4:
			Play_Parkour_Montage(Landing_Roll_B_R);
			break;

			default:
			Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
		
		//Execute_Random_Montage(Landing_Down_Roll_Array);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Free.Roam.Accelerating.Drop")))))
	{
		/* int Random_Montage_To_Play{UKismetMathLibrary::RandomIntegerInRange(1, 9)};

		//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
		//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
		//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
		//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
		while(Last_Random_Montage_Played == Random_Montage_To_Play)
		{
			Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 9);
		}

		//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
		Last_Random_Montage_Played = Random_Montage_To_Play; */

		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Dash_Drop);
			break;

			case 2:
			Play_Parkour_Montage(Jump_Drop);
			break;

			case 3:
			Play_Parkour_Montage(Monkey_Drop);
			break;

			case 4:
			Play_Parkour_Montage(Reverse_L_Drop);
			break;

			case 5:
			Play_Parkour_Montage(Reverse_R_Drop);
			break;

			case 6:
			Play_Parkour_Montage(Speed_L_Drop);
			break;

			case 7:
			Play_Parkour_Montage(Speed_R_Drop);
			break;

			case 8:
			Play_Parkour_Montage(Two_Hand_L_Drop);
			break;

			case 9:
			Play_Parkour_Montage(Two_Hand_R_Drop);
			break;

			default:
			Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
		
		//Execute_Random_Montage(Free_Roam_Accelerating_Drop_Array);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Grounded")))))
	{
		Play_Parkour_Montage(Idle_To_Wall_Pipe_Attach);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Airborne")))))
	{
		Play_Parkour_Montage(Jumping_To_Wall_Pipe_Attach);
		CharacterOwner->GetCapsuleComponent()->SetCapsuleHalfHeight(48.f);
	}

	else if(Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Fall.Down")))))
	{
		Play_Parkour_Montage(Wall_Pipe_Fall_Down);
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Climb.Up.2.Hand")))))
	{
		Play_Parkour_Montage(Wall_Pipe_Climb_Up_2_Hand);
		Owning_Player_Character->GetCapsuleComponent()->SetCapsuleHalfHeight(98.f);
	}

	/* else if(New_Parkour_Action == Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()))
	{
		Perform_Hop_Action(Get_Wall_Pipe_Hop_Action_Based_On_Parkour_Direction(Get_Controller_Direction()));
	} */

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Up")))))
	{
		Play_Parkour_Montage(Jump_Up);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.L")))))
	{
		Play_Parkour_Montage(Accurate_Jump_Start_L);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.R")))))
	{
		Play_Parkour_Montage(Accurate_Jump_Start_R);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.L.Warp")))))
	{
		Play_Parkour_Montage(Accurate_Jump_Start_L_Warp);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Start.R.Warp")))))
	{
		Play_Parkour_Montage(Accurate_Jump_Start_R_Warp);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Accurate.Jump.Finish")))))
	{
		Play_Parkour_Montage(Accurate_Jump_Finish);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.L.Start")))))
	{
		Play_Parkour_Montage(Jump_Front_L_Start);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.R.Start")))))
	{
		Play_Parkour_Montage(Jump_Front_R_Start);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.L.Start.Warp")))))
	{
		Play_Parkour_Montage(Jump_Front_L_Start_Warp);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Front.R.Start.Warp")))))
	{
		Play_Parkour_Montage(Jump_Front_R_Start_Warp);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.Finish")))))
	{
		Play_Parkour_Montage(Jump_Finish);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.One.L")))))
	{
		Play_Parkour_Montage(Jump_One_L);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Jump.One.R")))))
	{
		Play_Parkour_Montage(Jump_One_R);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.90.L")))))
	{
		Play_Parkour_Montage(Balance_Walk_90_L);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.90.R")))))
	{
		Play_Parkour_Montage(Balance_Walk_90_R);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.180")))))
	{
		Play_Parkour_Montage(Balance_Walk_180);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Walk.Automatic.Hop")))))
	{
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Balance_Walk_Jump_Front);
			break;

			case 2:
			Play_Parkour_Montage(Jump_One_L);
			break;

			case 3:
			Play_Parkour_Montage(Jump_One_R);
			break;
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Drop.L.Hanging")))))
	{
		Play_Parkour_Montage(Balance_Drop_L_Hanging);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Balance.Drop.R.Hanging")))))
	{
		Play_Parkour_Montage(Balance_Drop_R_Hanging);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT( "Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.L")))))
	{
		Play_Parkour_Montage(Hanging_Climb_Up_To_Balanced_Walk_L);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT( "Parkour.Action.Hanging.Climb.Up.To.Balanced.Walk.R")))))
	{
		Play_Parkour_Montage(Hanging_Climb_Up_To_Balanced_Walk_R);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Idle.Wall.Vault.On")))))
	{
		Play_Parkour_Montage(Idle_Wall_Two_Hand_L_On);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Idle.Wall.Vault.Over")))))
	{
		Play_Parkour_Montage(Idle_Wall_Safety_On);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.On")))))
	{
		/*Execute_Random_Montage(Running_Wall_Vault_On);*/

		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Wall_Monkey_On);
			break;

			case 2:
			Play_Parkour_Montage(Wall_Two_Hand_L_On);
			break;

			case 3:
			Play_Parkour_Montage(Wall_Two_Hand_R_On);
			break;

			default:
			Debug::Print("Parkour_Running_Wall_Vault_On_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.Over")))))
	{
		/*Execute_Random_Montage(Running_Wall_Vault_On);*/

		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Wall_Monkey_Vault);
			break;

			case 2:
			Play_Parkour_Montage(Wall_Reverse_L_Vault);
			break;

			case 3:
			Play_Parkour_Montage(Wall_Reverse_R_Vault);
			break;

			case 4:
			Play_Parkour_Montage(Wall_Safety_L_Vault);
			break;

			case 5:
			Play_Parkour_Montage(Wall_Safety_R_Vault);
			break;

			case 6:
			Play_Parkour_Montage(Wall_Thief_L_Vault);
			break;

			case 7:
			Play_Parkour_Montage(Wall_Thief_R_Vault);
			break;

			case 8:
			Play_Parkour_Montage(Wall_Two_Hand_L_Vault);
			break;

			case 9:
			Play_Parkour_Montage(Wall_Two_Hand_R_Vault);
			break;

			default:
			Debug::Print("Parkour_Running_Wall_Vault_Over_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.Under.Bar")))))
	{
		Play_Parkour_Montage(Wall_Under_Bar);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Running.Wall.Vault.Over.180.Shimmy")))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Wall_L_Turn);
			break;

			case 2:
			Play_Parkour_Montage(Wall_R_Turn);
			break;

			default:
			Debug::Print("Parkour_Running_Wall_Vault_Over_180_Shimmy_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Idle")))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Tic_Tac_L_Jump_On);
			break;

			case 2:
			Play_Parkour_Montage(Tic_Tac_L_Vault_On);
			break;

			default:
			Debug::Print("Parkour_Tic_Tac_L_On_Front_Wall_To_Idle_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Idle")))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Tic_Tac_R_Jump_On);
			break;

			case 2:
			Play_Parkour_Montage(Tic_Tac_R_Vault_On);
			break;

			default:
			Debug::Print("Parkour_Tic_Tac_R_On_Front_Wall_To_Idle_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.On.Front.Wall.To.Run"))))
	{
		Play_Parkour_Montage(Tic_Tac_L_Jump_On_Run);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.On.Front.Wall.To.Run"))))
	{
		Play_Parkour_Montage(Tic_Tac_R_Jump_On_Run);
	}
	
	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Front.Wall")))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Tic_Tac_L_Jump_Over);
			break;

			case 2:
			Play_Parkour_Montage(Tic_Tac_L_Reverse_Over);
			break;

			case 3:
			Play_Parkour_Montage(Tic_Tac_L_Speed_Over);
			break;

			default:
			Debug::Print("Tic_Tac_L_Over_Front_Wall_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Front.Wall")))))
	{
		//Execute a switch statement to play the random montage.
		switch(Network_Random_Montage_To_Play)
		{
			case 1:
			Play_Parkour_Montage(Tic_Tac_R_Jump_Over);
			break;

			case 2:
			Play_Parkour_Montage(Tic_Tac_R_Reverse_Over);
			break;

			case 3:
			Play_Parkour_Montage(Tic_Tac_R_Speed_Over);
			break;

			default:
			Debug::Print("Tic_Tac_R_Over_Front_Wall_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
		}
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.L.Over.Right.Wall"))))
	{
		Play_Parkour_Montage(Tic_Tac_L_Jump_Side_Over);
	}

	else if(New_Parkour_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Tic.Tac.R.Over.Left.Wall"))))
	{
		Play_Parkour_Montage(Tic_Tac_R_Jump_Side_Over);
	}

}

void UCustom_Movement_Component::Server_Perform_Hop_Action_Implementation(const FGameplayTag& Network_Parkour_State, const FGameplayTag& Network_Hop_Action, const int& Network_Random_Montage_To_Play)
{
	Multicast_Perform_Hop_Action(Network_Parkour_State, Network_Hop_Action, Network_Random_Montage_To_Play);
}

void UCustom_Movement_Component::Multicast_Perform_Hop_Action_Implementation(const FGameplayTag& Network_Parkour_State, const FGameplayTag& Network_Hop_Action, const int& Network_Random_Montage_To_Play)
{
	if(Network_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
	{
		if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Up"))))
		{
			/* Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 5);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 5);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play; */

			//Execute a switch statement to play the random montage.
			switch(Network_Random_Montage_To_Play)
			{
				case 1:
				Play_Parkour_Montage(Braced_Hang_Hop_Up);
				break;

				case 2:
				Play_Parkour_Montage(Ledge_Jump_Up_Power);
				break;

				case 3:
				Play_Parkour_Montage(Ledge_Jump_Up);
				break;

				case 4:
				Play_Parkour_Montage(Climb_Shimmy_Long_L_Up);
				break;

				case 5:
				Play_Parkour_Montage(Climb_Shimmy_Long_R_Up);
				break;

				default:
				Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			}

			/* Execute_Random_Montage(Hop_Up_Array); */
		}
	
		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left"))))
		{
			/* Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
			} 
		
			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play; */

			//Execute a switch statement to play the random montage.
			switch(Network_Random_Montage_To_Play)
			{
				case 1:
				Play_Parkour_Montage(Braced_Hang_Hop_Left);
				break;

				case 2:
				Play_Parkour_Montage(Ledge_Jump_L_Short);
				break;

				case 3:
				Play_Parkour_Montage(Ledge_Jump_L);
				break;

				case 4:
				Play_Parkour_Montage(Climb_Shimmy_Long_L_Left);
				break;

				default:
				Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			}

			/*Execute_Random_Montage(Braced_And_Ledge_Hop_Left_Array);*/
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right"))))
		{
			/* Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 4);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play; */

			//Execute a switch statement to play the random montage.
			switch(Network_Random_Montage_To_Play)
			{
				case 1:
				Play_Parkour_Montage(Braced_Hang_Hop_Right);
				break;

				case 2:
				Play_Parkour_Montage(Ledge_Jump_R_Short);
				break;

				case 3:
				Play_Parkour_Montage(Ledge_Jump_R);
				break;

				case 4:
				Play_Parkour_Montage(Climb_Shimmy_Long_R_Right);
				break;

				default:
				Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			}

			/*Execute_Random_Montage(Braced_And_Ledge_Hop_Right_Array);*/
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Up"))))
		{
			/* Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play; */

			//Execute a switch statement to play the random montage.
			switch(Network_Random_Montage_To_Play)
			{
				case 1:
				Play_Parkour_Montage(Climb_Shimmy_Long_L_Up_Left);
				break;

				case 2:
				Play_Parkour_Montage(Braced_Hang_Hop_Left_Up);
				break;

				default:
				Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			}

			/*Execute_Random_Montage(Braced_And_Adventure_Hop_Up_Left_Array);*/
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Up"))))
		{
			/* Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 2);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play; */

			//Execute a switch statement to play the random montage.
			switch(Network_Random_Montage_To_Play)
			{
				case 1:
				Play_Parkour_Montage(Climb_Shimmy_Long_R_Up_Right);
				break;

				case 2:
				Play_Parkour_Montage(Braced_Hang_Hop_Right_Up);
				break;

				default:
				Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			}

			/*Execute_Random_Montage(Braced_And_Adventure_Hop_Up_Right_Array);*/
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Down"))))
		{
			/* Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);

			//Via a while loop check to see if the random integer which is selected from the line of code above is the same as the integer which was selected the last time this Parkour_Action was 
			//activated (stored within the global int variable "Last_Random_Montage_Played"). If this is the case the while loop should continue to select a random integer until the integer which 
			//is selected is different from the integer stored within the global int variable "Last_Random_Montage_Played". On the other hand if the integer which was selected in the line of code above 
			//is different from the integer stored in the global int variable "Last_Random_Montage_Played", this loop will never activate.
			while(Last_Random_Montage_Played == Random_Montage_To_Play)
			{
				Random_Montage_To_Play = UKismetMathLibrary::RandomIntegerInRange(1, 3);
			}

			//Assign the integer which was selected to the global int variable "Last_Random_Montage_Played".
			Last_Random_Montage_Played = Random_Montage_To_Play; */

			//Execute a switch statement to play the random montage.
			switch(Network_Random_Montage_To_Play)
			{
				case 1:
				Play_Parkour_Montage(Braced_Hang_Hop_Down);
				break;

				case 2:
				Play_Parkour_Montage(Ledge_Jump_Down);
				break;

				case 3:
				Play_Parkour_Montage(Climb_Leap_Down_To_Ledge);
				break;

				default:
				Debug::Print("Parkour_Landing_Down_Roll_ERROR_SELECTING_RANDOM_MONTAGE_TO_PLAY", FColor::Red, 21);
			}

			/*Execute_Random_Montage(Braced_And_Ledge_Hop_Down_Array);*/
		}
	
		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Left.Down"))))
		{
			Play_Parkour_Montage(Climb_Shimmy_Long_L_Down_Left);
		}
	
		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Hop.Right.Down"))))
		{
			Play_Parkour_Montage(Climb_Shimmy_Long_R_Down_Right);
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Left"))))
		{
			Play_Parkour_Montage(Free_Hang_Hop_Left);
		}
	
		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Right"))))
		{
			Play_Parkour_Montage(Free_Hang_Hop_Right);
		}
	
		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Hop.Down"))))
		{
			Play_Parkour_Montage(Hanging_Drop);
		}
	}

	else if(Network_Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Pipe.Climb"))))
	{
		if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Up"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Up);
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Down"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Down);
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Left"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Left);
		}

		else if(Network_Hop_Action == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Pipe.Jump.Right"))))
		{
			Play_Parkour_Montage(Wall_Pipe_Jump_Right);
		}
	}
}

void UCustom_Movement_Component::Server_Decide_Parkour_Action_Implementation()
{
	Multicast_Decide_Parkour_Action();
}

void UCustom_Movement_Component::Multicast_Decide_Parkour_Action_Implementation()
{
	if(Wall_Top_Result.bBlockingHit)
	{
		if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Climb"))))
		{
			Debug::Print("Shimmy_180_Shimmy_Climb_Or_Hop", FColor::MakeRandomColor(), 7);
			Decide_Shimmy_180_Shimmy_Mantle_Or_Hop();	
		}

		else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Free.Roam"))))
		{
			if(bIs_On_Ground)
			{
				if(Wall_Height >= 90 && Wall_Height <= 170)
				{
					if(Wall_Depth >= 0 && Wall_Depth <= 120)
					{
						if(Vault_Height >= 70 && Vault_Height <= 120)
						{
							if(Wall_Depth >= 20 && Wall_Depth <= 30)
							{
								if(UpdatedComponent->GetComponentVelocity().Size() > 20)
								{
									Debug::Print("Parkour_Low_Vault", FColor::MakeRandomColor(), 7);
								}

								else
								{
									Debug::Print("Parkour_Thin_Vault", FColor::MakeRandomColor(), 100);
								}
							}
						
							else if(UpdatedComponent->GetComponentVelocity().Size() > 20)
							{
								Debug::Print("Parkour_Low_Vault", FColor::MakeRandomColor(), 7);
							}

							else
							{
								Debug::Print("Parkour_Mantle", FColor::MakeRandomColor(), 7);
							}

						}

						else if(Vault_Height >= 130 && Vault_Height <= 140)
						{
							if(UpdatedComponent->GetComponentVelocity().Size() > 20)
							{
								Debug::Print("Parkour_High_Vault", FColor::MakeRandomColor(), 7);
							}

							else
							{
								Debug::Print("Parkour_No_Action", FColor::MakeRandomColor(), 7);
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
							}
						
						}

						else
						{
							Debug::Print("Parkour_No_Action", FColor::MakeRandomColor(), 7);
							Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
						}
					}

					else
					{
						Debug::Print("Parkour_Mantle", FColor::MakeRandomColor(), 7);
					}
				}

				else if(Wall_Height >= 190.f && Wall_Height <= 280.f)
				{
					
					Debug::Print("Parkour_Climb", FColor::MakeRandomColor(), 7);
					Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
					/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
					FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
					"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
					after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
					a location to begin the next sequence of ray casts.*/
					Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
					
					if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
					{
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb"))));
					}
					
					else
					{
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang"))));
					}
				}

				//Set the value within the global FGameplayTag "Parkour_Wall_Run_Side" to the appropriate value (Calculated within "&UCustom_Movement_Component::Assign_Wall_Run_Hit_Result"). 
				//This will initiate the respective montage to play which will place the character at the location stored in the FHitResult "Wall_Run_Hit_Result 
				//(location transfered to the global FHitResult variable "Wall_Top_Result" within "&UCustom_Movement_Component::Analyze_And_Validate_Wall_Run_Surface").
				//After these lines of code are executed the value set in the global FGameplayTag Parkour_State will be set to "Parkour.State.Wall.Run" via the UParkour_Action_Data object that stores the respective information. 
				//Therefore the fucntions called within &UCustom_Movement_Component::Parkour_Call_In_Tick within "else if(Parkour_State == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.State.Wall.Run"))))" 
				//will handle the rest of the wall run execution.
				else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Left"))))
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Left"))));

					Debug::Print("Parkour_Action_Set_To_Wall_Run_Start_Left", FColor::Emerald, 28);
				}

				else if(Parkour_Wall_Run_Side == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Wall.Run.Side.Right"))))
				{
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Wall.Run.Start.Right"))));
					Debug::Print("Parkour_Action_Set_To_Wall_Run_Start_Right", FColor::Emerald, 28);
				}

				else if(bReady_To_Initialize_Parkour_Wall_Pipe)
				{
					/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
					FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
					"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
					after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
					a location to begin the next sequence of ray casts.*/
					Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
					
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Grounded")))));
					bReady_To_Initialize_Parkour_Wall_Pipe = false;
				}

				else
				{
					Debug::Print("No_Action", FColor::MakeRandomColor(), 7);
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
				}
			}

			else
			{
				if(Wall_Height >= 140 && Wall_Height <= 280 && Validate_Can_Start_Shimmying_While_Airborne())
				{								
												/*negative value in "Air_Speed" means character is falling*/  //Ground_Speed >= 700.f || Air_Speed <= -1000.f || Air_Speed <= 20.f
					if(Owning_Player_Character->Get_Is_Jogging() && (Air_Speed >= -200.f || Air_Speed <= 200.f))
					{
						Debug::Print("Parkour_Dynamic_Airorne_Climb", FColor::MakeRandomColor(), 7);

						/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						a location to begin the next sequence of ray casts.*/
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
						
						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						{
							if(Air_Speed <= -500.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb.Slipped"))));
							}

							else if(Air_Speed >= -499.f)
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));
							}
						}
						
						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.FreeHang"))))
						{
							if(Validate_Can_Fly_Hanging_Jump())
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb.Hanging.Jump"))));
							}
							
							else
							{
								Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
							}
						}
					}

					else
					{
						Debug::Print("Parkour_Airorne_Climb", FColor::MakeRandomColor(), 7);
						Decide_Climb_Style(Wall_Top_Result.ImpactPoint, Reversed_Front_Wall_Normal_Z);
					
						/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
						FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
						"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
						after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
						a location to begin the next sequence of ray casts.*/
						Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;

						if(Parkour_Climb_Style == FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Climb.Style.Braced.Climb"))))
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.Braced.Climb.Falling.Climb"))));

						else
						Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.FreeHang.Falling.Climb"))));
					}

				}

				else if(bReady_To_Initialize_Parkour_Wall_Pipe)
				{
					/*The FHitResult stored in the global FHitResult variable "Wall_Top_Result" is copied to the the global 
					FHitResult variable Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result so that when the function "Reset_Parkour_Variables()" is called within
					"&UCustom_Movement_Component::Parkour_Call_In_Tick" and "&UCustom_Movement_Component::Set_Parkour_Action"
					after each Parkour Action is complete within "&UCustom_Movement_Component::Play_Parkour_Montage" there will still be 
					a location to begin the next sequence of ray casts.*/
					Initialize_Parkour_IK_Limbs_Hit_Result = Wall_Top_Result;
					
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag((FName(TEXT("Parkour.Action.Wall.Pipe.Attach.Airborne")))));
					bReady_To_Initialize_Parkour_Wall_Pipe = false;
				}

				else
				{
					Debug::Print("No_Action", FColor::MakeRandomColor(), 7);
					Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
				}
			}
		}
	}
	
	else
	{
		Debug::Print("Parkour_No_Action", FColor::MakeRandomColor(), 1);
		Set_Parkour_Action(FGameplayTag::RequestGameplayTag(FName(TEXT("Parkour.Action.No.Action"))));
	}

}

void UCustom_Movement_Component::Server_Move_Character_To_New_Climb_Position_Interpolation_Settings_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	Multicast_Move_Character_To_New_Climb_Position_Interpolation_Settings_Implementation(Location_To_Move_Character, Rotation_For_Character_To_Face);
}

void UCustom_Movement_Component::Multicast_Move_Character_To_New_Climb_Position_Interpolation_Settings_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	if(!Owning_Player_Character)
	return;
	
	//Call "SetActorLocationAndRotation" and pass in the variable which holds the interpolated data where the caracter needs to move to (Interpolated_Location_To_Move_Character).
	Owning_Player_Character->SetActorLocationAndRotation(Location_To_Move_Character, Rotation_For_Character_To_Face);
	
	return;
}

void UCustom_Movement_Component::Server_Move_Character_To_New_Wall_Run_Position_Interpolation_Settings_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	Multicast_Move_Character_To_New_Wall_Run_Position_Interpolation_Settings(Location_To_Move_Character, Rotation_For_Character_To_Face);
}

void UCustom_Movement_Component::Multicast_Move_Character_To_New_Wall_Run_Position_Interpolation_Settings_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	if(!Owning_Player_Character)
	return;

	//Call the function "Owning_Player_Character->SetActorLocationAndRotation()" and pass in the new location to move the character every tick.
	Owning_Player_Character->SetActorLocationAndRotation(Location_To_Move_Character, Rotation_For_Character_To_Face);

	return;
}

void UCustom_Movement_Component::Server_Move_Character_To_Front_Of_Pipe_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	Multicast_Move_Character_To_Front_Of_Pipe(Location_To_Move_Character, Rotation_For_Character_To_Face);
}

void UCustom_Movement_Component::Multicast_Move_Character_To_Front_Of_Pipe_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	//Get DeltaTime
	const double DeltaTime{UGameplayStatics::GetWorldDeltaSeconds(this)};
	
	//Set the interpolation speed.
	const double Interpolation_Speed{140.f};

	//Interpolate the characters current location (X and Y) to the same location (X and Y) of the wall pipe.
	const double Interpolated_Location_X{UKismetMathLibrary::FInterpTo(UpdatedComponent->GetComponentLocation().X, Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result.TraceStart.X + 25, DeltaTime, Interpolation_Speed)};
	const double Interpolated_Location_Y{UKismetMathLibrary::FInterpTo(UpdatedComponent->GetComponentLocation().Y,  Custom_Wall_Pipe_Actor_Forward_Vector_Hit_Result.TraceStart.Y, DeltaTime, Interpolation_Speed)};

	//combine the interpolated attributes into a single variable.
	const FVector New_Location_To_Move_Charater_During_Wall_Pipe_Climb{FVector(Interpolated_Location_X, Interpolated_Location_Y, UpdatedComponent->GetComponentLocation().Z)};

	//Call the function "Owning_Player_Character->SetActorLocationAndRotation()" and pass in the new location to move the character every tick.
	Owning_Player_Character->SetActorLocationAndRotation(New_Location_To_Move_Charater_During_Wall_Pipe_Climb, Reversed_Front_Wall_Normal_Z);
}

void UCustom_Movement_Component::Server_Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	Multicast_Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings(Location_To_Move_Character, Rotation_For_Character_To_Face);
}

void UCustom_Movement_Component::Multicast_Move_Character_To_New_Balance_Walk_Position_Interpolation_Settings_Implementation(const FVector& Location_To_Move_Character, const FRotator& Rotation_For_Character_To_Face)
{
	if(!Owning_Player_Character)
	return;
	
	//Call the function "SetActorLocationAndRotation" and pass in the calculated results.
	Owning_Player_Character->SetActorLocationAndRotation(Location_To_Move_Character, Rotation_For_Character_To_Face);
}

#pragma endregion


#pragma endregion

#pragma endregion 

