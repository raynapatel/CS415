/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan, Monil
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE
// #define _GNU_SOURCE

int count_token (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through string counting tokens
	*		Cases to watchout for
	*			a.	string start with delimeter
	*			b. 	string end with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of token (note not number of delimeter)
	*/

	/* solution 1 */
	if(buf == NULL || delim == NULL){
		return 0;
	}
	int count = 0;
	char *saveptr;
	char *token = strtok_r(buf, delim, &saveptr);
	
	while(token != NULL){
		count++;
		token = strtok_r(NULL, delim, &saveptr);
	}
	return count;

	/* solution 2 */
	// char *saveptr;
	// int count = 0;
	// char *buf_copy = strdup(buf);
	// // using another ptr to keep original buf_copy (strtok_r will change buf_copy)
	// char *org_prt = buf_copy;
	// buf_copy = strtok_r(buf_copy, delim, &saveptr);

	// while(buf_copy != NULL){
	// 	buf_copy = strtok_r(NULL, delim, &saveptr);
	// 	count++;
	// }
	// free(org_prt);
	// return count;
}

command_line str_filler (char* buf, const char* delim)
{
	//TODO：
	/*
	*	#1.	create command_line variable to be filled and returned
	*	#2.	count the number of tokens with count_token function, set num_token. 
    *           one can use strtok_r to remove the \n at the end of the line.
	*	#3. malloc memory for token array inside command_line variable
	*			based on the number of tokens.
	*	#4.	use function strtok_r to find out the tokens 
    *   #5. malloc each index of the array with the length of tokens,
	*			fill command_list array with tokens, and fill last spot with NULL.
	*	#6. return the variable.
	*/

	command_line cmd;
	cmd.num_token = 0;
	cmd.command_list = NULL;

	if(buf == NULL || delim == NULL){
		return cmd;
	}

	// remove newline
	size_t len = strlen(buf);
    if (len > 0 && buf[len-1] == '\n') {
        buf[len-1] = '\0';
    }

	// template string copied from original string
	char *tmp = strdup(buf);

	// count the number of tokens
	cmd.num_token = count_token(tmp, delim);
	if (cmd.num_token == 0) {
		free(tmp);
        return cmd;
    }
	free(tmp);

	// malloc space for command list
	cmd.command_list = (char**)malloc((cmd.num_token + 1) * sizeof(char*));
    // allocation failed
	if (cmd.command_list == NULL) {
        cmd.num_token = 0;
		free(cmd.command_list);
		perror("cmd list malloc failed");
        return cmd;
    }

	int i = 0;
	char* saveptr;
	char *buf_copy = strdup(buf);
    char *token = strtok_r(buf_copy, delim, &saveptr);
    while (token != NULL) {
		// allocate space for each token
        cmd.command_list[i] = (char*)malloc(strlen(token) + 1);
        if (cmd.command_list[i] == NULL) {
			// allocation failed
			perror("cmd list malloc failed");
			// free cmd
            free_command_line(&cmd);
			
            cmd.command_list = NULL;
            cmd.num_token = 0;
            return cmd;
        }
        strcpy(cmd.command_list[i], token);
        i++;
        token = strtok_r(NULL, delim, &saveptr);
    }
    cmd.command_list[i] = NULL;
	free(buf_copy);

    return cmd;
}


void free_command_line(command_line* command)
{
	//TODO：
	/*
	*	#1.	free the array base num_token
	*/
	if (command == NULL || command->command_list == NULL) {
        return;
    }

    for (int i = 0; i < command->num_token; i++) {
        free(command->command_list[i]);
    }

    free(command->command_list);

    command->command_list = NULL;
    command->num_token = 0;

}
