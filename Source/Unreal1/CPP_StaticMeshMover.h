// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Kismet/KismetSystemLibrary.h"
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPP_StaticMeshMover.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class UNREAL1_API UCPP_StaticMeshMover : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UCPP_StaticMeshMover();

protected:
	FVector meshRelPos;
	
	// Called when the game starts
	virtual void BeginPlay() override;
	UStaticMeshComponent* meshComp;

public:
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
	float SinPotency = 2.f;
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	void MoveMesh(float elapsedTime);
		
};
