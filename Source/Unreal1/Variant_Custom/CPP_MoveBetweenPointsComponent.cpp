// Fill out your copyright notice in the Description page of Project Settings.


#include "Variant_Custom/CPP_MoveBetweenPointsComponent.h"

// Sets default values for this component's properties
UCPP_MoveBetweenPointsComponent::UCPP_MoveBetweenPointsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;
	
	// ...
}


// Called when the game starts
void UCPP_MoveBetweenPointsComponent::BeginPlay()
{
	Super::BeginPlay();
	startPos = GetOwner()->GetActorLocation();
	// ...
}


// Called every frame
void UCPP_MoveBetweenPointsComponent::TickComponent(float DeltaTime, ELevelTick TickType,
                                                    FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
	if (PercentageComplete >= 1.f)
	{
		startPos = WaypointDuration[waypointIndex].Waypoint;
		elapsedTime = 0.0f;
		waypointIndex++;
		
		if (waypointIndex >= WaypointDuration.Num())
		{
			waypointIndex = 0;
		}
	}

	currentDuration = WaypointDuration[waypointIndex].Duration;
	currentEndPos = WaypointDuration[waypointIndex].Waypoint;
	

	elapsedTime += DeltaTime;
	PercentageComplete = elapsedTime / currentDuration;

	GetOwner()->SetActorRelativeLocation(FMath::Lerp(startPos, currentEndPos, PercentageComplete)); 
}
