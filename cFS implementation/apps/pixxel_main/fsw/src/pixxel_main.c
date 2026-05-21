#include "cfe.h"
#include "pixxel_msgids.h"

/* Structure to decode tracking frames received on the pipe */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader;
    uint32                    DeviceStatus;
    uint32                    ResponseType;
} PIXXEL_IncomingTlm_t;

void PIXXEL_MAIN_AppMain(void)
{
    uint32                  status;
    CFE_SB_PipeId_t         MainPipe;
    CFE_SB_Buffer_t        *MsgBuffer;
    CFE_MSG_CommandHeader_t CmdMsg;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    CFE_EVS_SendEvent(1, CFE_EVS_EventType_INFORMATION, "Pixxel Main Orchestrator Activated");

    CFE_SB_CreatePipe(&MainPipe, 4, "PIXXEL_MAIN_PIPE");
    CFE_SB_Subscribe(CFE_SB_ValueToMsgId(PIXXEL_TLM_MID), MainPipe);

    /* Requirement: Start execution after pixxel_controller has started */
    OS_TaskDelay(500);

    /* Requirement: Upon execution, command pixxel_controller to enable the device */
    CFE_MSG_Init(&CmdMsg.Msg, CFE_SB_ValueToMsgId(PIXXEL_CMD_MID), sizeof(CmdMsg));
    CFE_MSG_SetFcnCode(&CmdMsg.Msg, PIXXEL_CMD_ENABLE_CC);
    CFE_SB_TransmitMsg(&CmdMsg.Msg, true);

    bool ackReceived     = false;
    bool processComplete = false;

    while (CFE_ES_RunLoop(&status) == true && !processComplete)
    {
        int32 receiveStatus = CFE_SB_ReceiveBuffer(&MsgBuffer, MainPipe, 1000);

        if (receiveStatus == CFE_SUCCESS)
        {
            CFE_SB_MsgId_t MsgId;
            CFE_MSG_GetMsgId(&MsgBuffer->Msg, &MsgId);

            if (CFE_SB_MsgIdToValue(MsgId) == PIXXEL_TLM_MID)
            {
                PIXXEL_IncomingTlm_t *TlmPtr = (PIXXEL_IncomingTlm_t *)MsgBuffer;

                /* Requirement: Receive acknowledgment from pixxel_controller */
                if (TlmPtr->ResponseType == 100 && !ackReceived)
                {
                    CFE_EVS_SendEvent(2, CFE_EVS_EventType_INFORMATION, "Main: Driver Enable ACK Verified");
                    ackReceived = true;

                    /* Requirement: Wait for fifty milliseconds */
                    /* Crucial: This allows the driver's internal 50ms kernel timer callback to execute! */
                    OS_TaskDelay(50);

                    /* Requirement: Command pixxel_controller again to retrieve the status */
                    CFE_MSG_SetFcnCode(&CmdMsg.Msg, PIXXEL_CMD_GET_STAT_CC);
                    CFE_SB_TransmitMsg(&CmdMsg.Msg, true);
                }

                /* Requirement: Check status of the device and ensure it is enabled */
                else if (TlmPtr->ResponseType == 200 && ackReceived)
                {
                    if (TlmPtr->DeviceStatus == 1)
                    {
                        CFE_EVS_SendEvent(3,
                                          CFE_EVS_EventType_INFORMATION,
                                          "Main Success Verification: Device status returns ENABLED (1)");
                    }
                    else
                    {
                        CFE_EVS_SendEvent(4,
                                          CFE_EVS_EventType_ERROR,
                                          "Main Error Verification: Device status is STALE or DISABLED (0)");
                    }
                    processComplete = true; /* State sequence fully parsed */
                }
            }
        }
    }

    /* Passive keep-alive loop */
    while (CFE_ES_RunLoop(&status) == true)
    {
        OS_TaskDelay(1000);
    }

    CFE_ES_ExitApp(status);
}