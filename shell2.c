#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>

#define MAX_COMMAND_LINE_LEN 1024
#define MAX_COMMAND_LINE_ARGS 128

char prompt[] = "> ";
char delimiters[] = " \t\r\n";
extern char **environ;

// Signal handler for SIGINT (Ctrl+C)
void handle_sigint(int sig) {
    printf("\nCaught signal %d. Type 'exit' to quit.\n", sig);
    printf("%s", prompt);
    fflush(stdout);
}

void execute_command(char *arguments[], int background) {
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        if (background == 0) {
            // Set an alarm to terminate process if it exceeds time limit
            alarm(10);
        }

        // Execute command using execvp
        execvp(arguments[0], arguments);
        perror("execvp failed"); // If execvp returns, it failed
        exit(1);
    } else if (pid > 0) {
        // Parent process
        if (background == 0) {
            // Wait for the foreground process
            waitpid(pid, NULL, 0);
        } else {
            // Print background process info
            printf("[Running in background] PID: %d\n", pid);
        }
    } else {
        perror("fork failed"); // Error in forking
    }
}

int main() {
    char command_line[MAX_COMMAND_LINE_LEN];
    char *arguments[MAX_COMMAND_LINE_ARGS];

    // Register signal handler
    signal(SIGINT, handle_sigint);

    while (true) {
        // Print the shell prompt with the current working directory
        char cwd[MAX_COMMAND_LINE_LEN];
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s %s", cwd, prompt);
        } else {
            perror("getcwd() error");
        }
        fflush(stdout);

        // Read input from stdin
        if ((fgets(command_line, MAX_COMMAND_LINE_LEN, stdin) == NULL) && ferror(stdin)) {
            fprintf(stderr, "fgets error");
            exit(0);
        }

        // If EOF (Ctrl+D) is encountered, exit the shell
        if (feof(stdin)) {
            printf("\n");
            fflush(stdout);
            fflush(stderr);
            return 0;
        }

        // Remove newline character
        if (command_line[strlen(command_line) - 1] == '\n') {
            command_line[strlen(command_line) - 1] = '\0';
        }

        // Tokenize the command line input
        int i = 0;
        char *token = strtok(command_line, delimiters);
        while (token != NULL && i < MAX_COMMAND_LINE_ARGS - 1) {
            arguments[i++] = token;
            token = strtok(NULL, delimiters);
        }
        arguments[i] = NULL; // Null-terminate the array of arguments

        // Handle built-in commands
        if (arguments[0] != NULL) {
            if (strcmp(arguments[0], "exit") == 0) {
                break; // Exit the shell
            } else if (strcmp(arguments[0], "cd") == 0) {
                if (arguments[1] != NULL) {
                    if (chdir(arguments[1]) != 0) perror("cd failed");
                } else {
                    fprintf(stderr, "cd: missing argument\n");
                }
                continue; // Skip to the next iteration of the loop
            } else if (strcmp(arguments[0], "pwd") == 0) {
                if (getcwd(command_line, sizeof(command_line)) != NULL) {
                    printf("%s\n", command_line);
                } else {
                    perror("pwd failed");
                }
                continue;
            } else if (strcmp(arguments[0], "echo") == 0) {
                // Handle echo command with environment variable substitution
                for (int j = 1; arguments[j] != NULL; j++) {
                    if (arguments[j][0] == '$') {
                        char *env_value = getenv(arguments[j] + 1);
                        if (env_value != NULL) {
                            printf("%s ", env_value);
                        }
                    } else {
                        printf("%s ", arguments[j]);
                    }
                }
                printf("\n");
                continue;
            } else if (strcmp(arguments[0], "env") == 0) {
                // Print environment variables
                for (int j = 0; environ[j] != NULL; j++) {
                    printf("%s\n", environ[j]);
                }
                continue;
            } else if (strcmp(arguments[0], "setenv") == 0) {
                if (arguments[1] != NULL && arguments[2] != NULL) {
                    setenv(arguments[1], arguments[2], 1); // Overwrite existing values
                } else {
                    fprintf(stderr, "setenv: missing arguments\n");
                }
                continue;
            }
        }

        // Check for background process (ends with '&')
        int background = 0;
        if (arguments[i - 1] != NULL && strcmp(arguments[i - 1], "&") == 0) {
            arguments[i - 1] = NULL; // Remove '&' from argument list
            background = 1; // Set background flag
        }

        // Execute command (non-built-in)
        execute_command(arguments, background);
    }

    return 0;
}
