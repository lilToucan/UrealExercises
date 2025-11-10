// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CPP_InteractionComponent.generated.h"


UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent), BlueprintType, Blueprintable)
class UNREAL1_API UCPP_InteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UCPP_InteractionComponent();
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fps_Character")
	float DistanceTrace = 1500.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fps_Character")
	float RadiousTrace = 400.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Fps_Character")
	TArray<TEnumAsByte<EObjectTypeQuery>> ObjectTypeTrace;

	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Fps_Character")
	TArray<AActor*> InteractionSphereTraceSingleForObjects(const float distance, const float radious);

	UFUNCTION(BlueprintCallable)
	AActor* FindClosestHitActor();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;
	
};
