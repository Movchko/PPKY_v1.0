#ifndef INC_CONFIG_IGN_BLOCK_SYNC_H_
#define INC_CONFIG_IGN_BLOCK_SYNC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void ConfigIgnBlockSync_Init(void);
void ConfigIgnBlockSync_Process1ms(uint32_t now_ms);
void ConfigIgnBlockSync_Request(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_CONFIG_IGN_BLOCK_SYNC_H_ */
