

/*
 * Copyright (c) 2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License")
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
//*****************************************************************************************
// connections_utils.h
//
// Define macros and typedefs to use within Connections. May be tool or
// library dependent.
//
//*****************************************************************************************

#ifndef __CONNECTIONS__CONNECTIONS_UTILS_H_
#define __CONNECTIONS__CONNECTIONS_UTILS_H_

#include <systemc.h>
#include <ac_assert.h>


/**
 * \def CONNECTIONS_ASSERT_MSG(x, msg)
 * \ingroup Assertions
 * Synthesizable assertion to check \a x and print \a msg if assertion fails. It will be synthesized by Catapult HLS tool to either psl or ovl assertions in RTL depending on HLS tool settings
 * \par A Simple Example
 * \code
 *      #include <connections_utils.h>
 *
 *      ...
 *      while(1) {
 *        if (in.PopNB(in_var)) {
 *          CONNECTIONS_ASSERT_MSG(in_var!=0, "Input is Zero"); // Assert that input is never 0
 *        ...
 *        }
 *      }
 * \endcode
 * \par
 */
#define CONNECTIONS_ASSERT_MSG(X,MSG)           \
  if (!(X)) {                                                 \
    CONNECTIONS_COUT("Assertion Failed. " << MSG << endl);    \
  }                                                           \
  assert(X);

/**
 * \def CONNECTIONS_SIM_ONLY_ASSERT_MSG(x, msg)
 * \ingroup Assertions
 * Non-synthesizable assertion to check \a x and print \a msg if assertion fails.
 * \par A Simple Example
 * \code
 *      #include <connections_utils.h>
 *
 *      ...
 *      while(1) {
 *        if (in.PopNB(in_var)) {
 *          CONNECTIONS_SIM_ONLY_ASSERT_MSG(in_var!=0, "Input is Zero"); // Assert that input is never 0
 *        ...
 *        }
 *      }
 * \endcode
 * \par
 */
#ifndef __SYNTHESIS__
#define CONNECTIONS_SIM_ONLY_ASSERT_MSG(X,MSG) \
  if (!(X)) { \
    CONNECTIONS_COUT("Assertion Failed. " << MSG << endl); \
  } \
  sc_assert(X);
#else
#define CONNECTIONS_SIM_ONLY_ASSERT_MSG(X,MSG) ((void)0);
#endif // ifdef __SYNTHESIS__

// Define preferred debug mechanism.
// FIXME: Change this to not support << but be a string instead, so can be pushed over to
// sc_report as well.
#if __SYNTHESIS__
#define CONNECTIONS_COUT(x) ((void)0);
#else
#define CONNECTIONS_COUT(x) std::cout << x
#endif

/**
 * \brief CONNECTIONS_SYNC_RESET define: Select synchronous or asynchronous reset.
 * \ingroup connections_module
 *
 * Connections uses asynchronous, active-low reset by default. Defining CONNECTIONS_SYNC_RESET
 * will use synchronous, active-low reset instead.
 * The macros CONNECTIONS_NEG_RESET_SIGNAL_IS() and CONNECTIONS_POS_RESET_SIGNAL_IS() can be used
 * in place of SystemC's reset_signal_is() and async_reset_signal_is() so that
 * CONNECTIONS_SYNC_RESET can select type.
 *
 * Active-low and -high are separate macros, in case module code assumes a priority.
 *
 */
//Backwards compatible with ENABLE_SYNC_RESET
#if defined(ENABLE_SYNC_RESET) && !defined(CONNECTIONS_SYNC_RESET)
#define CONNECTIONS_SYNC_RESET 
#endif
#if defined(CONNECTIONS_SYNC_RESET)
#define CONNECTIONS_NEG_RESET_SIGNAL_IS(port) reset_signal_is(port,false)
#define CONNECTIONS_POS_RESET_SIGNAL_IS(port) reset_signal_is(port,true)
#else
#define CONNECTIONS_NEG_RESET_SIGNAL_IS(port) async_reset_signal_is(port,false)
#define CONNECTIONS_POS_RESET_SIGNAL_IS(port) async_reset_signal_is(port,true)
#endif

/**
 * \brief CONNECTIONS_POS_RESET define: Select polarity for synchronous or asynchronous reset.
 * \ingroup connections_module
 *
 * Connections uses active-low reset by default. Defining CONNECTIONS_POS_RESET
 * will use CONNECTIONS_POS_RESET_SIGNAL_IS() as defined by CONNECTIONS_SYNC_RESET
 *
 * Use CONNECTIONS_RESET_SIGNAL_IS() inplace of the active low/high specific macros above.
 */
#if defined(CONNECTIONS_POS_RESET)
#define CONNECTIONS_RESET_SIGNAL_IS(port) CONNECTIONS_POS_RESET_SIGNAL_IS(port)
#else
#define CONNECTIONS_RESET_SIGNAL_IS(port) CONNECTIONS_NEG_RESET_SIGNAL_IS(port)
#endif

/**
 * \brief CONNECTIONS_CONCAT define: Concatenate two strings, separate with an underscore.
 * \ingroup connections_module
 *
 * Useful for concatenating two strings to derive a module name.
 *
 * Catapult synthesis in C++11 mode currently doesn't support + operators on
 * strings, so only s2 is used.
 */
#if defined(__SYNTHESIS__) && __cplusplus >= 201103L
#define CONNECTIONS_CONCAT(s1,s2) (std::string(s2)).c_str()
#else
#define CONNECTIONS_CONCAT(s1,s2) (std::string(s1) + "_" + std::string(s2)).c_str()
#endif

//Define some overrides and defaults for RAND_SEED (also used by nvhls_rand.h).
#if defined(USE_TIME_RAND_SEED)
#undef RAND_SEED
#undef NVHLS_RAND_SEED
#elif defined(NVHLS_RAND_SEED)
#undef RAND_SEED
#define RAND_SEED NVHLS_RAND_SEED
#elif !defined(RAND_SEED)
#define RAND_SEED 19650218U
#define NVHLS_RAND_SEED 19650218U
#endif

namespace Connections
{
  // placeholder for utility code
}

#endif  // __CONNECTIONS__CONNECTIONS_UTILS_H_
