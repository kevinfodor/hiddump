/*
 ==============================================================================
 Name        : utils.h
 Date        : Aug 19, 2011
 ==============================================================================

 BSD License
 -----------

 Copyright (c) 2011, and Kevin Fodor, All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:

 - Redistributions of source code must retain the above copyright notice,
 this list of conditions and the following disclaimer.

 - Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.

 - Neither the name of Kevin Fodor nor the names of
 its contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.

 NOTICE:
 SOME OF THIS CODE MAY HAVE ELEMENTS TAKEN FROM OTHER CODE WITHOUT ATTRIBUTION.
 IF THIS IS THE CASE IT WAS DUE TO OVERSIGHT WHILE DEBUGGING AND I APOLOGIZE.
 IF ANYONE HAS ANY REASON TO BELIEVE THAT ANY OF THIS CODE VIOLATES OTHER
 LICENSES PLEASE CONTACT ME WITH DETAILS SO THAT I MAY CORRECT THE SITUATION.

 ==============================================================================
 */

#ifndef UTILS_H_
#define UTILS_H_

#ifdef __cplusplus
extern "C"
{
#endif

	/* ************************************************************************* */
	/*!
	 \brief This header contains various common utilities.

	 \par
	 The information contained in this file are a collection of helpful
	 utilities in the form of macros which are common amongst any project.
	 The macros defined here are platform independent.

	 */
	/* ************************************************************************* */

// Byte/Word Extraction and Swapping macros
#define BYTE0(x)		    (uint8_t)(x)
#define BYTE1(x)		    (uint8_t)((x) >> 8)
#define BYTE2(x)		    (uint8_t)((x) >> 16)
#define BYTE3(x)		    (uint8_t)((x) >> 24)
#define BYTE4(x)		    (uint8_t)((x) >> 32)
#define BYTE5(x)		    (uint8_t)((x) >> 40)
#define BYTE6(x)		    (uint8_t)((x) >> 48)
#define BYTE7(x)		    (uint8_t)((x) >> 56)
#define WORD0(x)			(uint16_t)(x)
#define WORD1(x)			(uint16_t)((x) >> 16)

#define SWAPUN16(x)		(uint16_t)( BYTE0(x) << 8 | BYTE1(x) )

#define SWAPUN32(x)		(uint32_t)((uint32_t)(BYTE0((uint32_t)(x)) << 24) | \
							(uint32_t)(BYTE1((uint32_t)(x)) << 16 ) | \
							(uint32_t)(BYTE2((uint32_t)(x)) << 8) | \
							(uint32_t)(BYTE3((uint32_t)(x))))

// Error macros
#if defined _WIN32

#define print_err(code,text) do { \
fprintf(stderr, "%s at \"%s\":%d: %d\n", \
	text, __FILE__, __LINE__, code); \
} while (0)

#define print_errno(text) do { \
   fprintf(stderr, "%s at \"%s\":%d: %s\n", \
       text, __FILE__, __LINE__, strerror (GetLastError())); \
   } while (0)

#elif defined __GNUC__

#define print_err(code,text) do { \
   fprintf(stderr, "%s at \"%s\":%d: %s\n", \
       text, __FILE__, __LINE__, strerror (code)); \
   } while (0)

#define print_errno(text) do { \
   fprintf(stderr, "%s at \"%s\":%d: %s\n", \
       text, __FILE__, __LINE__, strerror (errno)); \
   } while (0)

#else

#error print_err is not defined!

#endif

// Structure packing macros for transport across a bus/interface
#ifndef __PACKED__
#if defined _WIN32
#define __PACKED__ _Pragma("pack(push, 1)")
#elif defined __GNUC__
#define __PACKED__ __attribute__ ((__packed__))
#else
#error Macro __PACKED__ is not defined!
#endif
#endif

#ifndef __UNPACKED__
#if defined _WIN32
#define __UNPACKED__ _Pragma("pack(pop)")
#elif defined __GNUC__
#define __UNPACKED__
#else
#error Macro __UNPACKED__ is not defined!
#endif
#endif

// This creates a constant string out of the macro argument provided
#define MACRO_TO_STRING(x)  # x

// This MACRO invokes a compiler-time assertion if the condition is not true
// Note: parameter 'msg' must be a non-white space string
#define COMPILE_TIME_ASSERT(condition, msg) \
    typedef char (msg)[(condition) ? 1 : -1]

// Machine alignment macros
#define MACHINE_ALIGNMENT ( sizeof( \
    struct alignment_struct{ UN8 un8Dummy; UN32 un32Dummy; } ) \
        / 2 )

#define ALIGN_SIZE(x) \
    { size_t tAlignment = MACHINE_ALIGNMENT; \
        (x) += ( (x) % tAlignment ) ? \
             /* true */ tAlignment - ( (x) % tAlignment ) : /* false */ 0; \
    }

// Determine the size of a member of a structure
#define SIZEOF(structure,member) ((size_t) sizeof(((structure *)0)->member))

// minimum and maximum macros
#define MIN(X,Y) ((X) < (Y) ? : (X) : (Y))
#define MAX(X,Y) ((X) > (Y) ? : (X) : (Y))

#ifdef __cplusplus
}
#endif

#endif /* UTILS_H_ */
