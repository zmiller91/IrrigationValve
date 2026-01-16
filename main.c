/**
 * A few things to note here: 
 * 
 * 1. This file must contain the config bits from MCC
 * 2. Otherwise this file delegates the configuration to the BaseBoardCore package
 * 
 * In order to link the two, you'll need to:
 * 
 * 1. Right Click "Libraries" and add the BaseBoardCore.x library
 * 2. Go to Properties -> XC8 Compiler -> Include Directories and add the 
 *    BaseBoardCore.x library
 * 
 */

#define ENABLE_DEBUG 0
#define ENABLE_INFO  0
#define ENABLE_ERROR 0

#include "run.h"
#include "log.h"
#include "lora.h"
#include "timeout.h"
#include "adcc_manager.h"
#include <stdio.h>
#include "mcc_generated_files/system/config_bits.h"


void task(void) {
    
    lora_enable();
    __delay_ms(2000);
    
    // uart_read uses timeout_wait which has a minimum timeout period. Since we
    // don't have a clock so we don't really know how much time is passed. The
    // best we can do is estimate. We'll just take how however long the timeout
    // period is, use it to find how many times we should retry. Adding one
    // extra retry to avoid any integer math rounding down. 
    uint16_t timeout_duration_ms = timeout_period_ms();
    uint16_t retries = 60000 / timeout_duration_ms + 1;
 
    LoraRcv message;
    char line_buf[64];
    uint8_t iteration_count = 0;
    bool has_data = false;
    
    send_rx_open(32, 60);
    while(!has_data && iteration_count < retries) {
        has_data = lora_receive(line_buf, 64, &message) == DATA_RECEIVED;
        iteration_count++;
    }
    
    if(has_data) {
        send_ack(32, message.msg_id);
        
        // TODO: Perform action
        log_info("Performing action...");
        
        __delay_ms(500);
        send_command_complete(32, message.msg_id);
    }   
}
 
int main(void) {
    return run(task);   
}