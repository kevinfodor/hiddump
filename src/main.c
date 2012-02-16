/*
 ==============================================================================
 Name        : main.c
 Date        : Aug 15, 2011
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
 This program requires the following libraries:

 C:\MinGW\lib

 setupapi.a - Contains the APIs needed for device detection. These are
 the SetupDi_API functions.
 hid.a -

 Compiler definitions
 WINVER=0x0501 - Windows XP
 (http://msdn.microsoft.com/en-us/library/aa383745(v=VS.85).aspx)
 */

// Standard includes
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>

// Windows includes
#include <windows.h>

// Other includes
#include "output.h"
#include "version.h"

// Project includes
#include "usb_defs.h"
#include "hiddump.h"

#define LINE_WIDTH      (80)
#define ARGS_MINIUMUM 	(0)

// Local declarations
static void credits(void);

static void title(void);

static void usage(void);

static int local_main(int argc, char **argv);

// Public declarations
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev_instance,
		char* command_line, int show_command);

// Implementation

static void credits(void)
{
	puts("Credits:");
	puts("\tserio: Simple nonblocking serial I/O module for Win32.\n"
			"\t\tDan Kegel - Copyright Activision 1998\n"
			"\t\thttp://alumnus.caltech.edu/~dank/overlap.htm");
	puts(
			"\tusbview: Microsoft Windows Driver Kit (WDK) sample application\n"
					"\t\thttp://msdn.microsoft.com/en-us/library/ff558728(v=VS.85).aspx");
	puts(
			"\thidclient: Microsoft Windows Driver Kit (WDK) sample application\n"
					"\t\thttp://msdn.microsoft.com/en-us/library/ff538800(v=vs.85).aspx");

	LINE(LINE_WIDTH, '-', true);

	return;
}

static void title(void)
{
	puts("hiddump - Kevin Fodor");
	version();

	LINE(LINE_WIDTH, '-', true);

	return;
}

static void usage(void)
{
	fprintf(stderr, "usage: hiddump [-vid #] [-pid #] [-e] [-d] [-r] [-v]\n");
	fprintf(stderr, "Where:\n");
	fprintf(stderr, "\t-vid The vendor-id of a USB device.\n");
	fprintf(stderr, "\t-pid The product-id of a USB device.\n");
	fprintf(stderr, "\t-e Enumerate all USB hcs, hubs and devices.\n");
	fprintf(stderr, "\t-d Descriptors for specified device id is output.\n");
	fprintf(stderr, "\t-r Real-time HID report parser.\n");
	fprintf(stderr, "\t-v Version information.\n");
	fprintf(stderr, "\n");

	return;
}

int local_main(int argc, char **argv)
{
	int i, result;

	InitializeOutput();

	title();

	// Parse command line args...
	for (i = 1; i < argc; i++) /* Skip argv[0] (program name). */
	{
		int cArgs = argc - 1;

		if (strcmp(argv[i], "-vid") == 0) /* Optional argument. */
		{
			i++;
			if (i <= cArgs) /* There are enough arguments in argv. */
			{
				int vid;

				if (argv[i][1] == 'x')
				{
					sscanf(argv[i], "0x%x", &vid);
				}
				else
				{
					sscanf(argv[i], "%d", &vid);
				}
				g_cmd_line_params.vid = vid;
			}
			else
			{
				/* Print usage statement and exit (see below). */
				usage();
				break;
			}
		}
		else if (strcmp(argv[i], "-pid") == 0) /* Optional argument. */
		{
			i++;
			if (i <= cArgs) /* There are enough arguments in argv. */
			{
				int pid;

				if (argv[i][1] == 'x')
				{
					sscanf(argv[i], "0x%x", &pid);
				}
				else
				{
					sscanf(argv[i], "%d", &pid);
				}
				g_cmd_line_params.pid = pid;
			}
			else
			{
				/* Print usage statement and exit (see below). */
				usage();
				break;
			}
		}
		else if (strcmp(argv[i], "-e") == 0) /* Optional argument. */
		{
			g_cmd_line_params.enumerate = true;
		}
		else if (strcmp(argv[i], "-d") == 0) /* Optional argument. */
		{
			g_cmd_line_params.show_descriptors = true;
		}
		else if (strcmp(argv[i], "-r") == 0) /* Optional argument. */
		{
			g_cmd_line_params.run_parser = true;
		}
		else if (strcmp(argv[i], "-v") == 0) /* Optional argument. */
		{
			credits();
			usage();
			return EXIT_SUCCESS;
		}
		else
		{
			/* Process non-optional arguments here. */
		}
	}

	printf("Using: VID=0x%0x, PID=0x%0x\n", g_cmd_line_params.vid,
			g_cmd_line_params.pid);

	// Run application
	result = hid_dump();

	puts("Done.");
	return result;
}

/*
 * Adapted from: http://www.flipcode.com/archives/WinMain_Command_Line_Parser.shtml
 * written by: by Max McGuire [amcguire@andrew.cmu.edu]
 *
 */
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prev_instance,
		char* command_line, int show_command)
{
	// ANSI-C main() call parameters
	int argc;
	char** argv;

	char* arg;
	int index, result;
	char filename[_MAX_PATH];

	// Copy windows stuff
	g_cmd_line_params.hInstance = hInstance;

	// count the arguments given

	argc = 1;
	arg = command_line;

	while (arg[0] != 0)
	{
		while (arg[0] != 0 && arg[0] == ' ')
		{
			arg++;
		}

		if (arg[0] != 0)
		{
			argc++;

			while (arg[0] != 0 && arg[0] != ' ')
			{
				arg++;
			}
		}
	}

	// tokenize the arguments

	argv = (char**) malloc(argc * sizeof(char*));
	if (NULL == argv)
	{
		return EXIT_FAILURE;
	}

	arg = command_line;
	index = 1;

	while (arg[0] != 0)
	{

		while (arg[0] != 0 && arg[0] == ' ')
		{
			arg++;
		}

		if (arg[0] != 0)
		{
			argv[index] = arg;
			index++;

			while (arg[0] != 0 && arg[0] != ' ')
			{
				arg++;
			}

			if (arg[0] != 0)
			{
				arg[0] = 0;
				arg++;
			}
		}

	}

	// put the program name into argv[0]

	GetModuleFileName(NULL, filename, _MAX_PATH);
	argv[0] = filename;

	// call the user specified main function

	result = local_main(argc, argv);

	free(argv);

	puts("WinMain: Done.");
	return result;
}
