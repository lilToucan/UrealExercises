// Copyright 2024 BitProtectStudio. All Rights Reserved.

#include "FlockSystemActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/BoxComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Runtime/Core/Public/HAL/RunnableThread.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"

AFlockSystemActor::AFlockSystemActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	SphereComponent = CreateDefaultSubobject<USphereComponent>(TEXT("SphereComponent"));
	RootComponent = SphereComponent;
	SphereComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SphereComponent->SetGenerateOverlapEvents(false);

	BoxComponent = CreateDefaultSubobject<UBoxComponent>(TEXT("BoxComponent"));
	BoxComponent->SetupAttachment(RootComponent);
	BoxComponent->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	BoxComponent->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_Pawn, ECollisionResponse::ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldStatic, ECollisionResponse::ECR_Overlap);
	BoxComponent->SetCollisionResponseToChannel(ECollisionChannel::ECC_WorldDynamic, ECollisionResponse::ECR_Overlap);

	BoxComponent->SetBoxExtent(FVector(1000.f, 1000.f, 1000.f), true);

	StaticMeshInstanceComponent = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StaticMeshInstanceComponent"));
	StaticMeshInstanceComponent->SetupAttachment(RootComponent);
	StaticMeshInstanceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaticMeshInstanceComponent->SetGenerateOverlapEvents(false);
}

FlockThread::FlockThread(AActor* newActor)
{
	m_Kill = false;
	m_Pause = false;

	//Initialize FEvent (as a cross platform (Confirmed Mac/Windows))
	m_semaphore = FGenericPlatformProcess::GetSynchEventFromPool(false);

	EThreadPriority threadPriority_;

	switch (flockParametersTHR.threadPriority)
	{
	case EPriority::Normal:
		threadPriority_ = TPri_Normal;
		break;
	case EPriority::Highest:
		threadPriority_ = TPri_Highest;
		break;
	case EPriority::Lowest:
		threadPriority_ = TPri_Lowest;
		break;
	case EPriority::AboveNormal:
		threadPriority_ = TPri_AboveNormal;
		break;
	case EPriority::BelowNormal:
		threadPriority_ = TPri_BelowNormal;
		break;
	case EPriority::SlightlyBelowNormal:
		threadPriority_ = TPri_SlightlyBelowNormal;
		break;
	case EPriority::TimeCritical:
		threadPriority_ = TPri_TimeCritical;
		break;

	default:
		threadPriority_ = TPri_AboveNormal;
		break;
	}

	ownerActor = newActor;

	Thread = FRunnableThread::Create(this, (TEXT("%s_FSomeRunnable"), *newActor->GetName()), 0, threadPriority_);
}

FlockThread::~FlockThread()
{
	if (m_semaphore)
	{
		//Cleanup the FEvent
		FGenericPlatformProcess::ReturnSynchEventToPool(m_semaphore);
		m_semaphore = nullptr;
	}

	if (Thread)
	{
		//Cleanup the worker thread
		delete Thread;
		Thread = nullptr;
	}
}

void FlockThread::EnsureCompletion()
{
	Stop();

	if (Thread)
	{
		Thread->WaitForCompletion();
	}
}

void FlockThread::PauseThread()
{
	m_Pause = true;
}

void FlockThread::ContinueThread()
{
	m_Pause = false;

	if (m_semaphore)
	{
		//Here is a FEvent signal "Trigger()" -> it will wake up the thread.
		m_semaphore->Trigger();
	}
}

bool FlockThread::Init()
{
	return true;
}

uint32 FlockThread::Run()
{
	//Initial wait before starting
	FPlatformProcess::Sleep(FMath::RandRange(0.05f, 0.5f));

	threadDeltaTime = 0.f;

	while (!m_Kill)
	{
		if (m_Pause)
		{
			//FEvent->Wait(); will "sleep" the thread until it will get a signal "Trigger()"
			m_semaphore->Wait();

			if (m_Kill)
			{
				return 0;
			}
		}
		else
		{
			// threadDeltaTime = FTimespan(FPlatformTime::Cycles64() - lastFrameTime).GetTotalSeconds();
			// lastFrameTime = FPlatformTime::Cycles64();

			// Fixing motion judder.
			threadDeltaTime = ownerActor->GetWorld()->GetTimeSeconds() - lastFrameTime;
			lastFrameTime = ownerActor->GetWorld()->GetTimeSeconds(); 

			TArray<FlockMemberData> flockMembersArr_ = flockThreadMembersArr;

			for (int32 flockMemberID_ = 0; flockMemberID_ < flockMembersArr_.Num(); ++flockMemberID_)
			{
				bool bIsAvoidance_(false);
				FlockMemberData& flockMember_ = flockMembersArr_[flockMemberID_];

				// Clear attacked actors 
				flockMember_.attackedActors.Empty();

				FVector followVec_ = FVector::ZeroVector;
				FVector cohesionVec_ = FVector::ZeroVector;
				FVector alignmentVec_ = FVector::ZeroVector;
				FVector separationVec_ = FVector::ZeroVector;
				FVector fleeVec_ = FVector::ZeroVector;
				FVector newVelocity_ = FVector::ZeroVector;

				FVector const flockMemberLocation_ = flockMember_.transform.GetLocation();


				// Follow to Leader
				if (flockMember_.bIsFlockLeader)
				{
					newVelocity_ += SteeringWander(flockMember_);

					flockMember_.elapsedTimeSinceLastWander += threadDeltaTime;
				}
				else
				{
					if (flockParametersTHR.followScale > 0.0f)
					{
						// Leader following (seek)
						followVec_ = SteeringFollow(flockMember_, 0) * flockParametersTHR.followScale;
					}

					// Other forces need nearby flock mates
					TArray<int32> mates_ = GetNearbyFlockMates(flockMemberID_);

					if (flockParametersTHR.cohesionScale > 0.0f)
					{
						// Cohesion - staying near nearby flock mates
						cohesionVec_ = SteeringCohesion(flockMember_, mates_) * flockParametersTHR.cohesionScale;
					}

					if (flockParametersTHR.alignScale > 0.0f)
					{
						// Alignment =  aligning with the heading of nearby flock mates
						alignmentVec_ = SteeringAlign(flockMember_, mates_) * flockParametersTHR.alignScale;
					}

					if (flockParametersTHR.separationScale > 0.0f)
					{
						// Separation = trying to not get too close to flock mates
						separationVec_ = SteeringSeparate(flockMember_, mates_) * flockParametersTHR.separationScale;
					}
				}
				// Flee = running away from enemies!
				if (!flockParametersTHR.bFollowToPawn && flockParametersTHR.fleeScale > 0.0f)
				{
					fleeVec_ = SteeringFlee(flockMember_) * flockParametersTHR.fleeScale;
					if (fleeVec_ != FVector::ZeroVector)
					{
						bIsAvoidance_ = true;
					}
				}
				// Flee = running away from primitive object collision from root component!
				if (flockParametersTHR.fleeScaleAvoidance > 0.f && allOverlappingComponentsArrTHR.Num() > 0 && flockParametersTHR.bAutoAddComponentsInArray)
				{
					FVector avoidVec_ = SteeringAvoidanceComponent(flockMember_) * flockParametersTHR.fleeScaleAvoidance;

					if (avoidVec_ != FVector::ZeroVector)
					{
						bIsAvoidance_ = true;
						fleeVec_ = avoidVec_;
					}
				}

				// Flee = running away from primitive object collision from root component!
				if (flockParametersTHR.fleeScaleAvoidance > 0.f && avoidanceActorRootArrTHR.Num() > 0)
				{
					FVector avoidVec_ = SteeringAvoidance(flockMember_) * flockParametersTHR.fleeScaleAvoidance;

					if (avoidVec_ != FVector::ZeroVector)
					{
						bIsAvoidance_ = true;
						fleeVec_ = avoidVec_;
					}
				}
				// Avoidance Aquarium. 
				if (flockParametersTHR.fleeScaleAquarium > 0.f && flockParametersTHR.bUseAquarium)
				{
					//if (!UKismetMathLibrary::IsPointInBox(flockMemberLocation_, BoxComponentRef->GetComponentLocation(), BoxComponentRef->GetScaledBoxExtent()))
					if (!UKismetMathLibrary::IsPointInBoxWithTransform(flockMemberLocation_, BoxComponentRef->GetComponentTransform(), BoxComponentRef->GetUnscaledBoxExtent()))
					{
						// Flee = running away from Aquarium wall!
						fleeVec_ = SteeringAquarium(flockMember_) * flockParametersTHR.fleeScaleAquarium;
						if (fleeVec_ != FVector::ZeroVector)
						{
							bIsAvoidance_ = true;
						}
					}
				}
				// Flee = running away from max height!
				if (flockParametersTHR.bUseMaxHeight)
				{
					if (flockMemberLocation_.Z >= flockParametersTHR.maxHeight)
					{
						// Flee = running away from Aquarium wall!
						fleeVec_ = SteeringMaxHeight(flockMember_) * flockParametersTHR.fleeScaleAquarium;
						if (fleeVec_ != FVector::ZeroVector)
						{
							bIsAvoidance_ = true;
						}
					}
				}
				// Follow to leader.
				newVelocity_ += fleeVec_;
				if (fleeVec_.SizeSquared() <= 0.1f)
				{
					newVelocity_ += followVec_;
					newVelocity_ += cohesionVec_;
					newVelocity_ += alignmentVec_;
					newVelocity_ += separationVec_;
				}
				// Truncate the new force calculated in newVelocity so we don't go crazy
				newVelocity_ = newVelocity_.GetClampedToSize(0.0f, flockParametersTHR.flockMaxSteeringForce);

				FVector targetVelocity = flockMember_.velocity + newVelocity_;

				float flockRotRate_(flockParametersTHR.flockMateRotationRate);
				if (bIsAvoidance_)
				{
					flockRotRate_ = flockParametersTHR.escapeMateRotationRate;
				}
				// Rotate the flock member towards the velocity direction vector
				// get the rotation value for our desired target velocity (i.e. if we were in that direction)
				// Interpolate our current rotation towards the desired velocity vector based on rotation speed * time
				FRotator rot = FRotationMatrix::MakeFromX((flockMemberLocation_ + targetVelocity) - flockMemberLocation_).Rotator();
				FRotator final = FMath::RInterpTo(flockMember_.transform.Rotator(), rot, threadDeltaTime, flockRotRate_);

				flockMember_.transform.SetRotation(final.Quaternion());

				FVector forward = flockMember_.transform.GetUnitAxis(EAxis::X);
				forward.Normalize();
				flockMember_.velocity = forward * targetVelocity.Size();

				// Clamp our new velocity to be within min->max speeds
				if (flockMember_.velocity.Size() > flockParametersTHR.flockMaxSpeed)
				{
					flockMember_.velocity = flockMember_.velocity.GetSafeNormal() * FMath::RandRange(flockParametersTHR.flockMaxSpeed - flockParametersTHR.flockOffsetSpeed,
					                                                                                 flockParametersTHR.flockMaxSpeed + flockParametersTHR.flockOffsetSpeed);
				}
				// If need escape from danger actor.
				if (bIsAvoidance_)
				{
					flockMember_.velocity = flockMember_.velocity * flockParametersTHR.escapeMaxSpeedMultiply;
				}
				FVector setSpeed_ = flockMemberLocation_ + flockMember_.velocity;

				flockMember_.transform.SetLocation(FMath::VInterpTo(flockMemberLocation_, setSpeed_, threadDeltaTime, flockParametersTHR.moveSpeedInterpInThread));

				// Save all parameters.
				flockThreadMembersArr[flockMemberID_] = flockMember_;
			}


			if (flockParametersTHR.ExperimentalOptimization)
			{
				threadDeltaTime += threadSleepTime;
			}

			//Critical section:
			m_mutex.Lock();
			//We are locking our FCriticalSection so no other thread will access it
			//And thus it is a thread-safe access now

			flockThreadMembersArr = flockMembersArr_;

			//Unlock FCriticalSection so other threads may use it.
			m_mutex.Unlock();

			//Pause Condition - if we RandomVectors contains more vectors than m_amount we shall pause the thread to release system resources.
			m_Pause = true;

			if (flockParametersTHR.ExperimentalOptimization)
			{
				//A little sleep between the chunks (So CPU will rest a bit -- (may be omitted))
				FPlatformProcess::Sleep(threadSleepTime);
			}
		}
	}
	return 0;
}

void FlockThread::Stop()
{
	m_Kill = true; //Thread kill condition "while (!m_Kill){...}"
	m_Pause = false;

	if (m_semaphore)
	{
		//We shall signal "Trigger" the FEvent (in case the Thread is sleeping it shall wake up!!)
		m_semaphore->Trigger();
	}
}

bool FlockThread::IsThreadPaused() const
{
	return (bool)m_Pause;
}

TArray<FlockMemberData> FlockThread::GetFlockMembersData()
{
	m_mutex.Lock();

	TArray<FlockMemberData> actualArray_ = flockThreadMembersArr;

	this->ContinueThread();

	m_mutex.Unlock();

	return actualArray_;
}

void FlockThread::InitFlockParameters(TArray<FlockMemberData> setFlockMembersArr, FlockMemberParameters newParameters, UBoxComponent* SetBoxComponent)
{
	flockParametersTHR = newParameters;
	BoxComponentRef = SetBoxComponent;
	flockThreadMembersArr = setFlockMembersArr;

	flockThreadMembersArr[0].bIsFlockLeader = true;
}

void FlockThread::SetOverlappingComponents(TArray<UPrimitiveComponent*> overlappingComponentsArr, TArray<AActor*> dangerActors)
{
	allOverlappingComponentsArrTHR = overlappingComponentsArr;
	dangerActorsTHR = dangerActors;
}

void FlockThread::SetAvoidanceActor(TArray<AActor*> avoidanceActorRootArr)
{
	avoidanceActorRootArrTHR = avoidanceActorRootArr;
}

void AFlockSystemActor::BeginPlay()
{
	Super::BeginPlay();

	StaticMeshInstanceComponent->SetWorldScale3D(FVector(1.f, 1.f, 1.f));

	StaticMeshInstanceComponent->SetStaticMesh(StaticMesh);

	FVector const startLoc_(GetActorLocation());

	for (int i = 0; i < flockMateInstances; ++i)
	{
		FVector const direction_(FMath::VRand());
		FRotator const rotation_(UKismetMathLibrary::RandomRotator());
		float const distance_(FMath::RandRange(0.01f, SphereComponent->GetScaledSphereRadius()));
		FVector const newLoc_(startLoc_ + direction_ * distance_);
		float const randScale_(FMath::RandRange(minMeshScale, maxMeshScale));

		AddFlockMemberWorldSpace(FTransform(rotation_, newLoc_, FVector(randScale_, randScale_, randScale_)));
	}

	if (flockParameters.bAutoAddComponentsInArray || flockParameters.bReactOnPawn)
	{
		GetWorldTimerManager().SetTimer(addAvoidanceActor_Timer, this, &AFlockSystemActor::AddAvoidanceComponentsTimer, 1.f, true, 0.5f);
	}

	DivideFlockArrayForThreads();

	for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
	{
		FlockActorPoolThreadArr[i] = nullptr;
	}

	GenerateFlockThread();
}

void AFlockSystemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
	{
		if (FlockActorPoolThreadArr[i])
		{
			FlockActorPoolThreadArr[i]->EnsureCompletion();
			delete FlockActorPoolThreadArr[i];
			FlockActorPoolThreadArr[i] = nullptr;
		}
	}
	FlockActorPoolThreadArr.Empty();

	Super::EndPlay(EndPlayReason);
}

void AFlockSystemActor::BeginDestroy()
{
	for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
	{
		if (FlockActorPoolThreadArr[i])
		{
			FlockActorPoolThreadArr[i]->EnsureCompletion();
			delete FlockActorPoolThreadArr[i];
			FlockActorPoolThreadArr[i] = nullptr;
		}
	}
	FlockActorPoolThreadArr.Empty();

	Super::BeginDestroy();
}

void AFlockSystemActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	if (!StaticMeshInstanceComponent) return;

	TArray<FlockMemberData> flockMembersDataArr_;

	for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
	{
		if (FlockActorPoolThreadArr[i])
		{
			TArray<FlockMemberData> tempArr = FlockActorPoolThreadArr[i]->GetFlockMembersData();
			for (int id = 0; id < tempArr.Num(); id++)
			{
				flockMembersDataArr_.Add(tempArr[id]);
			}
		}
	}
	// Move flock members. 
	for (int32 flockMemberID_ = 0; flockMemberID_ < flockMembersDataArr_.Num(); ++flockMemberID_)
	{
		if (flockMembersDataArr_[flockMemberID_].instanceIndex > StaticMeshInstanceComponent->GetInstanceCount()) continue; // don't do anything if we haven't got an instance in range...

		FTransform interpFlockTransform;

		// Interpolate Vector and Rotate
		FVector interpVector = FMath::VInterpConstantTo(mFlockMemberData[flockMemberID_].transform.GetLocation(),
		                                                flockMembersDataArr_[flockMemberID_].transform.GetLocation(), DeltaTime, interpMoveAnimRate);

		// FRotator InterpRotator = FMath::RInterpTo(mFlockMemberData[flockMemberID_].transform.Rotator(),
		//                                           flockMembersDataArr_[flockMemberID_].transform.Rotator(), DeltaTime, interpRotateAnimRate);

		interpFlockTransform.SetLocation(interpVector);
		interpFlockTransform.SetRotation(FQuat(flockMembersDataArr_[flockMemberID_].transform.Rotator()));
		mFlockMemberData[flockMemberID_].transform.SetLocation(interpVector);
		mFlockMemberData[flockMemberID_].transform.SetRotation(FQuat(flockMembersDataArr_[flockMemberID_].transform.Rotator()));

		StaticMeshInstanceComponent->UpdateInstanceTransform(mFlockMemberData[flockMemberID_].instanceIndex, mFlockMemberData[flockMemberID_].transform, true, false);

		// Attack Pawn.
		if (flockParameters.bCanAttackPawn)
		{
			for (int attackedID_ = 0; attackedID_ < flockMembersDataArr_[flockMemberID_].attackedActors.Num(); ++attackedID_)
			{
				if (flockMembersDataArr_[flockMemberID_].attackedActors[attackedID_])
				{
					UGameplayStatics::ApplyDamage(flockMembersDataArr_[flockMemberID_].attackedActors[attackedID_], flockParameters.damageValue,
					                              nullptr, this, flockParameters.damageType);
				}
			}
		}
	}

	StaticMeshInstanceComponent->MarkRenderStateDirty();
}

void AFlockSystemActor::GenerateFlockThread()
{
	if (maxUseThreads > flockMateInstances)
	{
		FlockActorPoolThreadArr.Add(new FlockThread(this));
		FlockActorPoolThreadArr[0]->InitFlockParameters(allFlockMembersArrays[0].flockMembersArr, flockParameters, BoxComponent);
		FlockActorPoolThreadArr[0]->SetPoolThread(FlockActorPoolThreadArr);
		FlockActorPoolThreadArr[0]->InitFlockLeader();
		if (avoidanceActorRootArr.Num() > 0)
		{
			FlockActorPoolThreadArr[0]->SetAvoidanceActor(avoidanceActorRootArr);
		}
		return;
	}
	for (int i = 0; i < maxUseThreads; i++)
	{
		FlockActorPoolThreadArr.Add(new FlockThread(this));
		FlockActorPoolThreadArr[i]->InitFlockParameters(allFlockMembersArrays[i].flockMembersArr, flockParameters, BoxComponent);

		if (avoidanceActorRootArr.Num() > 0)
		{
			FlockActorPoolThreadArr[i]->SetAvoidanceActor(avoidanceActorRootArr);
		}
	}

	// Set pool thread for control flock from first thread.
	for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
	{
		FlockActorPoolThreadArr[i]->SetPoolThread(FlockActorPoolThreadArr);
	}
	if (flockParameters.bUseOneLeader)
	{
		for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
		{
			FlockActorPoolThreadArr[i]->InitFlockLeader();
		}
	}
}

void AFlockSystemActor::AddAvoidanceComponentsTimer()
{
	if (flockParameters.bAutoAddComponentsInArray)
	{
		allOverlappingComponentsArr.Empty();
		dangerActors.Empty();

		TArray<UPrimitiveComponent*> overlappingComponentsArr_;


		BoxComponent->GetOverlappingComponents(overlappingComponentsArr_);

		for (int i = 0; i < overlappingComponentsArr_.Num(); i++)
		{
			if (overlappingComponentsArr_[i]->GetOwner()->GetClass() == GetClass())
			{
				continue;
			}
			APawn* checkPawn_ = Cast<APawn>(overlappingComponentsArr_[i]->GetOwner());
			if (checkPawn_)
			{
				if (flockParameters.bReactOnPawn)
				{
					dangerActors.AddUnique(checkPawn_);
				}
			}
			else
			{
				allOverlappingComponentsArr.Add(overlappingComponentsArr_[i]);
			}
		}
	}

	for (int i = 0; i < FlockActorPoolThreadArr.Num(); i++)
	{
		if (FlockActorPoolThreadArr[i])
		{
			FlockActorPoolThreadArr[i]->SetOverlappingComponents(allOverlappingComponentsArr, dangerActors);
		}
	}
}

FVector FlockThread::SteeringAquarium(const FlockMemberData& flockMember) const
{
	FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(flockMember.transform.GetLocation(), BoxComponentRef->GetComponentLocation()));
	FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
	direction_.Normalize();
	FVector newVec_ = direction_ * ((flockParametersTHR.flockEnemyAwarenessRadius / flockParametersTHR.strengthAquariumOffsetValue) * flockParametersTHR.fleeScaleAquarium);
	return newVec_;
}

FVector FlockThread::SteeringAvoidanceComponent(const FlockMemberData& flockMember) const
{
	FVector newVec_(FVector::ZeroVector);

	for (int i = 0; i < allOverlappingComponentsArrTHR.Num(); ++i)
	{
		if (allOverlappingComponentsArrTHR[i])
		{
			UPrimitiveComponent* primComp_(Cast<UPrimitiveComponent>(allOverlappingComponentsArrTHR[i]));
			if (primComp_)
			{
				FVector closestPoint_;
				primComp_->GetClosestPointOnCollision(flockMember.transform.GetLocation(), closestPoint_);

				if ((closestPoint_ - flockMember.transform.GetLocation()).Size() < flockParametersTHR.avoidancePrimitiveDistance)
				{
					FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(closestPoint_, flockMember.transform.GetLocation()));
					FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
					direction_.Normalize();

					newVec_ = direction_ * ((flockParametersTHR.flockEnemyAwarenessRadius / flockParametersTHR.strengthAquariumOffsetValue) * flockParametersTHR.fleeScaleAquarium);
				}
			}
		}
	}

	return newVec_;
}

FVector FlockThread::SteeringWander(FlockMemberData& flockMember) const
{
	FVector newVec_ = flockMember.wanderPosition - flockMember.transform.GetLocation();

	if (flockMember.elapsedTimeSinceLastWander >= flockParametersTHR.flockWanderUpdateRate || newVec_.Size() <= flockParametersTHR.flockMinWanderDistance)
	{
		flockMember.wanderPosition = GetRandomWanderLocation(); // + GetActorLocation();
		flockMember.elapsedTimeSinceLastWander = 0.0f;
		newVec_ = flockMember.wanderPosition - flockMember.transform.GetLocation();
	}
	return newVec_;
}

FVector FlockThread::SteeringAlign(FlockMemberData& flockMember, TArray<int32>& flockMates)
{
	FVector vel_ = FVector(0, 0, 0);
	if (flockMates.Num() == 0) return vel_;

	for (int32 i = 0; i < flockMates.Num(); i++)
	{
		vel_ += flockThreadMembersArr[flockMates[i]].velocity;
	}
	vel_ /= flockMates.Num();

	return vel_;
}

FVector FlockThread::SteeringSeparate(const FlockMemberData& flockMember, TArray<int32>& flockMates)
{
	FVector force_ = FVector(0, 0, 0);
	if (flockMates.Num() == 0) return force_;

	for (int32 i = 0; i < flockMates.Num(); i++)
	{
		FVector diff_ = flockMember.transform.GetLocation() - flockThreadMembersArr[flockMates[i]].transform.GetLocation();
		float const scale_ = diff_.Size();
		diff_.Normalize();
		diff_ = diff_ * (flockParametersTHR.separationRadius / scale_);
		force_ += diff_;
	}
	return force_;
}

FVector FlockThread::SteeringCohesion(const FlockMemberData& flockMember, TArray<int32>& flockMates)
{
	FVector avgPos_ = FVector(0, 0, 0);
	if (flockMates.Num() == 0) return avgPos_;

	for (int32 i = 0; i < flockMates.Num(); i++)
	{
		avgPos_ += flockThreadMembersArr[flockMates[i]].transform.GetLocation();
	}

	avgPos_ /= flockMates.Num();

	return avgPos_ - flockMember.transform.GetLocation();
}

FVector FlockThread::SteeringFlee(const FlockMemberData& flockMember)
{
	FVector newVec_ = FVector(0, 0, 0);

	for (int i = 0; i < dangerActorsTHR.Num(); ++i)
	{
		if (dangerActorsTHR[i])
		{
			// calculate flee from this threat
			FVector fromEnemy_ = flockMember.transform.GetLocation() - dangerActorsTHR[i]->GetActorLocation();
			float const distanceToEnemy_ = fromEnemy_.Size();
			fromEnemy_.Normalize();

			// enemy inside our enemy awareness threshold, so evade them
			if (distanceToEnemy_ < flockParametersTHR.flockEnemyAwarenessRadius)
			{
				newVec_ += fromEnemy_ * ((flockParametersTHR.flockEnemyAwarenessRadius / distanceToEnemy_) * flockParametersTHR.fleeScale);
			}
		}
	}
	return newVec_;
}

FVector FlockThread::GetRandomWanderLocation() const
{
	FVector returnVector_;

	if (flockParametersTHR.bUseAquarium)
	{
	//	returnVector_ = UKismetMathLibrary::RandomPointInBoundingBox(BoxComponentRef->GetComponentLocation(), BoxComponentRef->GetScaledBoxExtent());
		// Generating a point taking into account the box rotation
		FVector localRandom_ = UKismetMathLibrary::RandomPointInBoundingBox(FVector::ZeroVector, BoxComponentRef->GetScaledBoxExtent());
		FVector worldRandom_ = BoxComponentRef->GetComponentTransform().TransformPosition(localRandom_);
		returnVector_ = worldRandom_;
	}
	else
	{
		returnVector_ = (BoxComponentRef->GetComponentLocation() + FVector(FMath::RandRange(-flockParametersTHR.flockWanderInRandomRadius, flockParametersTHR.flockWanderInRandomRadius),
		                                                                   FMath::RandRange(-flockParametersTHR.flockWanderInRandomRadius, flockParametersTHR.flockWanderInRandomRadius),
		                                                                   FMath::RandRange(-flockParametersTHR.flockWanderInRandomRadius, flockParametersTHR.flockWanderInRandomRadius)));
	}

	if (flockParametersTHR.bUseMaxHeight)
	{
		if (returnVector_.Z >= flockParametersTHR.maxHeight)
		{
			returnVector_.Z = flockParametersTHR.maxHeight - 50.f;
		}
	}

	return returnVector_;
}

FVector FlockThread::SteeringFollow(FlockMemberData& flockMember, int32 flockLeader)
{
	bool bIsFollowToEnemy_(false);
	FVector newVec_ = FVector::ZeroVector;

	FlockMemberData newFlockLeader;
	if (flockParametersTHR.bUseOneLeader)
	{
		newFlockLeader = PoolThreadArr[0]->flockThreadMembersArr[flockLeader];
	}
	else
	{
		newFlockLeader = flockThreadMembersArr[flockLeader];
	}

	// Follow to pawn
	if (flockParametersTHR.bFollowToPawn)
	{
		for (int i = 0; i < dangerActorsTHR.Num(); ++i)
		{
			if (dangerActorsTHR[i])
			{
				// calculate flee from this threat
				FVector fromEnemy_ = dangerActorsTHR[i]->GetActorLocation() - flockMember.transform.GetLocation();
				float const distanceToEnemy_ = fromEnemy_.Size();
				fromEnemy_.Normalize();

				// enemy inside our enemy awareness threshold, so evade them
				if (distanceToEnemy_ < flockParametersTHR.followPawnAwarenessRadius)
				{
					newVec_ = dangerActorsTHR[i]->GetActorLocation() - flockMember.transform.GetLocation();
					newVec_.Normalize();
					newVec_ *= flockParametersTHR.flockMaxSpeed;
					newVec_ -= flockMember.velocity;

					// Add attacked actors in array.
					if (flockParametersTHR.bCanAttackPawn)
					{
						if ((dangerActorsTHR[i]->GetActorLocation() - flockMember.transform.GetLocation()).SizeSquared() < flockParametersTHR.attackRadiusSquared)
						{
							flockMember.attackedActors.AddUnique(dangerActorsTHR[i]);
						}

						// UPrimitiveComponent* primComp_(Cast<UPrimitiveComponent>(dangerActorsTHR[i]->GetRootComponent()));
						// if (primComp_)
						// {							
						// 	FVector closestPoint_;
						// 	primComp_->GetClosestPointOnCollision(flockMember.transform.GetLocation(), closestPoint_);
						//
						// 	if ((closestPoint_ - flockMember.transform.GetLocation()).SizeSquared() < flockParametersTHR.attackRadiusSquared)
						// 	{								
						// 		flockMember.attackedActors.AddUnique(dangerActorsTHR[i]);
						// 	}
						// }
					}

					bIsFollowToEnemy_ = true;
					break;
				}
			}
		}
	}

	if (!bIsFollowToEnemy_)
	{
		if (flockParametersTHR.followActor)
		{
			newVec_ = flockParametersTHR.followActor->GetActorLocation() - flockMember.transform.GetLocation();
			newVec_.Normalize();
			newVec_ *= flockParametersTHR.flockMaxSpeed;
			newVec_ -= flockMember.velocity;
		}
		else if (flockLeader <= flockThreadMembersArr.Num() && flockLeader >= 0)
		{
			newVec_ = newFlockLeader.transform.GetLocation() - flockMember.transform.GetLocation();
			newVec_.Normalize();
			newVec_ *= flockParametersTHR.flockMaxSpeed;
			newVec_ -= flockMember.velocity;
		}
	}

	return newVec_;
}

FVector FlockThread::SteeringMaxHeight(const FlockMemberData& flockMember) const
{
	FRotator const rotationToDeep_(UKismetMathLibrary::FindLookAtRotation(flockMember.transform.GetLocation(),
	                                                                      FVector(BoxComponentRef->GetComponentLocation().X, BoxComponentRef->GetComponentLocation().Y, flockParametersTHR.maxHeight)));
	FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToDeep_);
	direction_.Normalize();
	FVector newVec_ = direction_ * ((flockParametersTHR.flockEnemyAwarenessRadius / flockParametersTHR.strengthAquariumOffsetValue) * flockParametersTHR.fleeScaleAquarium);
	return newVec_;
}

FVector FlockThread::SteeringFollowPawn(FlockMemberData& flockMember)
{
	FVector newVec_ = FVector(0, 0, 0);

	for (int i = 0; i < dangerActorsTHR.Num(); ++i)
	{
		if (dangerActorsTHR[i])
		{
			// calculate flee from this threat
			FVector fromEnemy_ = dangerActorsTHR[i]->GetActorLocation() - flockMember.transform.GetLocation();
			float const distanceToEnemy_ = fromEnemy_.Size();
			fromEnemy_.Normalize();

			// enemy inside our enemy awareness threshold, so evade them
			if (distanceToEnemy_ < flockParametersTHR.followPawnAwarenessRadius)
			{
				newVec_ += fromEnemy_ * ((flockParametersTHR.followPawnAwarenessRadius / distanceToEnemy_) * flockParametersTHR.fleeScale);

				// Add attacked actors in array.
				if (flockParametersTHR.bCanAttackPawn)
				{
					// Add attacked actors in array.
					if (flockParametersTHR.bCanAttackPawn)
					{
						if ((dangerActorsTHR[i]->GetActorLocation() - flockMember.transform.GetLocation()).SizeSquared() < flockParametersTHR.attackRadiusSquared)
						{
							flockMember.attackedActors.AddUnique(dangerActorsTHR[i]);
						}
					}
				}
			}
		}
	}
	return newVec_;
}

void FlockThread::InitFlockLeader()
{
	if (flockParametersTHR.followActor)
	{
		for (int i = 0; i < PoolThreadArr.Num(); i++)
		{
			PoolThreadArr[i]->flockThreadMembersArr[0].bIsFlockLeader = false;
		}
	}
	else if (flockParametersTHR.bUseOneLeader)
	{
		for (int i = 0; i < PoolThreadArr.Num(); i++)
		{
			PoolThreadArr[i]->flockThreadMembersArr[0].bIsFlockLeader = false;
		}
		PoolThreadArr[0]->flockThreadMembersArr[0].bIsFlockLeader = true;
	}
}

void FlockThread::SetPoolThread(TArray<FlockThread*> setPoolThreadArr)
{
	PoolThreadArr = setPoolThreadArr;
}

FVector FlockThread::SteeringAvoidance(const FlockMemberData& flockMember) const
{
	FVector newVec_(FVector::ZeroVector);

	for (int i = 0; i < avoidanceActorRootArrTHR.Num(); ++i)
	{
		if (avoidanceActorRootArrTHR[i])
		{
			UPrimitiveComponent* primComp_(Cast<UPrimitiveComponent>(avoidanceActorRootArrTHR[i]->GetRootComponent()));
			if (primComp_)
			{
				FVector closestPoint_;
				primComp_->GetClosestPointOnCollision(flockMember.transform.GetLocation(), closestPoint_);

				if ((closestPoint_ - flockMember.transform.GetLocation()).Size() < flockParametersTHR.avoidancePrimitiveDistance)
				{
					FRotator const rotationToCenter_(UKismetMathLibrary::FindLookAtRotation(closestPoint_, flockMember.transform.GetLocation()));
					FVector direction_ = UKismetMathLibrary::Conv_RotatorToVector(rotationToCenter_);
					direction_.Normalize();

					newVec_ = direction_ * ((flockParametersTHR.flockEnemyAwarenessRadius / flockParametersTHR.strengthAquariumOffsetValue) * flockParametersTHR.fleeScaleAquarium);
				}
			}
		}
	}

	return newVec_;
}

void AFlockSystemActor::DivideFlockArrayForThreads()
{
	TArray<FlockMemberData> tempFlockData_;
	FlockMembersArrays newFlockData_;

	if (maxUseThreads > flockMateInstances)
	{
		newFlockData_.AddData(0, tempFlockData_);
		allFlockMembersArrays.Add(newFlockData_);
		allFlockMembersArrays[0].flockMembersArr = mFlockMemberData;
		return;
	}

	int flockID_(0);
	int flockPart_ = mFlockMemberData.Num() / maxUseThreads;
	int const flockPartIncrement_ = flockPart_;

	for (int i = 0; i < maxUseThreads; i++)
	{
		for (; flockID_ < flockPart_; flockID_++)
		{
			tempFlockData_.Add(mFlockMemberData[flockID_]);
		}

		newFlockData_.AddData(i, tempFlockData_);
		allFlockMembersArrays.Add(newFlockData_);
		tempFlockData_.Empty();

		flockPart_ += flockPartIncrement_;
	}

	// Adding the remainder of the division.
	if (flockID_ < mFlockMemberData.Num() - 1 && flockID_ == (flockPartIncrement_ * maxUseThreads))
	{
		for (; flockID_ < mFlockMemberData.Num(); flockID_++)
		{
			allFlockMembersArrays[maxUseThreads - 1].flockMembersArr.Add(mFlockMemberData[flockID_]);
		}
	}
}

void AFlockSystemActor::AddFlockMemberWorldSpace(const FTransform& WorldTransform)
{
	StaticMeshInstanceComponent->AddInstance(WorldTransform, true);

	FlockMemberData flockMember_;
	flockMember_.instanceIndex = numFlock;
	flockMember_.transform = WorldTransform;
	//   flockMember_.velocity = flockMember_.wanderPosition - flockMember_.transform.GetLocation();
	if (numFlock == 0)
	{
		flockMember_.bIsFlockLeader = true;
	}
	mFlockMemberData.Add(flockMember_);
	numFlock++;
}

TArray<int32> FlockThread::GetNearbyFlockMates(int32 flockMember)
{
	TArray<int32> mates;
	if (flockMember > flockThreadMembersArr.Num()) return mates;
	if (flockMember < 0) return mates;

	for (int32 i = 0; i < flockThreadMembersArr.Num(); i++)
	{
		if (i != flockMember)
		{
			FVector diff = flockThreadMembersArr[i].transform.GetLocation() - flockThreadMembersArr[flockMember].transform.GetLocation();
			if (FMath::Abs(diff.Size()) < flockParametersTHR.flockMateAwarenessRadius)
			{
				mates.Add(i);
			}
		}
	}

	return mates;
}
