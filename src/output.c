/*
 ==============================================================================
 Name        : output.c
 Date        : July 5, 2011
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

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Windows includes
#include <io.h>
#include <fcntl.h>
#include <windows.h>
#include <ctype.h>

// Project includes
#include "output.h"

void Message(const char *pMsg, uint32_t count)
{
	printf("%s #%u\n", pMsg, count);
	return;
}

void _STRING(const char *pText)
{
	printf("'%s'", pText);
	return;
}

void LINE(unsigned int num, char c, bool nl)
{
	if (num > 0)
	{
		do
		{
			putchar(c);
		} while (--num);
		if (nl == true)
			putchar('\n');
	}
	return;
}

void HEADER(const char *pText)
{
	printf("\n[%s]\n", pText);
	return;
}

void HEADER_INDEX(const char *pText, uint32_t idx)
{
	printf("\n[%s] %u\n", pText, idx);
	return;
}

void HEADER_ARRAY(const char *pText, uint32_t idx, uint32_t total)
{
	printf("\n[%s] %u of %u\n", pText, idx, total);
	return;
}

void FERROR(const char *pFunction)
{
	DWORD dw = GetLastError();
	printf("%s failed with error %lu: %s\n", pFunction, dw, pFunction);
	return;
}

void InitializeOutput(void)
{
	int hCrt;
	FILE *hf;

	AllocConsole();
	hCrt = _open_osfhandle((long) GetStdHandle(STD_OUTPUT_HANDLE), _O_TEXT);
	hf = _fdopen(hCrt, "w");
	*stdout = *hf;
	setvbuf(stdout, NULL, _IONBF, 0);
	return;
}

int Puts(const char *text)
{
	int retVal;
	retVal = puts(text);
	return retVal;
}

int Printf(const char *text, ...)
{
	int retVal;
	va_list tArgList; // variable arguments list

	// grab variable arguments from the stack
	va_start(tArgList, text);

	// Send formatted output to stream
	retVal = vprintf(text, tArgList);

	// restore stack
	va_end(tArgList);

	return retVal;
}
