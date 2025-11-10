// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "WaypointDuration.h"
#include "Components/ActorComponent.h"
#include "CPP_MoveBetweenPointsComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent),BlueprintType)
class UNREAL1_API UCPP_MoveBetweenPointsComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCPP_MoveBetweenPointsComponent();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings",meta = (MakeEditWidget))
	TArray<FWaypointDuration> WaypointDuration;
	


protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	FVector startPos;
	float elapsedTime = .0f;
	float PercentageComplete = .0f;
	float currentDuration = .0f;
	FVector currentEndPos = FVector::ZeroVector;
	int waypointIndex = 1;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
	                           FActorComponentTickFunction* ThisTickFunction) override;
};
