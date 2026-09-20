/*****************************************************************************
* File Name        : main.c
*
* Description      : Main CM33 secure core application for CC1 OOB demo.
*                    Initializes UART, blinks an LED using a TCPWM timer,
*                    and boots the PPCA secondary cores.
*
* Related Document : See README.md
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/


/******************************************************************************
 * Header Files
 *****************************************************************************/

#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"


/*******************************************************************************
* Macros
********************************************************************************/
/* Flash memory addresses where PPCA core firmware images are stored */
#define CORE0_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca0_nvm_C_S_START
#define CORE1_IMAGE_ADDRESS    CYMEM_CM33_0_S_m33s_ppca1_nvm_C_S_START
#define PPCA0_IMAGE_SIZE       CYMEM_CM33_0_S_ppca0_code_SIZE
#define PPCA1_IMAGE_SIZE       CYMEM_CM33_0_S_ppca1_code_SIZE

/* These are the addresses that the other application running on PPCA cores should be using to update. */
#define PPCA_M1_VAR_ADDRESS   0x53020400
#define PPCA_M3_VAR_ADDRESS   0x53040800

/*******************************************************************************
* Global Variables
********************************************************************************/

/* TCPWM timer interrupt configuration: source and priority */
const cy_stc_sysint_t intrCfg1 =
{
          .intrSrc = TCPWM_COUNTER_IRQ,
          .intrPriority = 7u
};

volatile bool timer_interrupt_flag = false; /* Flag set when TCPWM timer interrupt occurs */
bool led_blink_active_flag = true;        /* Flag to track LED blinking state (true = active) */

/* Variable for storing character read from terminal */
uint8_t uart_read_value;

/* Debug UART variables */
static cy_stc_scb_uart_context_t    DEBUG_UART_context; /* UART peripheral configuration context */
static mtb_hal_uart_t               DEBUG_UART_hal_obj; /* UART HAL abstraction object */

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void timer_init(void);
void isr_timer(void);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
* Main entry point for the CM33 secure core OOB demo. Initializes the board,
* configures the debug UART for console output, sets up a TCPWM timer to blink
* the user LED at 1-second intervals, initializes the PPCA block, and boots
* the two PPCA secondary cores. The main loop monitors UART input to
* pause/resume LED blinking and handles timer interrupt flags.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    
    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize and enable the SCB-based debug UART peripheral */
    Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);
    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Configure HAL layer for UART peripheral */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config,
                                &DEBUG_UART_context, NULL);

    /* HAL DEBUG_UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);

    /* retarget IO init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Clear terminal screen and move cursor to home position */
    printf("\x1b[2J\x1b[;H");

    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: OOB application\r\n");
    printf("************************************************************\r\n\n");

    printf("Hello World!!!\r\n\n");
    printf("For more projects, "
           "visit our code examples repositories:\r\n\n");

    printf("https://github.com/Infineon/"
           "Code-Examples-for-ModusToolbox-Software\r\n\n");

    /* Initialize timer to toggle the LED */
    timer_init();

    printf("Press 'Enter' key to pause or "
           "resume blinking the user LED \r\n\r\n");
    
    /* Initializing the PPCA Configuration. */      
    Cy_PPCA_CNFG_Init(ppca_0_ppca_cnfg_0_HW, &ppca_0_ppca_cnfg_0_config);
    
    /* Enabling the PPCA Configuration. */
    Cy_PPCA_Enable(ppca_0_ppca_cnfg_0_HW);

    /* enable interrupts */
    __enable_irq();

    /* Load and boot PPCA Core 0 (M1) with firmware from flash */
    Cy_System_Init_CPU0((void*)CORE0_IMAGE_ADDRESS, PPCA0_IMAGE_SIZE);
    /* Load and boot PPCA Core 1 (M3) with firmware from flash */
    Cy_System_Init_CPU1((void*)CORE1_IMAGE_ADDRESS, PPCA1_IMAGE_SIZE);

    for (;;)
    {
        /* Check if 'Enter' key was pressed */
         if (mtb_hal_uart_get(&DEBUG_UART_hal_obj, &uart_read_value, 1) == CY_RSLT_SUCCESS)
         {

               if (uart_read_value == '\r')
               {
                     /* Pause LED blinking by stopping the timer */
                    if (led_blink_active_flag)
                    {
                          Cy_TCPWM_TriggerStopOrKill_Single(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);

                         printf("LED blinking paused \r\n");
                    }
                    else /* Resume LED blinking by starting the timer */
                    {
                          Cy_TCPWM_TriggerStart_Single(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);

                          printf("LED blinking resumed\r\n");
                    }

                 /* Move cursor to previous line */
                 printf("\x1b[1F");

                 /* Toggle the blink active flag */
                 led_blink_active_flag ^= 1;
             }
         }

         /* Check if timer elapsed (interrupt fired) and toggle the LED */
         if (timer_interrupt_flag)
         {
             /* Clear the timer interrupt flag (already processed) */
             timer_interrupt_flag = false;

             /* Toggle USER LED output */
             Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN);
         }
    }
}

/*******************************************************************************
 * Function Name: timer_init
 ********************************************************************************
 * Summary:
 * Initializes and configures the TCPWM peripheral in counter mode to generate
 * periodic interrupts for LED blinking. The timer is configured to produce an
 * interrupt every 1 second (based on configuration). The interrupt handler
 * (isr_timer) sets a flag that is processed in the main loop.
 *
 * Parameters:
 *  none
 *
 *******************************************************************************/
void timer_init(void)
{
     /* Initialize TCPWM counter with pre-configured settings */
     if (CY_TCPWM_SUCCESS != Cy_TCPWM_Counter_Init(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM, &TCPWM_COUNTER_config))
     {
          CY_ASSERT(0);
     }

     /* Enable the initialized counter */
     Cy_TCPWM_Counter_Enable(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);

     /* Enable TCPWM terminal count interrupt */
     Cy_TCPWM_SetInterruptMask(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM, CY_GPIO_INTR_EN_MASK);

     /* Configure TCPWM interrupt handler and priority */
     Cy_SysInt_Init(&intrCfg1, isr_timer);
     NVIC_EnableIRQ(TCPWM_COUNTER_IRQ);

     /* Start the counter */
     Cy_TCPWM_TriggerStart_Single(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);
}

/*******************************************************************************
 * Function Name: isr_timer
 ********************************************************************************
 * Summary:
 * Interrupt service routine for TCPWM timer. Clears the interrupt status and
 * sets a flag to signal that the timer has elapsed. The LED toggle is performed
 * in the main loop to maintain a responsive interrupt handler.
 *
 * Parameters:
 *  None
 *
 *******************************************************************************/
void isr_timer(void)
{

     uint32_t interrupts = Cy_TCPWM_GetInterruptStatusMasked(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM);

     /* Clear the interrupt */
     Cy_TCPWM_ClearInterrupt(TCPWM_COUNTER_HW, TCPWM_COUNTER_NUM, interrupts);

     if (0UL != (CY_TCPWM_INT_ON_TC & interrupts))
     {
          /* Set flag for terminal count event (LED toggle will be handled in main loop) */
          timer_interrupt_flag = true;
     }
}
