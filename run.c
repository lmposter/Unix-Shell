#include "cscshell.h"

// COMPLETE
int cd_cscshell(const char *target_dir){
    if (target_dir == NULL) {
        char user_buff[MAX_USER_BUF];
        if (getlogin_r(user_buff, MAX_USER_BUF) != 0) {
           perror("run_command");
           return -1;
        }
        struct passwd *pw_data = getpwnam((char *)user_buff);
        if (pw_data == NULL) {
           perror("run_command");
           return -1;
        }
        target_dir = pw_data->pw_dir;
    }

    if(chdir(target_dir) < 0){
        perror("cd_cscshell");
        return -1;
    }
    return 0;
}

/*
** Executes a single "line" of commands (through pipes)
** If a command fails, the rest of the line should not be executed.
** Input: head - a pointer to the head of a list of commands to be executed.
** Return value: a pointer to a heap integer on success. If the line is a `cd` command, the
*/
int count_commands(Command *head) {
    int count = 0;
    Command *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

/*
** Executes a single "line" of commands (through pipes)
** If a command fails, the rest of the line should not be executed.
**
** The error code from the last command is returned through a pointer
** to a heap integer on success. If the line is a `cd` command, the
** return value of `cd_cscshell` is stored by the heap int.
** -- If there are no commands to execute, returns NULL
** -- If there were any errors starting any commands,
**    returns (pointer value) -1
*/
int *execute_line(Command *head) {
    if (!head) return NULL;
    Command *current = head;

    int *status = malloc(sizeof(int));
    if (!status) {
        perror("malloc");
        free_command(head);
        return (int *)(intptr_t)-1;
    }

    // Handle built-in commands like 'cd' directly
    if (strcmp(current->exec_path, "cd") == 0) {
        *status = cd_cscshell(current->args[1]);
        free_command(head);
        return status;
    }

    int pipefd[2];
    int command_count = count_commands(head);
    pid_t *pids = malloc(sizeof(pid_t) * command_count);
    
    if (!pids) {
        perror("malloc");
        free_command(head);
        *status = -1;
        return status;
    }
    int pid_count = 0;

    while (current) {
        // Setup pipe for command chaining
        if (current->redir_out_path && current->next) {
            free_command(head);
            free(pids);
            ERR_PRINT(ERR_EXECUTE_LINE);
            *status = -1;
            return status;
            
        } else if (current->next) {
            if (pipe(pipefd) == -1) {
                perror("pipe");
                free_command(head);
                free(pids);
                *status = -1;
                return status;
            }
            current->stdout_fd = pipefd[1];
            current->next->stdin_fd = pipefd[0];
        }

        pid_t pid = run_command(current);
        if (pid < 0) {
            perror("run_command");
            free_command(head);
            free(pids);
            *status = -1;
            return status;
        } else {
            pids[pid_count++] = pid;
        }

        if (current->stdout_fd != STDOUT_FILENO) { 
            close(current->stdout_fd);
        }

        if (current != head && current->stdin_fd != STDIN_FILENO) {
            close(current->stdin_fd);
        }
        current = current->next;
    }

    // Wait for all commands to finish
    for (int i = 0; i < pid_count; i++) {
        waitpid(pids[i], status, 0);
    }

    free(pids);
    free_command(head);
    return status;
}


/*
** Forks a new process and execs the command
** making sure all file descriptors are set up correctly.
**
** Parent process returns -1 on error.
** Any child processes should not return.
*/
pid_t run_command(Command *command){
    #ifdef DEBUG
    printf("Running command: %s\n", command->exec_path);
    printf("Argvs: ");
    if (command->args == NULL){
        printf("NULL\n");
    }
    else if (command->args[0] == NULL){
        printf("Empty\n");
    }
    else {
        for (int i=0; command->args[i] != NULL; i++){
            printf("%d: [%s] ", i+1, command->args[i]);
        }
    }
    printf("\n");
    printf("Redir out: %s\n Redir in: %s\n",
           command->redir_out_path, command->redir_in_path);
    printf("Stdin fd: %d | Stdout fd: %d\n",
           command->stdin_fd, command->stdout_fd);
    #endif
    
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    } else if (pid == 0) {
        // Child process
        if (command->stdin_fd != 0) {
            dup2(command->stdin_fd, STDIN_FILENO);
            close(command->stdin_fd);
        }
        if (command->stdout_fd != STDOUT_FILENO) {
            dup2(command->stdout_fd, STDOUT_FILENO);
            close(command->stdout_fd);
        } else if (command->redir_out_path) {
            // Handle redirection of stdout
            int fd = open(command->redir_out_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            dup2(fd, STDOUT_FILENO);
            close(fd); // Close the original file descriptor as it's no longer needed
        }

        // Execute the command
        execvp(command->exec_path, command->args);
        // If execvp returns, it means an error occurred
        perror("execvp");
        return -1;
    } else {
        #ifdef DEBUG
        printf("Parent process created child PID [%d] for %s\n", pid, command->exec_path);
        #endif
    }
    return pid;
}

/*
** Executes an entire script line-by-line.
** Stops and indicates an error as soon as any line fails.
** Returns 0 on success, -1 on error
** Arguments:
**   file_path - a string representing the path to the script file.
**   root - a pointer to the head of a list of variables that may be used or modified by the script.
** Return values:
**   0 on successful execution of the script,
**  -1 if an error occurs (file opening failure, command execution failure).
*/
int run_script(char *file_path, Variable **root){
    // Attempt to open the specified file for reading
    FILE *file = fopen(file_path, "r");
    if (file == NULL) {
        perror("Error opening file");
        return -1; // Indicate error opening the file
    }

    char *line = NULL; // Pointer to hold the current line read from the file
    size_t len = 0; // Length of the line
    ssize_t read; // Number of characters read
    int *status; // Pointer to hold the status returned by command execution

    // Read the file line by line
    while ((read = getline(&line, &len, file)) != -1) {
        // Parse the current line into a command
        Command *command = parse_line(line, root);
        // If parsing returns NULL, skip execution and move to the next line
        if (command == NULL) {
            continue;
        }

        // Execute the parsed command
        status = execute_line(command);
        // Check execution status; if NULL or indicates error, clean up and exit
        if (status == NULL || *status != 0) {
            free(status);
            fclose(file);
            if (line) {
                free(line);
            }
            return -1; 
        }

        // Clean up after successful command execution
        free(status);
    }

    // Clean up and return success
    fclose(file); // Close the file
    if (line) {
        free(line); // Free memory allocated for the last read line
    }

    return 0; // Indicate successful script execution
}

/*
** Implement the following function that frees all the
** heap memory associated with a particular command.
** Arguments:
**   command - a pointer to the Command structure to be freed.
** Return value: None.
*/
void free_command(Command *command) {
    if (command == NULL) {
        return; // If the command is NULL, there's nothing to free, so return immediately.
    }

    // Free the executable path string, if it's not NULL.
    if (command->exec_path != NULL) {
        free(command->exec_path);
    }

    // Free each argument string in the args array, then free the args array itself.
    if (command->args != NULL) {
        char **arg = command->args;
        while (*arg != NULL) { // Iterate through each argument until the NULL sentinel is found.
            free(*arg); 
            arg++;
        }
        free(command->args); // Once all arguments are freed, free the array itself.
    }

    // Free the input redirection path string, if it's not NULL.
    if (command->redir_in_path != NULL) {
        free(command->redir_in_path);
    }

    // Free the output redirection path string, if it's not NULL.
    if (command->redir_out_path != NULL) {
        free(command->redir_out_path);
    }

    // If there's a next command linked, recursively call free_command to free it.
    if (command->next != NULL) {
        free_command(command->next);
    }

    // After all associated data has been freed, free the command structure itself.
    free(command);
}