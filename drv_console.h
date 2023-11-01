/* *****************************************************************************
 * File:   drv_console.h
 * Author: Dimitar Lilov
 *
 * Created on 2022 06 18
 * 
 * Description: ...
 * 
 **************************************************************************** */
#pragma once

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */


/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include <stdbool.h>   
#include <stdint.h> 
/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */
typedef enum
{
    DRV_CONSOLE_TYPE_TCP,
    DRV_CONSOLE_TYPE_UART,
    
}drv_console_type_t;

/* *****************************************************************************
 * Function-Like Macro
 **************************************************************************** */

/* *****************************************************************************
 * Variables External Usage
 **************************************************************************** */ 

/* *****************************************************************************
 * Function Prototypes
 **************************************************************************** */
// bool drv_console_get_log_disabled(void);
// bool drv_console_get_other_log_disabled(void);
// bool drv_console_is_needed_finish_line(void);
// bool drv_console_is_needed_finish_line_caller_check(uint32_t* caller_id);
// void drv_console_set_needed_finish_line_caller(uint32_t* caller_id);
// void drv_console_set_log_disabled(void);
// void drv_console_set_log_enabled(void);
// void drv_console_set_other_log_disabled(void);
// void drv_console_set_other_log_enabled(void);
// bool drv_console_set_log_disabled_check_skipped(char* data, int size);
void drv_console_init(void);
void drv_console_task(void);


#ifdef __cplusplus
}
#endif /* __cplusplus */


