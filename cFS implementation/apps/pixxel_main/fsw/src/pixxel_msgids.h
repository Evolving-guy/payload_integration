#ifndef _PIXXEL_MSGIDS_H_
#define _PIXXEL_MSGIDS_H_

#include "cfe_core_api_base_msgids.h"

/* Dropped 'AL' from macros to match your cFS platform header definitions */
#define PIXXEL_CMD_MID CFE_PLATFORM_CMD_TOPICID_TO_MIDV(0x8C) /* Incoming commands to controller */
#define PIXXEL_TLM_MID CFE_PLATFORM_TLM_TOPICID_TO_MIDV(0x8D) /* Outbound feedback from controller */

/* Explicit command operations required by the state machine */
#define PIXXEL_CMD_NOOP_CC     0
#define PIXXEL_CMD_ENABLE_CC   1 /* Command to flip enable register to 1 */
#define PIXXEL_CMD_GET_STAT_CC 2 /* Command to parse the physical status register */

#endif /* _PIXXEL_MSGIDS_H_ */