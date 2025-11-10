// Fill out your copyright notice in the Description page of Project Settings.


#include "Variant_Custom/CPP_Spawner.h"

// Sets default values for this component's properties
UCPP_Spawner::UCPP_Spawner()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UCPP_Spawner::BeginPlay()
{
	Super::BeginPlay();
	// Spawn given actor:

	if (ActorToSpawn == nullptr)
		return;

	double randX = FMath::FRandRange(-SpawnRange, SpawnRange);
	double randY = FMath::FRandRange(-SpawnRange, SpawnRange);
	double randZ = FMath::FRandRange(0, SpawnRange);

	FVector ownerLocation = GetOwner()->GetActorLocation();

	FVector* randPos = new FVector(randX + ownerLocation.X, randY + ownerLocation.Y, randZ + ownerLocation.Z);
	FRotator* rotation = new FRotator();
	FActorSpawnParameters spawnParams;

	GetWorld()->SpawnActor(ActorToSpawn, randPos, rotation, spawnParams);
}


// Called every frame
void UCPP_Spawner::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}
