#include "cfe.h"
#include "pixxel_msgids.h"
#include <fcntl.h>
#include <unistd.h>

/* Sized telemetry payload structure to prevent SB length parsing alerts */
typedef struct
{
    CFE_MSG_TelemetryHeader_t TlmHeader;
    uint32                    DeviceStatus; /* Directly mirrors kernel's status_reg value */
    uint32                    ResponseType; /* 100 = Acknowledgement, 200 = Updated Status */
} PIXXEL_CtrlTlm_t;

void PIXXEL_CTRL_AppMain(void)
{
    uint32           status;
    CFE_SB_PipeId_t  CtrlPipe;
    CFE_SB_Buffer_t *MsgBuffer;
    PIXXEL_CtrlTlm_t OutMsg;
    int              fd;

    CFE_EVS_Register(NULL, 0, CFE_EVS_EventFilter_BINARY);
    CFE_EVS_SendEvent(1, CFE_EVS_EventType_INFORMATION, "Pixxel Controller Registered and Ready");

    /* Properly configure the message structure parameters */
    CFE_MSG_Init(&OutMsg.TlmHeader.Msg, CFE_SB_ValueToMsgId(PIXXEL_TLM_MID), sizeof(OutMsg));

    /* Spin up the message pipeline */
    CFE_SB_CreatePipe(&CtrlPipe, 4, "PIXXEL_CTRL_PIPE");
    CFE_SB_Subscribe(CFE_SB_ValueToMsgId(PIXXEL_CMD_MID), CtrlPipe);

    while (CFE_ES_RunLoop(&status) == true)
    {
        int32 receiveStatus = CFE_SB_ReceiveBuffer(&MsgBuffer, CtrlPipe, 1000);

        if (receiveStatus == CFE_SUCCESS)
        {
            CFE_SB_MsgId_t MsgId;
            CFE_MSG_GetMsgId(&MsgBuffer->Msg, &MsgId);

            if (CFE_SB_MsgIdToValue(MsgId) == PIXXEL_CMD_MID)
            {
                CFE_MSG_FcnCode_t CommandCode;
                CFE_MSG_GetFcnCode(&MsgBuffer->Msg, &CommandCode);

                /* HANDLER 1: Command pixxel_controller to enable the device */
                if (CommandCode == PIXXEL_CMD_ENABLE_CC)
                {
                    fd = open("/dev/pixxel", O_WRONLY); /* Matches DEVICE_NAME in driver */
                    if (fd >= 0)
                    {
                        /* Driver Requirement: Write payload must be a complete 4-byte 32-bit integer */
                        uint32 enable_val = 1;
                        write(fd, &enable_val, sizeof(enable_val));
                        close(fd);
                    }

                    /* Requirement: Send Acknowledgment back to pixxel_main */
                    OutMsg.ResponseType = 100; /* Flag identifying an ACK response */
                    OutMsg.DeviceStatus = 0;   /* Driver timer is still ticking, state isn't high yet */
                    CFE_SB_TransmitMsg(&OutMsg.TlmHeader.Msg, true);
                    CFE_EVS_SendEvent(2, CFE_EVS_EventType_INFORMATION, "Controller: Written to driver, ACK posted");
                }

                /* HANDLER 2: Command pixxel_controller to retrieve the status */
                else if (CommandCode == PIXXEL_CMD_GET_STAT_CC)
                {
                    fd = open("/dev/pixxel", O_RDONLY);
                    if (fd >= 0)
                    {
                        /* Driver Requirement: Securely extracts 32 bits from physical status register */
                        read(fd, &OutMsg.DeviceStatus, sizeof(OutMsg.DeviceStatus));
                        close(fd);
                    }

                    OutMsg.ResponseType = 200; /* Flag identifying a telemetry data delivery */
                    CFE_SB_TransmitMsg(&OutMsg.TlmHeader.Msg, true);
                    CFE_EVS_SendEvent(3, CFE_EVS_EventType_INFORMATION, "Controller: Hardware Status read complete");
                }
            }
        }
    }

    CFE_ES_ExitApp(status);
}