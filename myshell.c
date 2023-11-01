/*
    COMP3511 Fall 2023
    PA1: Simplified Linux Shell (MyShell)

    Your name: JEONG, YEONGMIN
    Your ITSC email: yjeongab@connect.ust.hk

    Declaration:

    I declare that I am not involved in plagiarism
    I understand that both parties (i.e., students providing the codes and students copying the codes) will receive 0 marks.

*/

/*
    Header files for MyShell
    Necessary header files are included.
    Do not include extra header files
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h> // For constants that are required in open/read/write/close syscalls
#include <sys/wait.h> // For wait() - suppress warning messages
#include <fcntl.h>    // For open/read/write/close syscalls
#include <signal.h>   // For signal handling

// Define template strings so that they can be easily used in printf
//
// Usage: assume pid is the process ID
//
//  printf(TEMPLATE_MYSHELL_START, pid);
//
#define TEMPLATE_MYSHELL_START "Myshell (pid=%d) starts\n"
#define TEMPLATE_MYSHELL_END "Myshell (pid=%d) ends\n"
#define TEMPLATE_MYSHELL_TERMINATE "Myshell (pid=%d) terminates by Ctrl-C\n"

// Assume that each command line has at most 256 characters (including NULL)
#define MAX_CMDLINE_LENGTH 256

// Assume that we have at most 8 arguments
#define MAX_ARGUMENTS 8

// Assume that we only need to support 2 types of space characters:
// " " (space) and "\t" (tab)
#define SPACE_CHARS " \t"

// The pipe character
#define PIPE_CHAR "|"

// Input / Output redirection characters
#define REDIRECT_CHAR "<>"

// Assume that we only have at most 8 pipe segements,
// and each segment has at most 256 characters
#define MAX_PIPE_SEGMENTS 8

// Assume that we have at most 8 arguments for each segment
// We also need to add an extra NULL item to be used in execvp
// Thus: 8 + 1 = 9
//
// Example:
//   echo a1 a2 a3 a4 a5 a6 a7
//
// execvp system call needs to store an extra NULL to represent the end of the parameter list
//
//   char *arguments[MAX_ARGUMENTS_PER_SEGMENT];
//
//   strings stored in the array: echo a1 a2 a3 a4 a5 a6 a7 NULL
//
#define MAX_ARGUMENTS_PER_SEGMENT 9

// Define the standard file descriptor IDs here
#define STDIN_FILENO 0  // Standard input
#define STDOUT_FILENO 1 // Standard output

// This function will be invoked by main()
// This function is given
int get_cmd_line(char *command_line)
{
    int i, n;
    if (!fgets(command_line, MAX_CMDLINE_LENGTH, stdin))
        return -1;
    // Ignore the newline character
    n = strlen(command_line);
    command_line[--n] = '\0';
    i = 0;
    while (i < n && command_line[i] == ' ')
    {
        ++i;
    }
    if (i == n)
    {
        // Empty command
        return -1;
    }
    return 0;
}

// read_tokens function is given
// This function helps you parse the command line
//
// Suppose the following variables are defined:
//
// char *pipe_segments[MAX_PIPE_SEGMENTS]; // character array buffer to store the pipe segements
// int num_pipe_segments; // an output integer to store the number of pipe segment parsed by this function
// char command_line[MAX_CMDLINE_LENGTH]; // The input command line
//
// Sample usage:
//
//  read_tokens(pipe_segments, command_line, &num_pipe_segments, "|");
//
void read_tokens(char **argv, char *line, int *numTokens, char *delimiter)
{
    int argc = 0;
    char *token = strtok(line, delimiter);
    while (token != NULL)
    {
        argv[argc++] = token;
        token = strtok(NULL, delimiter);
    }
    *numTokens = argc;
}

char* insert_spaces_around_operators(char *input) {
    char *output = malloc(strlen(input) * 3 +1);
    if (!output) {
        perror("malloc");
        exit(1);
    }

    char *p = output;
    for (int i = 0; i < strlen(input); i++) {
        if (strchr(REDIRECT_CHAR, input[i])) {
            *p++ = ' ';
            *p++ = input[i];
            *p++ = ' ';
        } else {
            *p++ = input[i];
        }
    }
    *p = '\0';

    return output;
}

void process_cmd(char *command_line, int input_fd, int output_fd)
{
        char *modified_segment = insert_spaces_around_operators(command_line);  

        int num_space_segments;
        char *space_segments[MAX_ARGUMENTS_PER_SEGMENT];
        read_tokens(space_segments, command_line, &num_space_segments, SPACE_CHARS);

        // debugging
        // for (int i = 0; i < num_space_segments; i++) {
        //     printf("Segment %d: %s\n", i, space_segments[i]);
        // }

        char *infile = NULL;
        char *outfile = NULL;

        // Check for redirection ('<>') and get the file names
        for (int j = 0; j < num_space_segments; j++) {
            if (space_segments[j] == NULL) {
                continue;
            }
            if (strcmp(space_segments[j], "<") == 0) {
                infile = space_segments[j + 1];
                space_segments[j] = NULL;
                space_segments[j + 1] = NULL;
                j++;
            } else if (strcmp(space_segments[j], ">") == 0) {
                outfile = space_segments[j + 1];
                space_segments[j] = NULL;
                space_segments[j + 1] = NULL;
                j++;
            }
        }

        // input redirection
        if (infile) {
            // file is an option(if file exists)
            int input_file_fd = open(infile, O_RDONLY);
            dup2(input_file_fd, STDIN_FILENO);
            close(input_file_fd);
            if (input_fd < -1) {
                perror("open");
                exit(1);
            }
        }

        // output redirection
        if (outfile) {
            // file is an option(if file exists)
            // Test) cat myshell.c > test10 | grep main
            int output_file_fd = open(outfile, O_CREAT | O_WRONLY, S_IRUSR | S_IWUSR);   // flag, user permission
            dup2(output_file_fd, STDOUT_FILENO);
            close(output_file_fd);
            if (output_fd < -1) {
                perror("open");
                exit(1);
            }
        }

        if (input_fd != STDIN_FILENO) {
            dup2(input_fd, STDIN_FILENO);
            close(input_fd);
        }

        if (output_fd != STDOUT_FILENO) {
            dup2(output_fd, STDOUT_FILENO);
            close(output_fd);
        }

        int new_argc = 0;
        for (int k = 0; k < num_space_segments; k++) {
            if (space_segments[k] != NULL) {
                new_argc++;
            }
        }
        space_segments[new_argc] = NULL;

        execvp(space_segments[0], space_segments);   // space_segments[0] as the command, space arguments as the arguments
        perror("execvp");
        exit(1);       
}

void signal_handler()
{
    pid_t current_pid = getpid();
    printf(TEMPLATE_MYSHELL_TERMINATE, current_pid);
    exit(1);
}


/* The main function implementation*/
int main()  // ROLE) Creating and Connecting Pipes 
{
    // TODO: replace the shell prompt with your ITSC account name
    // For example, if you ITSC account is cspeter@connect.ust.hk
    // You should replace ITSC with cspeter
    char *prompt = "yjeongab";
    char command_line[MAX_CMDLINE_LENGTH];

    // TODO:
    // The main function needs to be modified
    // For example, you need to handle the exit command inside the main function
    printf(TEMPLATE_MYSHELL_START, getpid());
    signal(SIGINT, signal_handler);
    
    // The main event loop
    while (1)
    {
        printf("%s> ", prompt);

        if (get_cmd_line(command_line) == -1)
            continue; /* empty line handling */

        if (strcmp(command_line, "exit") == 0) {
            printf(TEMPLATE_MYSHELL_END, getpid());
            exit(0);
        }

        int num_pipe_segments;
        char *pipe_segments[MAX_PIPE_SEGMENTS];
        read_tokens(pipe_segments, command_line, &num_pipe_segments, PIPE_CHAR);

        // debugging
        // for(int i = 0; i < num_pipe_segments; i++) {
        //     printf("Segment %d: %s\n", i, pipe_segments[i]);
        // }

        int pfds[2];
        int prev_pfds[2];
        int input_fd = STDIN_FILENO;
        int output_fd = STDOUT_FILENO;

        for (int i = 0; i < num_pipe_segments; i++) {
            if (i < num_pipe_segments - 1) {
                pipe(pfds);
            }

            pid_t pid = fork();

            if (pid == 0) {     // Child process
                if (i > 0) {
                    dup2(prev_pfds[0], STDIN_FILENO);   // Read from the previous pipe
                    close(prev_pfds[0]);
                    close(prev_pfds[1]);
                } 
                
                if (i < num_pipe_segments - 1) {
                    dup2(pfds[1], STDOUT_FILENO);   // Write to the next pipe
                    close(pfds[0]);
                    close(pfds[1]);
                }

                // If not last command
                input_fd = (i > 0) ? prev_pfds[0] : STDIN_FILENO;
                output_fd = (i < num_pipe_segments - 1) ? pfds[1] : STDOUT_FILENO;

                process_cmd(pipe_segments[i], input_fd, output_fd);

            } else {    // Parent process
                if (i > 0) {
                    close(prev_pfds[0]);
                    close(prev_pfds[1]);
                }

                if (i < num_pipe_segments - 1) {
                    prev_pfds[0] = pfds[0];
                    prev_pfds[1] = pfds[1];
                }
                wait(0);
            }
        }
    }
    return 0;
}

