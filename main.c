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

#define TICKS_PER_MINUTE 1200

#define MCP23017_ADDR  0x20
#define MCP_IODIRA     0x00
#define GROUP_COUNT 8
#define MCP_OLATA 0x14

#define MCP_IODIRA 0x00
#define MCP_IODIRB 0x01
#define MCP_GPPUB  0x0D
#define MCP_GPIOA  0x12
#define MCP_GPIOB  0x13

#define MCP_GPINTENB 0x05
#define MCP_DEFVALB  0x07
#define MCP_INTCONB  0x09
#define MCP_IOCON    0x0A
#define MCP_INTCAPB  0x11

#include "run.h"
#include "log.h"
#include "lora.h"
#include "timeout.h"
#include "adcc_manager.h"
#include "i2c_manager.h"
#include "string_utils.h"
#include <stdio.h>
#include "mcc_generated_files/system/config_bits.h"
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

volatile uint16_t timer_ticks = 0;
volatile bool minute_elapsed = false;

typedef struct {
    uint8_t index;

    // Output pin on GPIOA
    uint8_t output_pin;

    // Input pin on GPIOB
    uint8_t input_pin;

    // Job tracking
    char job_id[LORA_MAX_MSGID + 1];
    uint16_t duration_minutes;
    uint16_t elapsed_minutes;

    // Runtime state
    bool active;
    bool pending_complete;
} io_group_t;

static io_group_t groups[GROUP_COUNT];

/**
 * Mapping:
 *
 * Group 0 -> GPA0 / GPB7
 * Group 1 -> GPA1 / GPB6
 * Group 2 -> GPA2 / GPB5
 * Group 3 -> GPA3 / GPB4
 * Group 4 -> GPA4 / GPB3
 * Group 5 -> GPA5 / GPB2
 * Group 6 -> GPA6 / GPB1
 * Group 7 -> GPA7 / GPB0
*/

#define MCP_IODIRA   0x00
#define MCP_IODIRB   0x01

#define MCP_GPINTENB 0x05
#define MCP_DEFVALB  0x07
#define MCP_INTCONB  0x09

#define MCP_GPPUB    0x0D
#define MCP_GPIOA    0x12
#define MCP_GPIOB    0x13
#define MCP_OLATA 0x14

#define MCP_INTFB   0x0F
#define MCP_INTCAPB 0x11

void timer_callback(void) {
    
    timer_ticks++;

    if(timer_ticks >= TICKS_PER_MINUTE) {
        timer_ticks = 0;
        minute_elapsed = true;
    }
}

static void log_reg_value(const char *name, uint8_t value) {

//    char hex[3];
//    byte_to_hex2(value, hex);
//
//    char msg[32];
//
//    char *parts[] = {
//        (char*) name,
//        "=",
//        hex
//    };
//
//    join_buffers(parts, 3, msg, sizeof(msg));
//    log_info(msg);
}

static void verify_register(uint8_t reg_addr, const char *name) {
//
//    uint8_t reg = reg_addr;
//    uint8_t value = 0x00;
//
//    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &value, 1)) {
//        char msg[32];
//
//        char *parts[] = {
//            "Read failed: ",
//            (char*) name
//        };
//
//        join_buffers(parts, 2, msg, sizeof(msg));
//        log_info(msg);
//        return;
//    }
//
//    log_reg_value(name, value);
}

void groups_init(void) {

    uint8_t buf[2];

    log_info("groups_init start");

    //
    // IOCON: open-drain INT, active-low
    //
    buf[0] = MCP_IOCON;
    buf[1] = 0b00000100;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed IOCON write");
    }

    verify_register(MCP_IOCON, "IOCON");

    //
    // Disable interrupts during config
    //
    buf[0] = MCP_GPINTENB;
    buf[1] = 0x00;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed GPINTENB disable");
    }

    verify_register(MCP_GPINTENB, "GPINTENB");

    //
    // GPIOA outputs
    //
    buf[0] = MCP_IODIRA;
    buf[1] = 0x00;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed IODIRA write");
    }

    verify_register(MCP_IODIRA, "IODIRA");

    //
    // GPIOB inputs
    //
    buf[0] = MCP_IODIRB;
    buf[1] = 0xFF;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed IODIRB write");
    }

    verify_register(MCP_IODIRB, "IODIRB");

    //
    // Clear outputs
    //
    buf[0] = MCP_OLATA;
    buf[1] = 0x00;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed OLATA write");
    }

    verify_register(MCP_OLATA, "OLATA");

    //
    // Pullups enabled on GPIOB
    //
    buf[0] = MCP_GPPUB;
    buf[1] = 0xFF;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed GPPUB write");
    }

    verify_register(MCP_GPPUB, "GPPUB");

    //
    // Interrupt compare against DEFVAL
    //
    buf[0] = MCP_INTCONB;
    buf[1] = 0xFF;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed INTCONB write");
    }

    verify_register(MCP_INTCONB, "INTCONB");

    //
    // DEFVALB = all HIGH
    //
    buf[0] = MCP_DEFVALB;
    buf[1] = 0xFF;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed DEFVALB write");
    }

    verify_register(MCP_DEFVALB, "DEFVALB");

    //
    // Enable interrupts
    //
    buf[0] = MCP_GPINTENB;
    buf[1] = 0xFF;

    if(!i2c_mgr_write(MCP23017_ADDR, buf, 2)) {
        log_info("Failed GPINTENB enable");
    }

    verify_register(MCP_GPINTENB, "GPINTENB");

    //
    // Clear interrupt capture
    //
    uint8_t reg = MCP_INTCAPB;
    uint8_t dummy = 0x00;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &dummy, 1)) {
        log_info("Failed INTCAPB clear");
    } else {
        log_reg_value("INTCAPB", dummy);
    }

    //
    // Read GPIOB after clear
    //
    reg = MCP_GPIOB;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &dummy, 1)) {
        log_info("Failed GPIOB read");
    } else {
        log_reg_value("GPIOB", dummy);
    }

    //
    // Initialize groups
    //
    for(uint8_t i = 0; i < GROUP_COUNT; i++) {

        groups[i].index = i;
        groups[i].output_pin = i;
        groups[i].input_pin = 7 - i;

        groups[i].job_id[0] = '\0';
        groups[i].duration_minutes = 0;
        groups[i].elapsed_minutes = 0;
        groups[i].active = false;
        groups[i].pending_complete = false;
    }

    log_info("groups_init complete");
}

io_group_t* group_get(uint8_t index) {
    if(index >= GROUP_COUNT) {
        return NULL;
    }

    return &groups[index];
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

bool group_output_on(io_group_t *group) {

    if(group == NULL) {
        log_debug("Group is null");
        return false;
    }

    // Read current output latch state
    uint8_t reg = MCP_OLATA;
    uint8_t olata = 0x00;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &olata, 1)) {
        log_debug("Write read failed");
        return false;
    }

    // Set this group's output bit
    olata |= (1 << group->output_pin);

    char pin_str[4];
    uint16_to_str(pin_str, sizeof(pin_str), group->output_pin);

    char value_str[3];
    byte_to_hex2(olata, value_str);

    char msg[32];
    char *parts[] = {
        "Output ON pin=",
        pin_str,
        " value=0x",
        value_str
    };

    join_buffers(parts, 4, msg, sizeof(msg));
    log_debug(msg);

    uint8_t write_buf[2];
    write_buf[0] = MCP_OLATA;
    write_buf[1] = olata;

    return i2c_mgr_write(MCP23017_ADDR, write_buf, 2);
}

bool group_output_off(io_group_t *group) {

    if(group == NULL) {
        log_debug("Group is null");
        return false;
    }

    // Read current output latch state
    uint8_t reg = MCP_OLATA;
    uint8_t olata = 0x00;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &olata, 1)) {
        log_debug("Write read failed");
        return false;
    }

    // Clear this group's output bit
    olata &= ~(1 << group->output_pin);

    char pin_str[4];
    uint16_to_str(pin_str, sizeof(pin_str), group->output_pin);

    char value_str[3];
    byte_to_hex2(olata, value_str);

    char msg[32];
    char *parts[] = {
        "Output OFF pin=",
        pin_str,
        " value=0x",
        value_str
    };

    join_buffers(parts, 4, msg, sizeof(msg));
    log_debug(msg);

    uint8_t write_buf[2];
    write_buf[0] = MCP_OLATA;
    write_buf[1] = olata;

    return i2c_mgr_write(MCP23017_ADDR, write_buf, 2);
}

void group_log(io_group_t *group) {

    if(group == NULL) {
        log_info("Group is null");
        return;
    }

    char index_str[4];
    uint16_to_str(index_str, sizeof(index_str), group->index);

    char output_pin_str[4];
    uint16_to_str(output_pin_str, sizeof(output_pin_str), group->output_pin);

    char input_pin_str[4];
    uint16_to_str(input_pin_str, sizeof(input_pin_str), group->input_pin);
    
    char duration_str[8];
    uint16_to_str(duration_str, sizeof(duration_str), group->duration_minutes);

    char elapsed_str[8];
    uint16_to_str(elapsed_str, sizeof(elapsed_str), group->elapsed_minutes);

    // Section 1
    {
        char msg[48];

        char *parts[] = {
            "Group idx=",
            index_str,
            " out=",
            output_pin_str,
            " in=",
            input_pin_str
        };

        join_buffers(parts, 6, msg, sizeof(msg));
        log_info(msg);
    }

    // Section 2
    {
        char msg[64];

        char *parts[] = {
            "Job=",
            group->job_id,
            " duration=",
            duration_str,
            " elapsed=",
            elapsed_str
        };

        join_buffers(parts, 6, msg, sizeof(msg));
        log_info(msg);
    }

    // Section 3
    {
        char msg[32];

        char *parts[] = {
            "Active=",
            group->active ? "true" : "false"
        };

        join_buffers(parts, 2, msg, sizeof(msg));
        log_info(msg);
    }
}

static void complete_group_job(io_group_t *group) {
    log_info("Completing group");
        group_output_off(group);

//        group->job_id[0] = '\0'; // needed to send command complete
        group->duration_minutes = 0;
        group->elapsed_minutes = 0;
        group->active = false;
        group->pending_complete = true;
}

static inline void perform_action(LoraRcv message) {
    
    // Iterate through the payload to split on :: so we can separate the action
    // ID, zone, and the action parameter (e.g. duration)
    char *action_str = message.payload;
    char *zone_str = NULL;

    char *ptr = message.payload;

    // Find first ::
    while(*ptr) {

        if(ptr[0] == ':' && ptr[1] == ':') {
            *ptr = '\0';
            zone_str = ptr + 2;
            ptr += 2;
            break;
        }

        ptr++;
    }

    if(zone_str == NULL) {
        return;
    }
    
    io_group_t *group = group_get((uint8_t) parse_u16(zone_str));
    if(group == NULL) {
        return;
    }
    
    // Now check if we should turn this on or off
    if(strcmpr(action_str, "1007")) {
        

        // Find second ::
        
        char *duration_str = NULL;
        while(*ptr) {

            if(ptr[0] == ':' && ptr[1] == ':') {
                *ptr = '\0';
                duration_str = ptr + 2;
                break;
            }

            ptr++;
        }

        if(duration_str == NULL) {
            return;
        }
        
        memcpy(group->job_id, message.msg_id, LORA_MAX_MSGID + 1);
        group->duration_minutes = parse_u16(duration_str);
        group->elapsed_minutes = 0;
        group->active = true;
        group->pending_complete = false;

        group_output_on(group);
        

    } else if(strcmpr(action_str, "1008")) {
        complete_group_job(group);
        char buf[LORA_MAX_MSGID + 5]; 
        char *parts[] = { "CMDCMPLT::", message.msg_id };

        join_buffers(parts, 2, buf, LORA_MAX_MSGID + 5);
        lora_send_raw(32, buf);
//        send_command_complete(32, message.msg_id);
    }   
}

bool group_output_is_on(io_group_t *group) {

    if(group == NULL) {
        log_debug("Group is null");
        return false;
    }

    uint8_t reg = MCP_OLATA;
    uint8_t olata = 0x00;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &olata, 1)) {
        log_debug("Write read failed");
        return false;
    }

    bool is_on = (olata & (1 << group->output_pin)) != 0;

    char pin_str[4];
    uint16_to_str(pin_str, sizeof(pin_str), group->output_pin);

    char value_str[3];
    byte_to_hex2(olata, value_str);

    char msg[40];
    char *parts[] = {
        "Output state pin=",
        pin_str,
        " value=0x",
        value_str,
        is_on ? " ON" : " OFF"
    };

    join_buffers(parts, 5, msg, sizeof(msg));
    log_debug(msg);

    return is_on;
}

static inline int8_t send_lclreq(io_group_t *group) {

    if(group == NULL) {
        return -1;
    }

    char buff1[] = "LCLREQ::";
    const char *buff2;

    char index_buff[4]; // enough for 0-255 plus '\0'
    uint16_to_str(index_buff, sizeof(index_buff), group->index);

    if(group_output_is_on(group)) {
        buff2 = "OFF";
    } else {
        buff2 = "ON";
    }

    // Example: LCLREQ::7::ON
    char delimiter[] = "::";

    char out[18];
    char *parts[] = {
        buff1,
        index_buff,
        delimiter,
        (char*) buff2
    };

    join_buffers(parts, 4, out, sizeof(out));

    return lora_send_raw(32, out);
}

static inline int8_t send_lclovr(io_group_t *group) {

    if(group == NULL) {
        return -1;
    }

    char buff1[] = "LCLOVR::";
    const char *buff2;

    char index_buff[4]; // enough for 0-255 plus '\0'
    uint16_to_str(index_buff, sizeof(index_buff), group->index);

    if(group_output_is_on(group)) {
        buff2 = "OFF";
    } else {
        buff2 = "ON";
    }

    // Example: LCLOVR::7::ON
    char delimiter[] = "::";

    char out[18];
    char *parts[] = {
        buff1,
        index_buff,
        delimiter,
        (char*) buff2
    };

    join_buffers(parts, 4, out, sizeof(out));

    return lora_send_raw(32, out);
}



static void debug_gpiob(void) {
    uint8_t reg = MCP_GPIOB;
    uint8_t gpiob = 0x11;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &gpiob, 1)) {
        log_info("GPIOB read failed");;
        switch(I2C1_Host.ErrorGet()) {
             case 0:
                 log_info("I2C err none");
                 break;

             case 1:
                 log_info("I2C err 1");
                 break;

             case 2:
                 log_info("I2C err 2");
                 break;

             case 3:
                 log_info("I2C err 3");
                 break;

             case 4:
                 log_info("I2C err 4");
                 break;

             default:
                 log_info("I2C err unknown");
                 break;
         }
        return;
    }

    char hex[3];
    byte_to_hex2(gpiob, hex);

    char msg[16];
    char *parts[] = {
        "GPIOB=",
        hex
    };

    join_buffers(parts, 2, msg, sizeof(msg));
    log_info(msg);
}


/**
 * Returns the first group that triggered the GPIOB interrupt.
 *
 * Reading INTCAPB clears the interrupt flag and releases INTB.
 */
io_group_t* group_find_pressed(void) {

    uint8_t reg;
    uint8_t intfb = 0x00;
    uint8_t intcapb = 0xFF;
    uint8_t gpiob = 0xFF;

    //
    // Read INTFB
    //
    reg = MCP_INTFB;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &intfb, 1)) {
        log_info("Failed INTFB read");
        return NULL;
    }

    //
    // Read INTCAPB (clears interrupt)
    //
    reg = MCP_INTCAPB;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &intcapb, 1)) {
        log_info("Failed INTCAPB read");
        return NULL;
    }

    //
    // Read current GPIO state
    //
    reg = MCP_GPIOB;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &gpiob, 1)) {
        log_info("Failed GPIOB read");
        return NULL;
    }

    //
    // Log raw register values
    //
    {
        char intfb_hex[3];
        char intcap_hex[3];
        char gpiob_hex[3];

        byte_to_hex2(intfb, intfb_hex);
        byte_to_hex2(intcapb, intcap_hex);
        byte_to_hex2(gpiob, gpiob_hex);

        char msg[64];

        char *parts[] = {
            "INTFB=",
            intfb_hex,
            " INTCAPB=",
            intcap_hex,
            " GPIOB=",
            gpiob_hex
        };

        join_buffers(parts, 6, msg, sizeof(msg));
        log_info(msg);
    }

    //
    // Active LOW buttons
    //
    uint8_t pressed_now = (uint8_t)(~gpiob);
    uint8_t pressed_captured = (uint8_t)(~intcapb);

    //
    // Log interpreted states
    //
    {
        char now_hex[3];
        char captured_hex[3];

        byte_to_hex2(pressed_now, now_hex);
        byte_to_hex2(pressed_captured, captured_hex);

        char msg[64];

        char *parts[] = {
            "PRESSED_NOW=",
            now_hex,
            " PRESSED_CAPTURED=",
            captured_hex
        };

        join_buffers(parts, 4, msg, sizeof(msg));
        log_info(msg);
    }

    //
    // Prefer currently-held button
    //
    uint8_t active = pressed_now;

    //
    // Fall back to captured state
    //
    if(active == 0x00) {
        active = pressed_captured;
        log_info("Using captured state");
    }

    //
    // Nothing active
    //
    if(active == 0x00) {
        log_info("No active button");
        return NULL;
    }

    //
    // Find matching group
    //
    for(uint8_t i = 0; i < GROUP_COUNT; i++) {

        io_group_t *group = &groups[i];

        if(active & (1 << group->input_pin)) {

            char index_str[4];
            char pin_str[4];

            uint16_to_str(index_str, sizeof(index_str), group->index);
            uint16_to_str(pin_str, sizeof(pin_str), group->input_pin);

            char msg[48];

            char *parts[] = {
                "Matched group=",
                index_str,
                " pin=",
                pin_str
            };

            join_buffers(parts, 4, msg, sizeof(msg));
            log_info(msg);

            return group;
        }
    }

    log_info("No matching group");
    return NULL;
}

bool group_button_pressed(io_group_t *group) {

    if(group == NULL) {
        return false;
    }

    uint8_t reg = MCP_GPIOB;
    uint8_t gpiob = 0x00;

    if(!i2c_mgr_write_read(MCP23017_ADDR, &reg, 1, &gpiob, 1)) {
        return false;
    }

    // Active LOW because pull-ups are enabled
    return !(gpiob & (1 << group->input_pin));
}

static void log_reset_flags(void) {

    char msg[32];

    sprintf(msg, "PCON0=%02X", PCON0);
    log_info(msg);

    if(PCON0bits.nPOR == 0) {
        log_info("Power-on reset");
    }

    if(PCON0bits.nBOR == 0) {
        log_info("Brown-out reset");
    }

    if(PCON0bits.nRMCLR == 0) {
        log_info("MCLR reset");
    }

    if(PCON0bits.nRWDT == 0) {
        log_info("Watchdog reset");
    }

    if(PCON0bits.STKOVF == 1) {
        log_info("Stack overflow reset");
    }

    if(PCON0bits.STKUNF == 1) {
        log_info("Stack underflow reset");
    }

    // Clear flags for next reset
    PCON0 = 0xFF;
}

void static inline group_tick_minute(void) {

    for(uint8_t i = 0; i < GROUP_COUNT; i++) {

        io_group_t *group = group_get(i);

        if(group == NULL) {
            continue;
        }

        // No active job
        if(group->job_id[0] == '\0') {
            continue;
        }

        // No duration configured
        if(group->duration_minutes == 0) {
            continue;
        }

        group->elapsed_minutes++;

        // Job complete
        log_info("Checking for job...");
        if(group->elapsed_minutes >= group->duration_minutes) {
            complete_group_job(group);
        }
    }
}



void task(void) {
    
//    log_reset_flags();


    lora_enable();
    i2c_mgr_init();
    
    IO2_SetDigitalMode();
    IO2_SetDigitalInput();
    TMR2_Initialize();
    TMR2_OverflowCallbackRegister(timer_callback);
    TMR2_Start();
    
    __delay_ms(2000);
    groups_init();
    
    while(true) {
        
        /**
         * This is a testing block for the io extender
         */
//        debug_gpiob();
        

        /**
         * Find any button presses and track how long it's pressed. Then
         * send an override or request according to that duration.
         */
        bool intb_low = !IO2_GetValue();
        if(intb_low) {
//            log_info("pressed...");
            io_group_t *pressed = group_find_pressed();
            if(pressed != NULL) {
                
                    LED_EN_SetHigh();

                    char index_buff[4];
                    uint16_to_str(index_buff, sizeof(index_buff), pressed->index);

                    char out[24];
                    char *parts[] = {"Button pressed: ",index_buff};
                    join_buffers(parts, 2, out, sizeof(out));
                    log_info(out);

                    send_lclreq(pressed);

                    // spin until the button is released
                    while(group_button_pressed(pressed)) {
                        __delay_ms(25);
                    }
            
            } else {
                log_info("Pressed is null");
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
//            log_info("has data");
            if(lora_receive(line_buf, 64, &message) == DATA_RECEIVED) {
                log_info("received");
                
                send_ack(32, message.msg_id);
                log_info("ack sent");
                perform_action(message);
                log_info("action made");
            }
        }
        
        /**
         * If a minute has elapsed then to and increment each of the groups
         * that have an active job.
         */
        if(minute_elapsed) {
            log_info("Minute elapsed");
            minute_elapsed = false;
            group_tick_minute();
        }
        
        /**
         * Check each group for job completion
         */
        for(uint8_t i = 0; i < GROUP_COUNT; i++) {

            io_group_t *group = group_get(i);
            if(group == NULL) {
                continue;
            }

            if(group->pending_complete) {
                
                log_info("Sending command complete");
                
                char buf[LORA_MAX_MSGID + 5]; 
                char *parts[] = { "CMDCMPLT::", group->job_id };

                join_buffers(parts, 2, buf, LORA_MAX_MSGID + 5);
                lora_send_raw(32, buf);
                
//                send_command_complete(32, group->job_id);
                group->pending_complete = false;
                group->job_id[0] = '\0';
            }
        }
        
        // Allow things to settle
//        __delay_ms(25);
    }
}
 
int main(void) {
    return run(task);   
}