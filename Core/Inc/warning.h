#ifndef INC_WARNING_H_
#define INC_WARNING_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Обработка предупреждений/неисправностей виртуальных устройств. */
void WarningProcess1ms(void);
void Warning_SetPowerFaultMask(uint8_t mask);
uint8_t Warning_HasActiveFault(void);

#ifdef __cplusplus
}
#endif

#endif /* INC_WARNING_H_ */

