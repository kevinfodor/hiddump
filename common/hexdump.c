/*
 ==============================================================================
 Name        : hexdump.c
 Date        : Sep 26, 2011
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
#include <ctype.h>

// Other includes

// Module include
#include "hexdump.h"

// Local declarations
#define STDIO_DUMP_WIDTH (16)

// Implementation
void hex_dump(FILE *p_file, uint16_t * p_row, uint8_t const * p_data,
		size_t data_length)
{
	/*
	 Print output similar to as follows:

	 00000001: xx xx xx xx xx xx xx xx   xx xx xx xx xx xx xx xx   ..abc..t xyz..ty
	 00000002: xx xx xx xx xx xx xx xx   xx xx xx xx xx xx xx xx   ..abc..t xyz..ty
	 00000003: xx xx xx xx xx xx xx xx   xx xx xx xx xx xx xx xx   ..abc..t xyz..ty
	 */
	uint8_t un8ColumnIndex, un8Columns;
	uint16_t un16RowIndex, un16Rows, un16RowStart;
	uint32_t un32Index = 0;
	char acRow[8 + 1 + 1];
	char acHex[2][(3 * (STDIO_DUMP_WIDTH / 2)) + 1];
	char acASCII[STDIO_DUMP_WIDTH + 1];
	bool bAddresss = false;

	if (data_length)
	{
		// First check if they are providing a ROW pointer if not,
		// use our own row.
		if (p_row != NULL)
		{
			// Start with this row
			un16RowStart = *p_row;
		}
		else
		{
			// Don't use row numbers, instead use an address
			bAddresss = true;

			// Print mast header
			fprintf(p_file, "Hex/ASCII Dump: %d bytes\n", data_length);
		}

		// Determine the number of rows to print

		// Check if it's an even number of rows
		if (data_length % STDIO_DUMP_WIDTH)
		{
			// it's not even so divide and add 1
			un16Rows = (data_length / STDIO_DUMP_WIDTH) + 1;
		}
		else
		{
			// it's even so just divide
			un16Rows = data_length / STDIO_DUMP_WIDTH;
		}

		// For each row...
		for (un16RowIndex = 1; un16RowIndex <= un16Rows; un16RowIndex++)
		{
			// Report current row back to caller.
			if (p_row != NULL)
			{
				// Calculate current row
				*p_row = un16RowIndex + un16RowStart;
			}

			// record row(line) number
			if (bAddresss == false)
			{
				// Print row number
				snprintf(acRow, sizeof(acRow), "%8.8d:",
						un16RowIndex + un16RowStart - 1);
			}
			else
			{
				// Print address
				snprintf(acRow, sizeof(acRow), "%p:",
						(void *) (p_data + un32Index));
			}

			// Init the number of columns to print based on the remaining length
			if ((data_length - un32Index) < STDIO_DUMP_WIDTH)
			{
				// init with remaining number
				un8Columns = data_length - un32Index;
			}
			else
			{
				// init with maximum in a row
				un8Columns = STDIO_DUMP_WIDTH;
			}

			// Clear out previous hex and ASCII info by setting the strings
			// to NULL
			acHex[0][0] = acHex[1][0] = acASCII[0] = '\0';

			// Print hex notation in columns for each one
			// also build a string containing the data for ASCII display
			for (un8ColumnIndex = 0; un8ColumnIndex < un8Columns;
					un8ColumnIndex++)
			{
				// Check if this is in the first or second half
				if (un8ColumnIndex < (STDIO_DUMP_WIDTH / 2))
				{
					// First half
					// Print hex representation
					snprintf(&acHex[0][0] + (un8ColumnIndex * 3),
							sizeof(acHex[0]) - (un8ColumnIndex * 3), " %2.2x",
							*(p_data + un32Index));
				}
				else
				{
					// Second half
					// Print hex representation
					snprintf(
							&acHex[1][0]
									+ ((un8ColumnIndex - (STDIO_DUMP_WIDTH / 2))
											* 3),
							sizeof(acHex[1])
									- ((un8ColumnIndex - (STDIO_DUMP_WIDTH / 2))
											* 3), " %2.2x",
							*(p_data + un32Index));
				}

				// Store string representation if it is printable, otherwise
				// we just store a '.'
				if (isgraph(*(p_data + un32Index)))
				{
					snprintf(acASCII + un8ColumnIndex,
							sizeof(acASCII) - un8ColumnIndex, "%c",
							*(p_data + un32Index));
				}
				else
				{
					snprintf(acASCII + un8ColumnIndex,
							sizeof(acASCII) - un8ColumnIndex, ".");
				}

				// move on to the next one!
				un32Index++;
			}

			// We have now completed a row, so let's print it
			fprintf(p_file, "%9s%-24s  %-24s  %-16s\n", acRow, &acHex[0][0],
					&acHex[1][0], acASCII);
		}

		// Blank line (if not a successive call)
		if (p_row == NULL)
		{
			fprintf(p_file, "\n");
		}
	}

	return;
}
