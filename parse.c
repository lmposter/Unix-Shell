#include "cscshell.h"
#include <ctype.h>
#include <assert.h>

#define CONTINUE_SEARCH NULL

// COMPLETE
char *resolve_executable(const char *command_name, Variable *path){

    if (command_name == NULL || path == NULL){
        return NULL;
    }

    if (strcmp(command_name, CD) == 0){
        return strdup(CD);
    }

    if (strcmp(path->name, PATH_VAR_NAME) != 0){
        ERR_PRINT(ERR_NOT_PATH);
        return NULL;
    }

    char *exec_path = NULL;

    if (strchr(command_name, '/')){
        exec_path = strdup(command_name);
        if (exec_path == NULL){
            perror("resolve_executable");
            return NULL;
        }
        return exec_path;
    }

    // we create a duplicate so that we can mess it up with strtok
    char *path_to_toke = strdup(path->value);
    if (path_to_toke == NULL){
        perror("resolve_executable");
        return NULL;
    }
    char *current_path = strtok(path_to_toke, ":");

    do {
        DIR *dir = opendir(current_path);
        if (dir == NULL){
            ERR_PRINT(ERR_BAD_PATH, current_path);
            closedir(dir);
            continue;
        }

        struct dirent *possible_file;

        while (exec_path == NULL) {
            // rare case where we should do this -- see: man readdir
            errno = 0;
            possible_file = readdir(dir);
            if (possible_file == NULL) {
                if (errno > 0){
                    perror("resolve_executable");
                    closedir(dir);
                    goto res_ex_cleanup;
                }
                // end of files, break
                break;
            }

            if (strcmp(possible_file->d_name, command_name) == 0){
                // +1 null term, +1 possible missing '/'
                size_t buflen = strlen(current_path) +
                    strlen(command_name) + 1 + 1;
                exec_path = (char *) malloc(buflen);
                // also sets remaining buf to 0
                strncpy(exec_path, current_path, buflen);
                if (current_path[strlen(current_path)-1] != '/'){
                    strncat(exec_path, "/", 2);
                }
                strncat(exec_path, command_name, strlen(command_name)+1);
            }
        }
        closedir(dir);

        // if this isn't null, stop checking paths
        if (possible_file) break;

    } while ((current_path = strtok(CONTINUE_SEARCH, ":")));

res_ex_cleanup:
    free(path_to_toke);
    return exec_path;
}

/**
 * Preprocesses a given line by adding spaces around certain symbols ('|', '<', '>'),
 * ensuring there are no additional spaces where they're not needed.
 * 
 * @param line The input line to be processed. It is a constant pointer to ensure
 *             the input is not modified.
 * 
 * @return A pointer to the newly allocated string that has been processed.
 *         Returns NULL if the input line is NULL.
 */
char* preprocess_line(const char* line) {
    // Return NULL immediately if the input line is NULL.
    if (line == NULL) return NULL;

    // Allocate memory for the processed line, considering worst-case scenario for spacing.
    char* processed_line = malloc(strlen(line) * 3 + 1); // 3 times the length for spaces + 1 for null terminator.
    if (processed_line == NULL) {
        perror("malloc"); // Print error message if memory allocation fails.
        exit(EXIT_FAILURE); // Exit the program if memory allocation fails.
    }

    const char* current_char = line;
    char* new_char_position = processed_line; // Pointer to append characters to the processed line.

    // Loop through each character of the input line.
    while (*current_char != '\0') {
        // Check if the current character is one of the specified symbols.
        if (*current_char == '|' || *current_char == '<' || *current_char == '>') {
            // Add a space before the symbol if it's not the first character and the previous one isn't a space.
            if (new_char_position != processed_line && *(new_char_position - 1) != ' ') {
                *new_char_position++ = ' ';
            }
            *new_char_position++ = *current_char; // Add the symbol to the processed line.

            // Special handling for ">>" to avoid adding an extra space.
            if (*current_char == '>' && *(current_char + 1) == '>') {
                *new_char_position++ = *++current_char; // Skip the next '>' character.
            }

            // Add a space after the symbol if the next character isn't a space or the end of the string.
            if (*(current_char + 1) != ' ' && *(current_char + 1) != '\0') {
                *new_char_position++ = ' ';
            }
        } else {
            *new_char_position++ = *current_char; // Add the current character to the processed line.
        }
        current_char++; // Move to the next character in the input line.
    }

    *new_char_position = '\0'; // Null-terminate the processed line.
    return processed_line;
}

// Handles the assignment of values to variables. If the variable already exists in the list, its value is updated.
// If it does not exist, a new variable is created and added to the list.
// Arguments:
//   token - a string containing the variable name, an equals sign, and the value to assign.
//   variables - a pointer to the head pointer of the list of variables.
// Return value: None.
int handle_variable_assignment(char* token, Variable** variables) {
    char *saveptr1; // For strtok_r's internal use

    // Extract the variable name from the token
    char* name = strtok_r(token, "=", &saveptr1);
    // Validate the variable name
    for(char *p = name; *p; p++) {
        if (!isalpha((unsigned char)*p) && *p != '_') {
            ERR_PRINT(ERR_VAR_NAME, name);
            return 1;
        }
    }

    // Extract the variable value from the token
    char* value = strtok_r(NULL, "=\n", &saveptr1);
    // Default value to an empty string if none is provided
    if (value == NULL) {
        value = "";
    }

    // Look for the variable in the existing list
    Variable* var;
    for (var = *variables; var != NULL; var = var->next) {
        if (strcmp(var->name, name) == 0) {
            break; // Variable found
        }
    }

    if (var != NULL) {
        // Update the value of an existing variable
        free(var->value);
        var->value = strdup(value);
        if (var->value == NULL) {
            perror("strdup");
            return 1;
        }
    } else {
        // Add a new variable to the list
        var = malloc(sizeof(Variable));
        if (!var) {
            perror("malloc");
            return 1;
        }

        // Initialize the new variable
        var->name = strdup(name);
        if (var->name == NULL) {
            perror("strdup");
            return 1;
        }

        var->value = strdup(value);
        if (var->value == NULL) {
            perror("strdup");
            return 1;
        }

        var->next = NULL;

        // Insert the new variable into the list
        if (*variables == NULL) {
            // The list is empty, make the new variable the head
            *variables = var;
        } else {
            // Append the new variable to the end of the list
            Variable *current = *variables;
            while (current->next != NULL) {
                current = current->next;
            }
            current->next = var;
        }
    }
    return 0;
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
Command* parse_line(char* line, Variable** variables) {
    
    // Check if the line only contains white spaces
    char *temp = line;
    while (*temp != '\0') {
        if (!isspace((unsigned char)*temp)) {
            if (*temp == '#') {
                return NULL;
            }
            break;
        }
        temp++;
    }

    if (*temp == '|' || *temp == '<' || *temp == '>') {
        return (Command *)-1;
    }
    // If the line only contains white spaces, return NULL
    if (*temp == '\0') {
        return NULL;
    }

    Command* command = malloc(sizeof(Command));
    if (!command) {
        perror("malloc");
        return (Command *)-1;
    }

    Command* current_command = command;

    // Initialize command
    command->exec_path = NULL;
    command->args = malloc(sizeof(char*));
    command->args[0] = NULL;
    command->next = NULL;
    command->stdin_fd = 0;
    command->stdout_fd = 1;
    command->redir_in_path = NULL;
    command->redir_out_path = NULL;
    command->redir_append = 0;

    // Tokenize the line
    char *var_token = strdup(line);
    if (var_token == NULL) {
        free_command(command);
        perror("strdup");
        return (Command *)-1;
    }
    char *saveptr;
    char *processed = preprocess_line(line);
    char *token = strtok_r(processed, " ", &saveptr);
    char *new;
    

    while (token != NULL) {
        // If the token is a comment, break the loop
        if (token[0] == '#') {
            break;
        }
        // If the token is a variable assignment
        else if (strchr(token, '=')) {
            if(handle_variable_assignment(var_token, variables) == 1) {
                free_command(command);
                free(var_token);
                free(processed);
                ERR_PRINT(ERR_EXECUTE_LINE);
                return (Command *)-1;
            }
        }
        // If the token is a redirection symbol
        else if (strcmp(token, "<") == 0 || strcmp(token, ">") == 0 || strcmp(token, ">>") == 0) {
            char* symbol = token;
            token = strtok_r(NULL, " ", &saveptr);
            if (token == NULL) {
                free_command(command);
                free(var_token);
                free(processed);
                ERR_PRINT(ERR_EXECUTE_LINE);
                return (Command *)-1;
            }
            new = replace_variables_mk_line(token, *variables);
            if (new == (char *)-1) {
                free_command(command);
                free(var_token);
                free(processed);
                ERR_PRINT(ERR_EXECUTE_LINE);
                return (Command *)-1;
            }
            if (new == NULL) {
                free_command(command);
                free(var_token);
                free(processed);
                ERR_PRINT(ERR_EXECUTE_LINE);
                return (Command *)-1;
            }
            if (strcmp(symbol, "<") == 0) {
                current_command->redir_in_path = strdup(new);
                if (current_command->redir_in_path == NULL) {
                    free_command(command);
                    free(new);
                    free(var_token);
                    free(processed);
                    perror("strdup");
                    return (Command *)-1;
                }
                current_command->stdin_fd = open(new, O_RDONLY);
            }
            else {
                current_command->redir_out_path = strdup(new);
                if (current_command->redir_out_path == NULL) {
                    free_command(command);
                    free(new);
                    free(var_token);
                    free(processed);
                    perror("strdup");
                    return (Command *)-1;
                }
                current_command->redir_append = (strcmp(symbol, ">>") == 0);

                int flags = O_WRONLY | O_CREAT;
                flags |= current_command->redir_append ? O_APPEND : O_TRUNC;

                current_command->stdout_fd = open(new, flags, 0644);
            }
            free(new);
        }
        // If the token is a pipe symbol
        else if (strcmp(token, "|") == 0) {
            current_command->next = malloc(sizeof(Command));
            current_command = current_command->next;
            if (!current_command) {
                free_command(command);
                free(var_token);
                free(processed);
                perror("malloc");
                return (Command *)-1;
            }

            // Initialize the next command
            current_command->exec_path = NULL;
            current_command->args = malloc(sizeof(char*));
            current_command->args[0] = NULL;
            current_command->next = NULL;
            current_command->stdin_fd = 0;
            current_command->stdout_fd = 1;
            current_command->redir_in_path = NULL;
            current_command->redir_out_path = NULL;
            current_command->redir_append = 0;
        }
        // If the token is a command or argument
        else {
            new = replace_variables_mk_line(token, *variables);
            if (new == (char *)-1 || new == NULL) {
                free_command(command);
                free(var_token);
                free(processed);
                ERR_PRINT(ERR_EXECUTE_LINE);
                return (Command *)-1;
            }

            // If this is the first argument, it's the command
            if (current_command->args[0] == NULL) {
                current_command->exec_path = resolve_executable(new, *variables);
            }

            // Count the current number of arguments
            int arg_count;
            for (arg_count = 0; current_command->args[arg_count] != NULL; arg_count++);

            // Resize the args array
            current_command->args = realloc(current_command->args, (arg_count + 2) * sizeof(char *));
            if (!current_command->args) {
                free_command(command);
                free(new);
                free(var_token);
                free(processed);
                perror("realloc");
                return (Command *)-1;
            }

            // Add the new argument
            current_command->args[arg_count] = strdup(new);
            if (current_command->args[arg_count] == NULL) {
                free_command(command);
                free(new);
                free(var_token);
                free(processed);
                perror("strdup");
                return (Command *)-1;
            }
            current_command->args[arg_count + 1] = NULL;
            free(new);
        }
        // Get the next token
        token = strtok_r(NULL, " ", &saveptr);
    }
    
    if (command->exec_path == NULL) {
        free_command(command);
        command = NULL;
    }

    free(var_token);
    free(processed);

    return command;
}

/*
** This function is partially implemented for you, but you may
** scrap the implementation as long as it produces the same result.
**
** Creates a new line on the heap with all named variable *usages*
** replaced with their associated values.
**
** Returns NULL if replacement parsing had an error, or (char *) -1 if
** system calls fail and the shell needs to exit.
*/
char *replace_variables_mk_line(const char *line, Variable *variables) {
    size_t new_line_length = strlen(line) + 1;
    char *new_line = (char *)malloc(new_line_length);
    if (new_line == NULL) {
        perror("malloc");
        return (char *) -1;
    }

    const char *line_ptr = line;
    char *new_line_ptr = new_line;
    while (*line_ptr != '\0') {
        if (*line_ptr == '$') {
            const char *start_var = line_ptr + 1; // Skip '$'
            const char *end_var = start_var;
            if (*start_var == '{') {
                end_var++; // Skip '{'
                while (*end_var && *end_var != '}') end_var++;
                if (*end_var == '}') { // Found closing brace
                    end_var++; // Include '}'
                }
            } else {
                while (isalnum(*end_var) || *end_var == '_') end_var++;
            }

            char var_name[256] = {0};
            if (*(start_var) == '{') {
                strncpy(var_name, start_var + 1, end_var - start_var - 2); // Skip '${' and '}'
            } else {
                strncpy(var_name, start_var, end_var - start_var);
            }

            Variable *current = variables;
            int found = 0;
            while (current != NULL) {
                if (strcmp(current->name, var_name) == 0) {
                    size_t value_len = strlen(current->value);
                    memcpy(new_line_ptr, current->value, value_len);
                    new_line_ptr += value_len;
                    found = 1;
                    break;
                }
                current = current->next;
            }

            if (!found) {
                ERR_PRINT(ERR_VAR_NOT_FOUND, var_name);
                free(new_line);
                return NULL;
            }

            line_ptr = end_var; // Move past the variable
        } else {
            *new_line_ptr++ = *line_ptr++; // Copy characters outside variables
        }
    }

    *new_line_ptr = '\0'; // Ensure null-terminated string
    return new_line;
}

/*
** Implement the following function that frees variable(s).
**
** If recursive is non-zero, recursively free an entire
** list starting at var, else just var.
*/
void free_variable(Variable *var, uint8_t recursive) {
    if (var == NULL) {
        return;
    }

    // Free the name and value
    if (var->name != NULL) {
        free(var->name);
    }
    if (var->value != NULL) {
        free(var->value);
    }

    // If recursive is non-zero, free the next variable in the list
    if (recursive && var->next != NULL) {
        free_variable(var->next, recursive);
    }

    // Finally, free the variable itself
    free(var);
}