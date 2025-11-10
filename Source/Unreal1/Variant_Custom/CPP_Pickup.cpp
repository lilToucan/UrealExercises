// Fill out your copyright notice in the Description page of Project Settings.


#include "Variant_Custom/CPP_Pickup.h"

#include "Components/BoxComponent.h"

// Sets default values
ACPP_Pickup::ACPP_Pickup()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	// // set box component as root
	// boxComponent = CreateDefaultSubobject<UBoxComponent>("BoxComponent");
	// boxComponent->InitBoxExtent(FVector (50,50,50));
	// SetRootComponent(boxComponent);
	//
	// //add mesh component 
	//  meshComp = CreateDefaultSubobject<UStaticMeshComponent>("StaticMesh");
	// meshComp->SetCollisionEnabled(ECollisionEnabled::Type::NoCollision);
	// meshComp->SetupAttachment(GetRootComponent());
}

// Called when the game starts or when spawned
void ACPP_Pickup::BeginPlay()
{
	Super::BeginPlay();
	
}



// Called every frame
void ACPP_Pickup::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

}


