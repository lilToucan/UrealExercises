// Fill out your copyright notice in the Description page of Project Settings.


#include "Variant_Custom/CPP_InteractionComponent.h"

#include "Kismet/KismetSystemLibrary.h"

// Sets default values for this component's properties
UCPP_InteractionComponent::UCPP_InteractionComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCPP_InteractionComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
}

// Called every frame
void UCPP_InteractionComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                              FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}


TArray<AActor*> UCPP_InteractionComponent::InteractionSphereTraceSingleForObjects(
	const float distance, const float radious)
{
	FVector EndTrace = GetOwner()->GetActorLocation() + GetOwner()->GetActorForwardVector() * distance;

	TArray<AActor*> ActorsToIgnore;

	TArray<FHitResult> arrayHit;
	TArray<AActor*> actorsHit;

	UKismetSystemLibrary::SphereTraceMultiForObjects
	(
		GetWorld(),
		GetOwner()->GetActorLocation(), // start trace
		EndTrace, // end trace
		radious,
		ObjectTypeTrace,\
		false,
		ActorsToIgnore, // empty 
		EDrawDebugTrace::ForOneFrame, // when draw debug
		arrayHit, // hit result
		true // ignore self
	);


	for (FHitResult hitResult : arrayHit)
	{
		actorsHit.Add(hitResult.GetActor());
	}

	return actorsHit;
}

AActor* UCPP_InteractionComponent::FindClosestHitActor()
{
	if (!GetWorld())
		return nullptr;

	TArray<AActor*> hitActor = InteractionSphereTraceSingleForObjects(DistanceTrace, RadiousTrace);

	if (hitActor.Num() <= 0)
		return nullptr;

	AActor* closestActor = hitActor[0];
	float minDist = FLT_MAX;
	FVector currentPos = GetOwner()->GetActorLocation();

	for (AActor* actor : hitActor)
	{
		if (!actor)
			continue;

		FVector actorPos = actor->GetActorLocation();
		float distance = (currentPos - actorPos).Length();

		if (distance < minDist)
		{
			minDist = distance;
			closestActor = actor;
		}
	}

	//UE_LOG(LogTemp, Warning, TEXT("hit actor: %s"),*(closestActor->GetActorNameOrLabel()));


	return closestActor;
}
