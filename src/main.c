/**
 * Copyright (c) 2012 - 2020, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include "config.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "app_error.h"

#include "app_timer.h"
#include "app_scheduler.h"
#include "nrf_pwr_mgmt.h"

#include "nrf_drv_power.h"
#include "nrf_drv_clock.h"

#include "keyboard.h"

#include "tmk_driver.h"
 
#include "kb_evt.h"
#include "kb_storage.h"

#include "ble_main.h"
#include "usbd_hid.h"

#define DEAD_BEEF 0xDEADBEEF

#define SCHED_MAX_EVENT_DATA_SIZE APP_TIMER_SCHED_EVENT_DATA_SIZE
#ifdef SVCALL_AS_NORMAL_FUNCTION
#define SCHED_QUEUE_SIZE 100
#else
#define SCHED_QUEUE_SIZE 75
#endif


// Setting GPIO Voltage to 3.3V
// Default 1.8V is too low for driving some of the electronic components
static void set_gpio_voltage_3V3(void)
{
    // Configure UICR_REGOUT0 register only if it is set to default value.
    if ((NRF_UICR->REGOUT0 & UICR_REGOUT0_VOUT_Msk) != (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos))
    {
        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}
        

        NRF_UICR->REGOUT0 = (NRF_UICR->REGOUT0 & ~((uint32_t)UICR_REGOUT0_VOUT_Msk)) | (UICR_REGOUT0_VOUT_3V3 << UICR_REGOUT0_VOUT_Pos);

        NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
        while (NRF_NVMC->READY == NVMC_READY_READY_Busy){}

        // System reset is needed to update UICR registers.
        NVIC_SystemReset();
    }
}


// Function of init log function
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);
    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

// Function of itit clock, related to USB Controler
static void clock_init(void)
{
    ret_code_t err_code;
    err_code = nrf_drv_clock_init();
    APP_ERROR_CHECK(err_code);
}

// Function of apptimer initation
static void timer_init(void)
{
// init app_timer
    ret_code_t err_code;
    err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

// Function of init power management library
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}

// Function of init scheduler, for apptimer task
static void scheduler_init(void)
{
    APP_SCHED_INIT(SCHED_MAX_EVENT_DATA_SIZE, SCHED_QUEUE_SIZE);
}

// Task of main loop
static void idle_state_handle(void)
{
    app_sched_execute(); // execute task in scheduler generated by app timer
    execute_kb_event();  // execute task of keyboard define event
    app_sched_execute(); // execute task in scheduler generated by app timer
    usbd_evt_process();  // execute usb device event
    app_sched_execute(); // execute task in scheduler generated by app timer
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run(); // start power management
    }
}


int main()
{
    // check and set GPIO Voltage to 3V3
    set_gpio_voltage_3V3();
    // init log function
    log_init();
    // init the clock
    clock_init();
    // init the scheduler
    scheduler_init();
    // init the app timer
    timer_init();
    // init keyboard event queue
    kb_event_queue_init();
    // init keyboard storage
    storage_init();

    // init usb device
    usbd_prepare(); 
    // init bluetooth stack, services, etc.
    ble_init();
    
    // trig init event, init keyboard peripherals of keyboards
    trig_kb_event(KB_EVT_INIT);
    // keyboard_task_timer and matrix init
    keyboard_init();

    // start keyboard functions
    trig_kb_event(KB_EVT_START);
    // start the tmk keyboard task timer, start tmk functions
    // scanning, keycode processing, sending
    keyboard_task_start();

    // enter keyboard main loop
    for(;;){
        // execute keybaord mission of every loop
        idle_state_handle();
    }
}