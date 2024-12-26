# shells
Just messing with C to build my own shell


# Simple C Shell

A lightweight shell in C, based off Stephen Brennan's LSH.  
It supports history, tab completion, and several built-in commands.

## Features

- Command history (with up/down arrow navigation)  
- Built-in commands:  
  - cd, help, exit, ls, pwd, clear, history, cat, grep, touch, echo, rm  
- Tab completion for built-in commands and files  
- History stored in a file “.shell_history”  
- Simple raw mode editor for command input  

## Requirements

- GCC (or another C compiler)  
- Linux or macOS environment (though Windows with appropriate libraries can work)  

## Build & Run

1. Compile:  
   ```bash
   gcc -Wall -o my_shell main.c

2. Run:
    ```bash
    ./my_shell


## Usage

Enter commands at the prompt. Press up/down arrows to navigate history.  
The built-in commands are listed by typing:
    ```bash
    help


Tab completion attempts to complete your current typed text from built-ins or filenames.

## Project Structure

- **main.c**: Contains all functionality (history, built-ins, command parsing).
- **.shell_history**: Stores command history across sessions.

## Extending

1. Add a prototype and function definition.  
2. Insert function name in the `builtin_str` and `builtin_func` arrays.

## License

Feel free to use or modify. No specific license is declared.