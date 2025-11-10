// Fill out your copyright notice in the Description page of Project Settings.


#include "CPP_StaticMeshMover.h"

// Sets default values for this component's properties
UCPP_StaticMeshMover::UCPP_StaticMeshMover()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCPP_StaticMeshMover::BeginPlay()
{
	Super::BeginPlay();

	// ...
	meshComp = GetOwner()->FindComponentByClass<UStaticMeshComponent>();
	meshRelPos= meshComp->GetComponentLocation();
}


// Called every frame
void UCPP_StaticMeshMover::TickComponent(float DeltaTime, ELevelTick TickType,
                                         FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UCPP_StaticMeshMover::MoveMesh(float elapsedTime)
{
	float sinZ = meshRelPos.Z + (FMath::Sin(elapsedTime)*SinPotency)+SinPotency;

	FVector newMeshRelPos =  FVector(meshRelPos.X, meshRelPos.Y, sinZ);
	
	meshComp->SetRelativeLocation(newMeshRelPos);
}
