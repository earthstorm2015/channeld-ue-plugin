#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "ChanneldMissionNotiProxy.generated.h"

class FChanneldProcWorkerThread;

DECLARE_MULTICAST_DELEGATE(FMissionCanceled);
/**
 * 
 */
UCLASS()
class CHANNELDEDITOR_API UChanneldMissionNotiProxy : public UObject
{
	GENERATED_UCLASS_BODY()
public:
	virtual void ReceiveOutputMsg(FChanneldProcWorkerThread* Worker, const FString& InMsg);
	virtual void SpawnRunningMissionNotification(FChanneldProcWorkerThread* ProcWorker);
	virtual void SpawnMissionSucceedNotification(FChanneldProcWorkerThread* ProcWorker);
	virtual void SpawnMissionFailedNotification(FChanneldProcWorkerThread* ProcWorker);
	virtual void CancelMission();

	virtual FText GetRunningNotifyText() const;
	virtual FText GetRunningNotifyCancelText() const;
	virtual FText GetMissionSucceedNotifyText() const;
	virtual FText GetMissionFailedNotifyText() const;

	virtual void SetRunningNotifyText(const FText& RunningNotifyText);
	virtual void SetRunningNotifyCancelText(const FText& RunningNotifyCancelText);
	virtual void SetMissionSucceedNotifyText(const FText& MissionSucceedNotifyText);
	virtual void SetMissionFailedNotifyText(const FText& MissionFailedNotifyText);

	virtual void SetMissionNotifyText(const FText& RunningText, const FText& CancelText, const FText& SucceedText, const FText& FailedText);

	FMissionCanceled MissionCanceled;

protected:
	TWeakPtr<SNotificationItem> PendingProgressPtr;
	bool bRunning = false;
	FText RunningNotifyText;
	FText RunningNotifyCancelText;
	FText MissionSucceedNotifyText;
	FText MissionFailedNotifyText;
	FName MissionName;
};

#define GO_ZAP_LOGGER_LEVEL_DEBUG "DEBUG"
#define GO_ZAP_LOGGER_LEVEL_INFO "INFO"
#define GO_ZAP_LOGGER_LEVEL_WARN "WARN"
#define GO_ZAP_LOGGER_LEVEL_ERROR "ERROR"
#define GO_ZAP_LOGGER_LEVEL_DPANIC "DPANIC"
#define GO_ZAP_LOGGER_LEVEL_PANIC "PANIC"
#define GO_ZAP_LOGGER_LEVEL_FATAL "FATAL"

UCLASS()
class CHANNELDEDITOR_API UChanneldGetawayNotiProxy : public UChanneldMissionNotiProxy
{
	GENERATED_BODY()

public:
	virtual void ReceiveOutputMsg(FChanneldProcWorkerThread* Worker, const FString& InMsg) override;

protected:
	FString PrevLogLevel = GO_ZAP_LOGGER_LEVEL_INFO;
};
