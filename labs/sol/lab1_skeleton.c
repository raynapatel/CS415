/*
 * lab1_skeleton.c
 *
 *  Created on: Nov 25, 2020
 *      Author: Guan, Xin, Monil
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GNU_SOURCE

int main(int argc, char const *argv[])
{
	//checking for command line argument
	if (argc != 2)
	{
		printf ("Usage ./lab1.exe intput.txt\n");
	}
	//opening file to read
	FILE *inFPtr;
	inFPtr = fopen (argv[1], "r");

	//declear line_buffer
	size_t len = 128;
	char* line_buf = malloc (len);

	command_line large_token_buffer;
	command_line small_token_buffer;

	int line_num = 0;

	//loop until the file is over
	while (getline (&line_buf, &len, inFPtr) != -1)
	{
		printf ("Line %d:\n", ++line_num);

		//tokenize line buffer
		//large token is seperated by ";"
		large_token_buffer = str_filler (line_buf, ";");
		//iterate through each large token
		for (int i = 0; large_token_buffer.command_list[i] != NULL; i++)
		{
			printf ("\tLine segment %d:\n", i + 1);

			//tokenize large buffer
			//smaller token is seperated by " "(space bar)
			small_token_buffer = str_filler (large_token_buffer.command_list[i], " ");

			//iterate through each smaller token to print
			for (int j = 0; small_token_buffer.command_list[j] != NULL; j++)
			{
				printf ("\t\tToken %d: %s\n", j + 1, small_token_buffer.command_list[j]);
			}

			//free smaller tokens and reset variable
			free_command_line(&small_token_buffer);
			memset (&small_token_buffer, 0, 0);
		}

		//free smaller tokens and reset variable
		free_command_line (&large_token_buffer);
		memset (&large_token_buffer, 0, 0);
	}
	fclose(inFPtr);
	//free line buffer
	free (line_buf);
}
