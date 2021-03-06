// Fill out your copyright notice in the Description page of Project Settings.

#include "VRInteractPlatform.h"
#include "CentralCommunicator.h"


// Sets default values
ACentralCommunicator::ACentralCommunicator()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ACentralCommunicator::BeginPlay()
{
	Super::BeginPlay();
	
	SocketInit();

	// Trace of Robots in level
	TArray<AActor*> FoundActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ABaseRobot::StaticClass(), FoundActors);
	for(auto Actor : FoundActors)
	{
		ABaseRobot* BaseRobot = Cast<ABaseRobot>(Actor);
		Name2RobotMap.Emplace(BaseRobot->GetActorLabel(), BaseRobot);
	}
}

void ACentralCommunicator::BeginDestroy()
{
	Super::BeginDestroy();

	SocketDestroy();
}

// Called every frame
void ACentralCommunicator::Tick( float DeltaTime )
{
	Super::Tick( DeltaTime );

}

void ACentralCommunicator::OnTcpBuildListenerConnection()
{
	if (ListenerSocket != NULL) {

        TcpListener = new FTcpListener(*ListenerSocket);
        TcpListener->OnConnectionAccepted().BindUObject(this, &ACentralCommunicator::OnTcpListenerConnectionAccepted);

        if (TcpListener->Init()) {
            // GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("Initialize listener!")));
            // We are now accepting clients.
            Accepting = true;
        }
    }
    else {
        GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("Failed initializing listener!")));
    }
}

bool ACentralCommunicator::OnTcpListenerConnectionAccepted(FSocket* Socket, const FIPv4Endpoint& Endpoint)
{
	if (Accepting == true) {
        // We donot accept any more connection.
        // Accepting = false;
        // GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("INCOMING CONNECTION!")));

        auto State = Socket->GetConnectionState();

        if (State == ESocketConnectionState::SCS_Connected) {
            // GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("CONNECTTED")));
        }

        // Start another thread listening on this connection.
        ConnectionSocket = Socket;
        Receiver = new TCPReceiver(Socket, UB_TCP_BUFFER_SIZE);
        Receiver->OnTcpReceive().BindUObject(this, &ACentralCommunicator::OnTcpReceived);
        Receiver->Init();
        return true;
    }
    // Reject the connection.
    return false;
}

int32 ACentralCommunicator::OnTcpReceived(const uint8* Data, int32 BytesReceived)
{
	// This is a static buffer for convert the data.
    // Notice this function should be called only on single thread.
    static uint8 buffer[UB_TCP_BUFFER_SIZE];

    // This is a static frame used for parsing.
    // Notice this function should be called only on single thread.
    //static Frame frame_buffer[UB_BUFFER_FRAME_SIZE];

    int32 consumed = 0;
    int32 previous = 0;
    int32 count = 0;    // How many frames we have parsed?
                        // Looking for line breaker of '\n'
    for (int i = 0; i < BytesReceived; ++i) {
        // if (count > UB_BUFFER_FRAME_SIZE) {
        //     // We have reached the capacity of our parsing buffer.
        //     // Wait for next time.
        //     break;
        // }
        if (Data[i] == '\n') {
            // copy the data back.
            FGenericPlatformMemory::Memmove(reinterpret_cast<void*>(buffer), reinterpret_cast<const void*>(Data + previous), i - previous);
            // Add a zero.
            buffer[i - previous] = 0;

            //JSON String
            FString FJSONData(ANSI_TO_TCHAR(reinterpret_cast<const char*>(buffer)));
            std::string JSONData(TCHAR_TO_UTF8(*FJSONData));
            UE_LOG(LogTemp, Warning, TEXT("json data: %s"), *FString(JSONData.c_str()));

            Document doc;
            doc.Parse(JSONData.c_str());
            std::string RobotName = doc["RobotName"].GetString();
            FString FRobotName(RobotName.c_str());
            ABaseRobot* BaseRobot = Name2RobotMap[FRobotName];
            if(BaseRobot)
            	BaseRobot->RecvRobotData(JSONData);

            // Consume everything including the '\n'.
            consumed = i + 1;

            // Reset the previous indexer.
            previous = i + 1;
        }
    }

    // GEngine->AddOnScreenDebugMessage(-1, 1.0f, FColor::Red, FString::Printf(TEXT("Received %d frames!"), count));
    // UE_LOG(LogTemp, Warning, TEXT("%s"), *FString::Printf(TEXT("received %d frames!"), count));
    return consumed;
}