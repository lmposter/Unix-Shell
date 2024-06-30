# Introducing CUS: A Custom Terminal-Based Shell

## Overview

Custom Unix Shell (CUS) is a robust, terminal-based shell designed to execute a range of basic and advanced operations, similar to POSIX sh and BASH. Created as an educational project, CUS provides hands-on experience with system calls, process management, and C-string parsing, making it an invaluable tool for both learning and practical use.

## Key Features

**Interactive and Scriptable:** CUS operates both interactively and non-interactively, allowing users to execute commands directly from the terminal or from script files.

**Command Execution:** Capable of launching executables with appropriate permissions from directories listed in the $PATH variable, as well as those specified with absolute or relative paths. Users can also supply command line arguments to these programs.

**Variable Management:** Supports creation and usage of shell variables, following a strict syntax to ensure correct assignment and utilization within commands.

**File Redirection:** Implements redirection of input and output streams, allowing users to redirect stdin and stdout to and from files using `>`, `>>`, and `<`.

**Piping:** Enables the connection of the stdout of one command to the stdin of another, facilitating the creation of complex command chains.

**Special Commands:** Includes built-in support for the `cd` command to change directories and handle both relative and absolute paths.

**Error Handling:** Gracefully manages errors related to command execution and variable assignment, providing clear error messages without terminating the shell.

## Explore with Ease

Navigating CUS is intuitive and user-friendly. Start by launching the shell in your terminal environment, and you'll find a familiar interface ready to accept commands. Whether you need to run system programs, handle files, or execute scripts, CUS's commands are designed to deliver quick and accurate results. Its support for variable usage within commands, file redirection, and piping ensures a comprehensive shell experience.

## For Developers and Enthusiasts

CUS is not only a practical tool for end-users but also a rich resource for developers and programming enthusiasts. Its implementation provides insights into system calls, process management, and shell scripting, making it an excellent learning platform. By exploring its open design and detailed codebase, users can deepen their understanding of C programming principles and software development best practices.

## Get Started

To begin using CUS, ensure you have a terminal environment set up. Compile the application following the provided setup instructions, and you're ready to go. Load script files or type commands directly into the shell, and experience the seamless execution of commands, variable management, file redirection, and piping.
