/************************************************************************************
 * arch/arm/src/stm32L4/stm32l4_dfsdm.h
 *
 *   Copyright (C) 2017 Haltian Ltd. All rights reserved.
 *   Authors: Juha Niskanen <juha.niskanen@haltian.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name NuttX nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ************************************************************************************/

#ifndef __ARCH_ARM_SRC_STM32L4_STM32L4_DFSDM_H
#define __ARCH_ARM_SRC_STM32L4_STM32L4_DFSDM_H

/************************************************************************************
 * Included Files
 ************************************************************************************/

#include <tinyara/config.h>
#include <tinyara/analog/adc.h>
#include "chip.h"
#include "chip/stm32l4_dfsdm.h"

/************************************************************************************
 * Pre-processor Definitions
 ************************************************************************************/

/* Configuration ********************************************************************/

/* Timer devices may be used for different purposes.  One special purpose is to
 * control periodic ADC sampling.  If CONFIG_STM32L4_TIMn is defined then
 * CONFIG_STM32L4_TIMn_DFSDM must also be defined to indicate that timer "n" is
 * intended to be used for that purpose. Timers 1,3,6 and 16 may be used on
 * STM32L4X3, while STM32L4X6 adds support for timers 4,7 and 8 as well.
 */

#ifndef CONFIG_STM32L4_TIM1
#  undef CONFIG_STM32L4_TIM1_DFSDM
#endif
#ifndef CONFIG_STM32L4_TIM3
#  undef CONFIG_STM32L4_TIM3_DFSDM
#endif
#ifndef CONFIG_STM32L4_TIM4
#  undef CONFIG_STM32L4_TIM4_DFSDM
#endif
#ifndef CONFIG_STM32L4_TIM6
#  undef CONFIG_STM32L4_TIM6_DFSDM
#endif
#ifndef CONFIG_STM32L4_TIM7
#  undef CONFIG_STM32L4_TIM7_DFSDM
#endif
#ifndef CONFIG_STM32L4_TIM8
#  undef CONFIG_STM32L4_TIM8_DFSDM
#endif
#ifndef CONFIG_STM32L4_TIM16
#  undef CONFIG_STM32L4_TIM16_DFSDM
#endif

#if defined(CONFIG_STM32L4_DFSDM)

/* DMA support */

#undef DFSDM_HAVE_DMA
#if defined(CONFIG_STM32L4_DFSDM1_DMA)
#  define DFSDM_HAVE_DMA  1
#endif

/* ADC output to DFSDM support */

#undef ADC_HAVE_DFSDM
#if defined(CONFIG_STM32L4_ADC1_OUTPUT_DFSDM) || \
    defined(CONFIG_STM32L4_ADC2_OUTPUT_DFSDM) || \
    defined(CONFIG_STM32L4_ADC3_OUTPUT_DFSDM)
#  define ADC_HAVE_DFSDM
#endif

/* Timer configuration:  If a timer trigger is specified, then get
 * information about the timer.
 */

#if defined(CONFIG_STM32L4_TIM1_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM1_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB2_TIM1_CLKIN
#elif defined(CONFIG_STM32L4_TIM3_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM3_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB1_TIM3_CLKIN
#elif defined(CONFIG_STM32L4_TIM4_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM4_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB1_TIM4_CLKIN
#elif defined(CONFIG_STM32L4_TIM6_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM6_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB1_TIM6_CLKIN
#elif defined(CONFIG_STM32L4_TIM7_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM7_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB1_TIM7_CLKIN
#elif defined(CONFIG_STM32L4_TIM8_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM8_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB2_TIM8_CLKIN
#elif defined(CONFIG_STM32L4_TIM16_DFSDM)
#    define DFSDM_HAVE_TIMER           1
#    define DFSDM_TIMER_BASE           STM32L4_TIM16_BASE
#    define DFSDM_TIMER_PCLK_FREQUENCY STM32L4_APB2_TIM16_CLKIN
#else
#    undef  DFSDM_HAVE_TIMER
#endif

#ifdef DFSDM_HAVE_TIMER
#  ifndef CONFIG_STM32L4_DFSDM_SAMPLE_FREQUENCY
#    error "CONFIG_STM32L4_DFSDM_SAMPLE_FREQUENCY not defined"
#  endif
#  ifndef CONFIG_STM32L4_DFSDM_TIMTRIG
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG not defined"
#    warning "Values 0:CC1 1:CC2 2:CC3 3:CC4 4:TRGO 5:TRGO2"
#  endif
#endif

/* NOTE:  The following assumes that all possible combinations of timers and
 * values are support JEXTSEL.  That is not so and it varies from one STM32
 * to another.  But this (wrong) assumptions keeps the logic as simple as
 * possible.  If unsupported combination is used, an error will show up
 * later during compilation although it may be difficult to track it back
 * to this simplification.
 */

#define DFSDM_JEXTSEL_T1CC1      DFSDM_FLTCR1_JEXTSEL_T1CC1
#define DFSDM_JEXTSEL_T1CC2      DFSDM_FLTCR1_JEXTSEL_T1CC2
#define DFSDM_JEXTSEL_T1CC3      DFSDM_FLTCR1_JEXTSEL_T1CC3
#define DFSDM_JEXTSEL_T1CC4      DFSDM_FLTCR1_JEXTSEL_T1CC4
#define DFSDM_JEXTSEL_T1TRGO     DFSDM_FLTCR1_JEXTSEL_T1TRGO
#define DFSDM_JEXTSEL_T1TRGO2    DFSDM_FLTCR1_JEXTSEL_T1TRGO2

#define DFSDM_JEXTSEL_T3CC1      DFSDM_FLTCR1_JEXTSEL_T3CC1
#define DFSDM_JEXTSEL_T3CC2      DFSDM_FLTCR1_JEXTSEL_T3CC2
#define DFSDM_JEXTSEL_T3CC3      DFSDM_FLTCR1_JEXTSEL_T3CC3
#define DFSDM_JEXTSEL_T3CC4      DFSDM_FLTCR1_JEXTSEL_T3CC4
#define DFSDM_JEXTSEL_T3TRGO     DFSDM_FLTCR1_JEXTSEL_T3TRGO

#define DFSDM_JEXTSEL_T4CC1      DFSDM_FLTCR1_JEXTSEL_T4CC1
#define DFSDM_JEXTSEL_T4CC2      DFSDM_FLTCR1_JEXTSEL_T4CC2
#define DFSDM_JEXTSEL_T4CC3      DFSDM_FLTCR1_JEXTSEL_T4CC3
#define DFSDM_JEXTSEL_T4CC4      DFSDM_FLTCR1_JEXTSEL_T4CC4
#define DFSDM_JEXTSEL_T4TRGO     DFSDM_FLTCR1_JEXTSEL_T4TRGO

#define DFSDM_JEXTSEL_T6CC1      DFSDM_FLTCR1_JEXTSEL_T6CC1
#define DFSDM_JEXTSEL_T6CC2      DFSDM_FLTCR1_JEXTSEL_T6CC2
#define DFSDM_JEXTSEL_T6CC3      DFSDM_FLTCR1_JEXTSEL_T6CC3
#define DFSDM_JEXTSEL_T6CC4      DFSDM_FLTCR1_JEXTSEL_T6CC4
#define DFSDM_JEXTSEL_T6TRGO     DFSDM_FLTCR1_JEXTSEL_T6TRGO

#define DFSDM_JEXTSEL_T7CC1      DFSDM_FLTCR1_JEXTSEL_T7CC1
#define DFSDM_JEXTSEL_T7CC2      DFSDM_FLTCR1_JEXTSEL_T7CC2
#define DFSDM_JEXTSEL_T7CC3      DFSDM_FLTCR1_JEXTSEL_T7CC3
#define DFSDM_JEXTSEL_T7CC4      DFSDM_FLTCR1_JEXTSEL_T7CC4
#define DFSDM_JEXTSEL_T7TRGO     DFSDM_FLTCR1_JEXTSEL_T7TRGO

#define DFSDM_JEXTSEL_T8CC1      DFSDM_FLTCR1_JEXTSEL_T8CC1
#define DFSDM_JEXTSEL_T8CC2      DFSDM_FLTCR1_JEXTSEL_T8CC2
#define DFSDM_JEXTSEL_T8CC3      DFSDM_FLTCR1_JEXTSEL_T8CC3
#define DFSDM_JEXTSEL_T8CC4      DFSDM_FLTCR1_JEXTSEL_T8CC4
#define DFSDM_JEXTSEL_T8TRGO     DFSDM_FLTCR1_JEXTSEL_T8TRGO
#define DFSDM_JEXTSEL_T8TRGO2    DFSDM_FLTCR1_JEXTSEL_T8TRGO2

#define DFSDM_JEXTSEL_T16CC1     DFSDM_FLTCR1_JEXTSEL_T16CC1
#define DFSDM_JEXTSEL_T16CC2     DFSDM_FLTCR1_JEXTSEL_T16CC2
#define DFSDM_JEXTSEL_T16CC3     DFSDM_FLTCR1_JEXTSEL_T16CC3
#define DFSDM_JEXTSEL_T16CC4     DFSDM_FLTCR1_JEXTSEL_T16CC4
#define DFSDM_JEXTSEL_T16TRGO    DFSDM_FLTCR1_JEXTSEL_T16TRGO

#if defined(CONFIG_STM32L4_TIM1_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T1CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T1CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T1CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T1CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T1TRGO
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 5
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T1TRGO2
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#elif defined(CONFIG_STM32L4_TIM3_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T3CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T3CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T3CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T3CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T3TRGO
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#elif defined(CONFIG_STM32L4_TIM4_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T4CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T4CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T4CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T4CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T4TRGO
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#elif defined(CONFIG_STM32L4_TIM6_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T6CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T6CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T6CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T6CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T6TRGO
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#elif defined(CONFIG_STM32L4_TIM7_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T7CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T7CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T7CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T7CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T7TRGO
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#elif defined(CONFIG_STM32L4_TIM8_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T8CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T8CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T8CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T8CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T8TRGO
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 5
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T8TRGO2
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#elif defined(CONFIG_STM32L4_TIM16_DFSDM)
#  if CONFIG_STM32L4_DFSDM_TIMTRIG == 0
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T16CC1
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 1
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T16CC2
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 2
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T16CC3
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 3
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T16CC4
#  elif CONFIG_STM32L4_DFSDM_TIMTRIG == 4
#    define DFSDM_JEXTSEL_VALUE DFSDM_JEXTSEL_T16TRGO
#  else
#    error "CONFIG_STM32L4_DFSDM_TIMTRIG is out of range"
#  endif
#endif

/************************************************************************************
 * Public Types
 ************************************************************************************/

/************************************************************************************
 * Public Function Prototypes
 ************************************************************************************/

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#define EXTERN extern "C"
extern "C"
{
#else
#define EXTERN extern
#endif

/****************************************************************************
 * Name: stm32l4_dfsdm_initialize
 *
 * Description:
 *   Initialize the DFSDM.
 *
 * Input Parameters:
 *   intf      - Could be {0,1,2,3} for DFSDM FLT0, FLT1, FLT2, FLT3
 *   chanlist  - The list of channels
 *   nchannels - Number of channels
 *
 * Returned Value:
 *   Valid ADC device structure reference on success; a NULL on failure
 *
 ****************************************************************************/

struct adc_dev_s;
struct adc_dev_s *stm32l4_dfsdm_initialize(int intf,
                                           FAR const uint8_t *chanlist,
                                           int nchannels);
#undef EXTERN
#ifdef __cplusplus
}
#endif
#endif /* __ASSEMBLY__ */

#endif /* CONFIG_STM32L4_DFSDM */
#endif /* __ARCH_ARM_SRC_STM32L4_STM32L4_DFSDM_H */
