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
#include <string.h>




static bool job_in_progress = false;
static char job_id[LORA_MAX_MSGID + 1] = {0};
static uint16_t job_time_remaining_min = 0;
static uint8_t job_sleep_period_min = 0;

static inline void complete_job() {
    LED_EN_SetLow();
    if(job_in_progress) {
        send_command_complete(32, job_id);
        job_in_progress = false;
        job_id[0] = '\0';
        job_sleep_period_min = 0;
        job_time_remaining_min = 0;
    }
}

static inline uint16_t parse_u16(const char *s) {
    if (!s) return 0;
    
    uint16_t v = 0;
    while (*s >= '0' && *s <= '9') {
        v = (v * 10) + (*s - '0');
        s++;
    }
    return v;
}

static inline void perform_action(LoraRcv message) {
    log_info("Performing action...");
    log_info(message.payload);
    
    char *action_id = message.payload;
    char *duration = NULL;
  
    // Iterate through the payload to split on :: so we can separate the action
    // ID and the action parameter (e.g. duration)
    char *ptr = message.payload;
    while(*ptr) {
        if(ptr[0] == ':' && ptr[1] == ':') {
            *ptr = '\0';
            duration = ptr + 2;
            break;
        }
        
        ptr++;
    }
    
    if(duration == NULL) {
        complete_job();
        return;
    }
    
    job_time_remaining_min = parse_u16(duration);
    LED_EN_SetHigh();
}

// Wake up periodically to check the status of the job. We are using this 
// functionality as our clock, since we don't have an external one. We want to
// wake up every 15 minutes and then if less than 15 minutes are left we will
// wake up either 5 minutes or 1 minute, depending on what time allows.
static inline sleep_period get_sleep_period() {
    
    if(!job_in_progress) {
        return THIRTY_MINUTES;
    }
      
    if(job_time_remaining_min >= 15) {
        job_sleep_period_min = 15;
        return FIFTEEN_MINUTES;
        
    }else if(job_time_remaining_min >= 5) {
        job_sleep_period_min = 5;
        return FIVE_MINUTES;
        
    } else {
        job_sleep_period_min = 1;
        return ONE_MINUTE;  
    } 
}

static inline void check_job_status() {
    
    if(job_sleep_period_min > job_time_remaining_min || 
            job_sleep_period_min == job_time_remaining_min) {
        complete_job();
        return;
    }
    
    job_time_remaining_min = job_time_remaining_min - job_sleep_period_min;
}

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
    
    if(!job_in_progress) {
        send_rx_open(32, 60);
        while(!has_data && iteration_count < retries) {
            has_data = lora_receive(line_buf, 64, &message) == DATA_RECEIVED;
            iteration_count++;
        }

        if(has_data) {
            send_ack(32, message.msg_id);
            job_in_progress = true;
            strcpy(job_id, message.msg_id);
            perform_action(message);
        }  
        
    } else {
        check_job_status();
    }
    
    
    set_sleep_period(get_sleep_period());
}
 
int main(void) {
    return run(task);   
}