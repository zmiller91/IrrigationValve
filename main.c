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
#define TMR6_TICKS_PER_MINUTE 1196

#include "run.h"
#include "log.h"
#include "lora.h"
#include "timeout.h"
#include "adcc_manager.h"
#include "string_utils.h"
#include <stdio.h>
#include "mcc_generated_files/system/config_bits.h"
#include <string.h>




static bool job_in_progress = false;
static char job_id[LORA_MAX_MSGID + 1] = {0};

volatile uint16_t tmr6_tick_count = 0;
volatile uint32_t minutes_elapsed = 0;
volatile uint16_t job_duration = 0;

/**
 * TMR6 overflow callback used as a coarse software clock.
 *
 * TMR6 is configured to overflow approximately every 50 ms.
 * This callback accumulates those overflows and converts them
 * into elapsed minutes.
 *
 * Intended for very low-cost time tracking on the
 * PIC16F18076
 * without requiring an RTC or precise crystal timing.
 *
 * Timing Notes:
 * - 1 TMR6 overflow ? 50 ms
 * - ~1196 overflows ? 1 minute
 *
 * Usage:
 *
 *   TMR6_OverflowCallbackRegister(tmr6_tick);
 *
 * Example:
 *
 *   uint32_t start = minutes_elapsed;
 *
 *   // later...
 *
 *   if((minutes_elapsed - start) >= 15)
 *   {
 *       // 15 minutes passed
 *   }
 *
 * Overflow Safety:
 * - Uses unsigned arithmetic
 * - Safe across uint32_t rollover
 * - uint32_t minute counter overflows after ~8000 years
 */
void tmr6_tick(void)
{
    tmr6_tick_count++;

    if(tmr6_tick_count >= TMR6_TICKS_PER_MINUTE)
    {
        tmr6_tick_count = 0;
        minutes_elapsed++;
    }
}


static inline void complete_job() {
    
    LED_EN_SetLow();
    TMR6_Stop();
    TMR6_Write(0);
    tmr6_tick_count = 0;
    minutes_elapsed = 0;
    job_duration = 0;
    
    if(job_in_progress) {
        send_command_complete(32, job_id);
        job_in_progress = false;
        job_id[0] = '\0';
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
}// --- helpers ---
static inline void pulse_srclk(void)
{
    IO3_SetHigh();
    __delay_ms(15);
    IO3_SetLow();
}

static inline void pulse_rclk(void)
{
    IO2_SetHigh();
    __delay_ms(15);
    IO2_SetLow();
}

// Send one byte to 74HC595 (MSB first by default)
static void hc595_write_byte(uint8_t value)
{
    for (int i = 7; i >= 0; i--) {
        if (value & (1u << i)) IO1_SetHigh();
        else                   IO1_SetLow();

        pulse_srclk(); // shift this bit in on rising edge
    }

    pulse_rclk(); // latch to outputs
}

// Call once at startup
void hc595_init(void)
{
    IO1_SetDigitalOutput(); // SER
    IO2_SetDigitalOutput(); // RCLK
    IO3_SetDigitalOutput(); // SRCLK

    // Known idle states (prevents accidental clocks/latches)
    IO1_SetLow();
    IO2_SetLow();
    IO3_SetLow();

    // If you wired SRCLR and OE:
    // SRCLR should be HIGH (inactive), OE should be LOW (enabled)
    // Otherwise tie SRCLR to VCC, OE to GND in hardware.
}

/**
 * Lightweight string comparison for embedded systems.
 *
 * Compares two null-terminated strings and returns:
 *   1 = strings are equal
 *   0 = strings are not equal
 *
 * Designed for small MCUs like the
 * PIC16F18076
 * where the standard string library may consume excessive
 * program memory or stack space.
 *
 * Example:
 *
 *   if(strcmpr(message.payload, "1007"))
 *   {
 *       // matched
 *   }
 */
static inline int8_t strcmpr(const char *a, const char *b)
{
    while(*a && *b)
    {
        if(*a != *b)
        {
            return 0;
        }

        a++;
        b++;
    }

    return (*a == '\0' && *b == '\0');
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
    
    // TURN_OFF action
    if(strcmpr(message.payload, "1008")) {
        
        // Complete the current job
        complete_job();
            
        // Complete the TURN_OFF job
        job_in_progress = true;
        strcpy(job_id, message.msg_id);
        complete_job();
            return;
        return;
    
    // TURN_ON action
    } else if(strcmpr(message.payload, "1007")) {
        
        strcpy(job_id, message.msg_id);
        if(duration == NULL) {
            complete_job();
            return;
        }
        
        job_in_progress = true;
        job_duration = parse_u16(duration);
        TMR6_Write(0);
        tmr6_tick_count = 0;
        minutes_elapsed = 0;
        TMR6_Start();
        return;
    }
    
//    
//    
//    
//    // VCC
//    // GND
//    // SER -> IO1
//    // RCLK -> IO2
//    // SRCLK -> IO3
//        hc595_init();
//
//    // Q0=1, others 0
//    hc595_write_byte(0b00001001);
//    __delay_ms(1000);
//    hc595_write_byte(0b00001100);
//    __delay_ms(1000);
//    hc595_write_byte(0b00000101);
//    __delay_ms(1000);
//    hc595_write_byte(0b00000011);
//    __delay_ms(1000);
//    hc595_write_byte(0b00000110);
//    __delay_ms(1000);
//    hc595_write_byte(0b00000000);
//    
//    
//    
//    // Shift all bits out of the register
//    // Charge the capacitor
//    // Shift bits into the register
//    // Charge the capacitor
//    
//    
//    
//    
//    
//    
//    
//    
//    job_time_remaining_min = parse_u16(duration);
//    LED_EN_SetHigh();
}

// Wake up periodically to check the status of the job. We are using this 
// functionality as our clock, since we don't have an external one. We want to
// wake up every 15 minutes and then if less than 15 minutes are left we will
// wake up either 5 minutes or 1 minute, depending on what time allows.
//static inline sleep_period get_sleep_period() {
//    
//    if(!job_in_progress) {
//        return THIRTY_MINUTES;
//    }
//      
//    if(job_time_remaining_min >= 15) {
//        job_sleep_period_min = 15;
//        return FIFTEEN_MINUTES;
//        
//    }else if(job_time_remaining_min >= 5) {
//        job_sleep_period_min = 5;
//        return FIVE_MINUTES;
//        
//    } else {
//        job_sleep_period_min = 1;
//        return ONE_MINUTE;  
//    } 
//}

static inline void check_job_status() {
    if(job_in_progress && job_duration <= minutes_elapsed) {
        complete_job();
    }
}

static inline int8_t send_test() {

    char buff1[] = "LCLTST::";
    const char *buff2;

    if(job_in_progress) {
        buff2 = "OFF";
    } else {
        buff2 = "ON";
    }

    char out[14];
    char *parts[] = { buff1, (char*) buff2 };
    join_buffers(parts, 2, out, 14);
    
    return lora_send_raw(32, out);
}

static inline int8_t send_lclreq() {

    char buff1[] = "LCLREQ::";
    const char *buff2;

    if(job_in_progress) {
        buff2 = "OFF";
    } else {
        buff2 = "ON";
    }

    char out[14];
    char *parts[] = { buff1, (char*) buff2 };
    join_buffers(parts, 2, out, 14);
    
    return lora_send_raw(32, out);
}

static inline int8_t send_lclovr() {

    char buff1[] = "LCLOVR::";
    const char *buff2;

    if(job_in_progress) {
        buff2 = "OFF";
    } else {
        buff2 = "ON";
    }

    char out[14];
    char *parts[] = { buff1, (char*) buff2 };
    join_buffers(parts, 2, out, 14);
    
    return lora_send_raw(32, out);
}


void task(void) {
    
    lora_enable();
    IO2_SetDigitalInput();
    TMR6_OverflowCallbackRegister(tmr6_tick);
    TMR6_Stop();
    TMR6_Write(0);
    tmr6_tick_count = 0;
    minutes_elapsed = 0;
    
    __delay_ms(2000);
    
    while(true) {
        
        
        
        /**
         * Everything in this block is about recognizing the button press
         * and sending the manual commands. It's not great. 
         */
        if(IO2_GetValue()) {
            
            LED_EN_SetHigh();
            
            uint8_t count = 0;
            while(IO2_GetValue()) {
                __delay_ms(25);
                count++;
                if(count >= 120) {
                    break;
                }
            }
            
            if(count >= 120) {
                send_lclovr();
            } else {
                send_lclreq();
            }
            
            
            while(IO2_GetValue()) {
                // spin until the button is released
            }
            
        } else {
            LED_EN_SetLow();
        }

        /**
         * Everything in this block is about starting or stopping jobs
         */
        LoraRcv message;
        char line_buf[64];    
        if(lora_has_data()) {
            if(lora_receive(line_buf, 64, &message) == DATA_RECEIVED) {
                send_test();
                send_ack(32, message.msg_id);
                perform_action(message);
            }
        }

        if(job_in_progress) {
            check_job_status();
        }
    }
}
 
int main(void) {
    return run(task);   
}