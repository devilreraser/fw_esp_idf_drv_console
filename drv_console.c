/* *****************************************************************************
 * File:   drv_console.c
 * Author: Dimitar Lilov
 *
 * Created on 2022 06 18
 * 
 * Description: ...
 * 
 **************************************************************************** */

/* *****************************************************************************
 * Header Includes
 **************************************************************************** */
#include "drv_console.h"

#include <string.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio_ext.h>
#include <errno.h>

#include "esp_log.h"
#include "esp_spiffs.h"

#include "driver/uart.h"
#include "esp_vfs_dev.h"

#include "esp_console.h"
#include "linenoise/linenoise.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// #include "drv_system_if.h"
// #include "drv_ota_if.h"
// #include "drv_eth_if.h"
// #include "drv_wifi_if.h"
// #include "drv_socket_if.h"
// #include "drv_stream_if.h"
// #include "drv_ping_if.h"
// #include "drv_w25n01gv_if.h"
// #include "drv_ext_spi_if.h"
// #include "drv_ht1302_if.h"
// #include "drv_nvs_if.h"
// #include "drv_log_if.h"
// #include "drv_fld12_spi_if.h"

/* *****************************************************************************
 * Configuration Definitions
 **************************************************************************** */
#define TAG "drv_console"

/* *****************************************************************************
 * Constants and Macros Definitions
 **************************************************************************** */
#define MOUNT_PATH      "/data"
#define HISTORY_PATH    "/history.txt"

#define HISTORY_SIZE    100

#define PROMPT_STR      "esp32"

/* *****************************************************************************
 * Enumeration Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Type Definitions
 **************************************************************************** */

/* *****************************************************************************
 * Function-Like Macros
 **************************************************************************** */

/* *****************************************************************************
 * Variables Definitions
 **************************************************************************** */

#if CONFIG_DRV_CONSOLE_CUSTOM

bool bUseHistoryStore = false;

const char* prompt = LOG_COLOR_I PROMPT_STR "> " LOG_RESET_COLOR;
bool bPromptPlaced = false;

TaskHandle_t console_task_handle = NULL;

bool bDrvConsoleRunExec = false;

bool bConsoleProcessing = false;

bool bStopLogRequest = false;
bool bLogDisabled = false;
bool bOtherLogDisabled = false;

bool bJustStopLog = false;
bool bJustStartLog = false;

#if CONFIG_DRV_CONSOLE_CUSTOM_LOG_DISABLE_FIX
bool bLogWasLstPrinted = false;
uint32_t* last_caller_id = NULL;
#endif

#endif



/* *****************************************************************************
 * Prototype of functions definitions
 **************************************************************************** */

/* *****************************************************************************
 * Functions
 **************************************************************************** */

// void drv_console_register_commands(void)
// {
//     esp_console_register_help_command();
//     cmd_system_register();
//     cmd_ota_register();
//     #if CONFIG_USE_ETHERNET
//     cmd_eth_register();
//     cmd_ethernet_iperf_register();
//     #endif
//     #if CONFIG_USE_WIFI
//     cmd_wifi_register();
//     #endif
//     cmd_socket_register();
//     cmd_stream_register();
//     cmd_ping_register();
//     #if CONFIG_USE_DRV_W25N01GV
//     cmd_w25n01gv_register();
//     #endif
//     #if CONFIG_DRV_EXT_SPI_USE
//     cmd_ext_spi_register();
//     #endif
//     cmd_ht1302_register();
//     cmd_nvs_register();
//     cmd_log_register();
//     #if CONFIG_USE_FLD12_SPI
//     cmd_fld12_spi_register();
//     #endif
// }

#if CONFIG_DRV_CONSOLE_CUSTOM
void drv_console_config_prompt(void)
{
    printf( "\n"
            "Type 'help' to get the list of commands.\n"
            "Use UP/DOWN arrows to navigate through command history.\n"
            "Press TAB when typing command name to auto-complete.\n\r");

    /* Figure out if the terminal supports escape sequences */

    /* Delay of 500 ms is needed for linenoiseProbe() */
    vTaskDelay(pdMS_TO_TICKS(500));

    int probe_status = linenoiseProbe();

    //probe_status = 0;   /* always use escape sequences */
    //probe_status = 1;   /* always skip escape sequences */

    if (probe_status)
    {
        printf( "\n"
            "Your terminal application does not support escape sequences.\n"
            "Line editing and history features are disabled.\n"
            "On Windows, try using Putty instead.\n\r");
        linenoiseSetDumbMode(1);
        /* Since the terminal does not support escape sequences, don't use color codes in the prompt */
        prompt = PROMPT_STR "> ";
    }
}


esp_err_t spiffs_init(void)
{
    ESP_LOGI(TAG, "Initializing SPIFFS");

    esp_vfs_spiffs_conf_t conf = 
    {
        .base_path = MOUNT_PATH,
        .partition_label = NULL,
        .max_files = 4,
        .format_if_mount_failed = true,
    };

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format spiffs");
        }
        else
        if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find spiffs partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize spiffs (%s)", esp_err_to_name(ret));
        }
        return ret;
    }

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if(ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get spiffs partition information (%s)", esp_err_to_name(ret));
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return ret;
}

void set_console_uart_use(void)
{
    /* Drain stdout before reconfiguring it */
    fflush(stdout);
    fsync(fileno(stdout));


    /* Disable buffering on stdin */
    setvbuf(stdin, NULL, _IONBF, 0);

    /* Minicom, screen, idf_monitor send CR when ENTER key is pressed */
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CR);
    /* Move the caret to the beginning of the next line on '\n' */
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM, ESP_LINE_ENDINGS_CRLF);

    if (uart_is_driver_installed(CONFIG_ESP_CONSOLE_UART_NUM) == false)
    {
        /* Configure UART. Note that REF_TICK is used so that the baud rate remains
        * correct while APB frequency is changing in light sleep mode.
        */
        const uart_config_t uart_config = {
                .baud_rate = CONFIG_ESP_CONSOLE_UART_BAUDRATE,
                .data_bits = UART_DATA_8_BITS,
                .parity = UART_PARITY_DISABLE,
                .stop_bits = UART_STOP_BITS_1,
    #if SOC_UART_SUPPORT_REF_TICK
            .source_clk = UART_SCLK_REF_TICK,
    #elif SOC_UART_SUPPORT_XTAL_CLK
            .source_clk = UART_SCLK_XTAL,
    #endif
        };
        /* Install UART driver for interrupt-driven reads and writes */
        ESP_ERROR_CHECK( uart_driver_install(CONFIG_ESP_CONSOLE_UART_NUM,
                256, 0, 0, NULL, 0) );
        ESP_ERROR_CHECK( uart_param_config(CONFIG_ESP_CONSOLE_UART_NUM, &uart_config) );
    }

    /* Tell VFS to use UART driver */
    esp_vfs_dev_uart_use_driver(CONFIG_ESP_CONSOLE_UART_NUM);

}

#if CONFIG_DRV_CONSOLE_CUSTOM_LOG_DISABLE_FIX
bool drv_console_get_log_disabled(void)
{
    if (bLogDisabled)
    {
    }
    else
    {
        bLogWasLstPrinted = true;
    }
    return bLogDisabled;
}

bool drv_console_get_other_log_disabled(void)
{
    return bOtherLogDisabled;
}

bool drv_console_is_needed_finish_line(void)
{
    bool bReturn = bJustStartLog | bPromptPlaced;

    last_caller_id = NULL;

    bJustStartLog = false;

    bPromptPlaced = false;

    return bReturn;
}

bool drv_console_is_needed_finish_line_caller_check(uint32_t* caller_id)
{
    if (last_caller_id == caller_id)
    {
        return false;
    }
    if (drv_console_is_needed_finish_line())
    {
        last_caller_id = caller_id;
        return true;
    }
    if (last_caller_id != caller_id)
    {
        last_caller_id = caller_id;
        return true;
    }
    return false;
}

void drv_console_set_needed_finish_line_caller(uint32_t* caller_id)
{
    bPromptPlaced = true;
}

void drv_console_set_log_disabled(void) /* used by cmd registered functions */
{
    #if CONFIG_DRV_CONSOLE_CUSTOM
    bLogDisabled = true;
    #endif
}

void drv_console_set_log_enabled(void) /* used by cmd registered functions */
{
    bLogDisabled = false;
}

void drv_console_set_other_log_disabled(void) /* used by cmd registered functions thah use ESP_LOG */
{
    #if CONFIG_DRV_CONSOLE_CUSTOM
    bOtherLogDisabled = true;
    #endif
}

void drv_console_set_other_log_enabled(void) /* used by cmd registered functions thah use ESP_LOG */
{
    bOtherLogDisabled = false;
}

bool drv_console_set_log_disabled_check_skipped(char* data, int size)   /* need printf("%s", prompt); if log was last printed */
{
    bool bReturn = bLogWasLstPrinted;

    if (bReturn)
    {
        printf("\r");
        printf("%s", prompt);
        #if 0
        char * pString = malloc(size+1);
        if (pString != NULL)
        {
            memcpy(pString, data, size);
            pString[size] = 0;
            printf("%s", pString);
            free(pString);
        }
        #endif
    }

    bLogWasLstPrinted = false;
        
    bLogDisabled = true;
    return bReturn;
}
#endif  /* #if CONFIG_DRV_CONSOLE_CUSTOM_LOG_DISABLE_FIX */

#endif  /* #if CONFIG_DRV_CONSOLE_CUSTOM */



void drv_console_init(void)
{
#if CONFIG_DRV_CONSOLE_CUSTOM

    set_console_uart_use();
    

    /* Initialize filesystem */
    if (spiffs_init() == ESP_OK)
    {
        bUseHistoryStore = true;
    }
    else
    {
        bUseHistoryStore = false;
    }
    ESP_LOGI(TAG, "Console history store %s", (bUseHistoryStore)?"enabled":"disabled");

    /* Initialize Console */
    esp_console_config_t console_config = 
    {
        .max_cmdline_args = 256,
        .max_cmdline_length = 1024,
        .hint_color = atoi(LOG_COLOR_CYAN),
    };

    ESP_ERROR_CHECK( esp_console_init(&console_config) );



    /* Configure linenoise line completion library */
    /* Enable multiline editing. If not set, long commands will scroll within single line */
    linenoiseSetMultiLine(1);

    /* Tell linenoise where to get command completions and hints */
    linenoiseSetCompletionCallback(&esp_console_get_completion);
    linenoiseSetHintsCallback((linenoiseHintsCallback*)&esp_console_get_hint);

    /* Set command history size */
    linenoiseHistorySetMaxLen(HISTORY_SIZE);

    /* Set command maximum length */
    linenoiseSetMaxLineLen(console_config.max_cmdline_length);

    /* Don't return emplty lines */
    linenoiseAllowEmpty(false);

    if (bUseHistoryStore)
    {
        linenoiseHistoryLoad(HISTORY_PATH);
    }

    drv_console_config_prompt();

#else

    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    //repl_config.prompt = "esp32>";


    // init console REPL environment
    #if CONFIG_ESP_CONSOLE_UART
    if (uart_is_driver_installed(CONFIG_ESP_CONSOLE_UART_NUM))
    {
        uart_driver_delete(CONFIG_ESP_CONSOLE_UART_NUM);
    }
    esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));
    #elif CONFIG_ESP_CONSOLE_USB_CDC
    esp_console_dev_usb_cdc_config_t cdc_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&cdc_config, &repl_config, &repl));
    #elif CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG
    esp_console_dev_usb_serial_jtag_config_t usbjtag_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&usbjtag_config, &repl_config, &repl));
    #endif

    // start console REPL
    ESP_ERROR_CHECK(esp_console_start_repl(repl));

#endif

    esp_console_register_help_command();
    //drv_console_register_commands();

    ESP_LOGI(TAG, "Console Initialization complete!");
}

#if CONFIG_DRV_CONSOLE_CUSTOM

esp_err_t drv_console_run(const char *cmd_line, int *cmd_ret)
{
    esp_err_t error;
    bDrvConsoleRunExec = true;
    //add here filter by MAC if needed
    error = esp_console_run(cmd_line, cmd_ret);
    bDrvConsoleRunExec = false;
    return error;
}

static void drv_console_execute(void* parameters)
{
    bConsoleProcessing = true;

    fflush(stdin);

    

    while(bConsoleProcessing)
    {
        bLogDisabled = bStopLogRequest;
        bPromptPlaced = true;
        char* line = linenoise(prompt);
        bLogDisabled = false;
        vTaskDelay(pdMS_TO_TICKS(10));

        if (line == NULL)
        {
            //printf("Command line is NULL\r\n");

            bStopLogRequest = !bStopLogRequest;
            if (bStopLogRequest)
            {
                bJustStopLog = true;
                ESP_LOGI(TAG, "Stopped Log (on NULL line)");
            }
            else
            {
                bJustStartLog = true;
                ESP_LOGI(TAG, "Started Log (on NULL line)");
            }
            continue;
        }

        if (strlen(line) > 0)
        {
            printf("\r\n");
            linenoiseHistoryAdd(line);
            if (bUseHistoryStore)
            {
                //spi_shared_take
                linenoiseHistorySave(HISTORY_PATH);
                //spi_shared_give
            }
        }
        else
        {
            printf("\r\n");
            bStopLogRequest = !bStopLogRequest;
            if (bStopLogRequest)
            {
                bJustStopLog = true;
                ESP_LOGI(TAG, "Stopped Log (on zero line length)");
            }
            else
            {
                bJustStartLog = true;
                ESP_LOGI(TAG, "Started Log (on zero line length)");
            }
            continue;
        }

        ESP_LOGI(TAG, "Exec Command '%s'", line);

        //to do note: during the drv_console_run() log prints from other tasks must not interfere with the prints of drv_console_run()
        // will be disabled in cmd_registered functions that use printf and will be re-enabled if needed after drv_console_run() 
        
        //deny log print for help command
        if(memcmp(line, "help", 4) == 0)
        {
            bLogDisabled = true;
        }

        int ret;
        esp_err_t err = drv_console_run(line, &ret);

        if (err == ESP_ERR_NOT_FOUND)
        {
            printf("Unrecognized command\r\n");
        }
        else
        if (err == ESP_ERR_INVALID_ARG)
        {
            printf("Command with invalid arguments\r\n");
        }
        else
        if ((err == ESP_OK) && (ret != ESP_OK))
        {
            printf("Command returned non-zero error code: 0x%x (%s)\r\n", ret, esp_err_to_name(ret));
        }
        else
        if (err != ESP_OK) 
        {
            printf("Internal error ccode: 0x%x (%s)\r\n", ret, esp_err_to_name(ret));
        }

        ESP_LOGI(TAG, "Done Command '%s'", line);
        linenoiseFree(line);

        bLogDisabled = false;
        bOtherLogDisabled = false;

        vTaskDelay(pdMS_TO_TICKS(10));
    }
    console_task_handle = NULL;
    vTaskDelete(NULL);
}

#endif  /* #if CONFIG_DRV_CONSOLE_CUSTOM */

void drv_console_task(void)
{

#if CONFIG_DRV_CONSOLE_CUSTOM

    if (console_task_handle == NULL)
    {
        xTaskCreate(&drv_console_execute, "console", 2048 + 1024, NULL, 2, &console_task_handle);
        configASSERT(console_task_handle);
    }

#endif

}