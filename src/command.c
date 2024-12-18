#include "command.h"
#include "uart.h"
#include "gear.h"
#include "memory.h"
#include "string_utils.h"
#include <stdbool.h>

#define BUFFER_SIZE 128

/* Global pointer to command history gear */
gear_t *command_history_gear = NULL;

/* constant for invert ansi */
#define ANSI_INVERT "\x1B[7m"
#define ANSI_BOLD "\x1B[1m"
#define ANSI_HIDE_CURSOR "\x1B[?25l"
#define ANSI_SHOW_CURSOR "\x1B[?25h"
#define ANSI_CLEAR_LINE "\x1B[2K"
#define ANSI_RESET "\x1B[0m"

/* UART input field, newline delimited, history navigation and editing */
void uart_simple_input(char *buf, int size)
{
    int buffer_length = 0;  // Actual length of the buffer
    int cursor_pos = 0;     // Cursor position within the buffer

    int is_history = 0;     // Flag to indicate history navigation mode
    int history_index = -1; // Initialize history index to -1 (no selection)

    buf[0] = '\0';          // Initialize the buffer

    // Hide terminal cursor
    uart_puts(ANSI_HIDE_CURSOR);

    // Initial rendering of the prompt and cursor
    uart_putc('\r');
    uart_puts(ANSI_CLEAR_LINE);
    uart_puts("> ");
    uart_puts(ANSI_INVERT " " ANSI_RESET);  // Display the inverted space as cursor

    while (1) {
        char c = uart_getc();

        if (c == '\r' || c == '\n') {
            buf[buffer_length] = '\0';

            // Re-render the line without inversion before moving to new line
            uart_putc('\r');
            uart_puts(ANSI_CLEAR_LINE);
            uart_puts("> ");
            uart_puts(buf);

            // Move to new line
            uart_putc('\r');
            uart_putc('\n');
            break;
        } else if (c == 3) {  // Detect Ctrl+C (ASCII code 3)
            // Clear the buffer
            buf[0] = '\0';

            // Re-render the line without inversion before exiting
            uart_putc('\r');
            uart_puts(ANSI_CLEAR_LINE);

            // Move to new line
            uart_putc('\r');
            uart_putc('\n');

            // Show terminal cursor and exit the function
            uart_puts(ANSI_SHOW_CURSOR);
            return;
        } else if (c == 27) {
            // Escape sequence for arrow keys
            c = uart_getc();
            if (c == '[') {
                c = uart_getc();
                if (c == 'C') {
                    // Right arrow key
                    if (cursor_pos < buffer_length) {
                        cursor_pos++;
                    }
                } else if (c == 'D') {
                    // Left arrow key
                    if (cursor_pos > 0) {
                        cursor_pos--;
                    }
                } else if (c == 'A') {
                    // Up arrow key (Previous command)
                    if (command_history_gear != NULL) {
                        axel_t *history_axel = (axel_t *)command_history_gear->data;
                        unsigned int history_length = history_axel->count;

                        if (is_history == 0) {
                            is_history = 1;
                            history_index = history_length; // Start from beyond the last index
                        }

                        // Move to the previous valid command
                        while (history_index > 0) {
                            history_index--;
                            gear_t *command_gear = history_axel->gears[history_index];
                            if (command_gear != NULL) {
                                char *command_data = (char *)command_gear->data;
                                // Skip empty entries (data is a single null byte)
                                if (command_data[0] != '\0' || command_gear->size > 1) {
                                    // Copy the history item to buffer
                                    buffer_length = strlen(command_data);
                                    cursor_pos = buffer_length;
                                    memcpy(buf, command_data, buffer_length);
                                    buf[buffer_length] = '\0';
                                    break;
                                }
                            }
                        }
                    }
                } else if (c == 'B') {
                    // Down arrow key (Next command)
                    if (command_history_gear != NULL && is_history == 1) {
                        axel_t *history_axel = (axel_t *)command_history_gear->data;
                        unsigned int history_length = history_axel->count;

                        // Move to the next valid command
                        while (history_index < history_length - 1) {
                            history_index++;
                            gear_t *command_gear = history_axel->gears[history_index];
                            if (command_gear != NULL) {
                                char *command_data = (char *)command_gear->data;
                                // Skip empty entries (data is a single null byte)
                                if (command_data[0] != '\0' || command_gear->size > 1) {
                                    // Copy the history item to buffer
                                    buffer_length = strlen(command_data);
                                    cursor_pos = buffer_length;
                                    memcpy(buf, command_data, buffer_length);
                                    buf[buffer_length] = '\0';
                                    break;
                                }
                            }
                        }

                        // If we've moved past the most recent command, exit history mode
                        if (history_index >= history_length - 1) {
                            is_history = 0;
                            history_index = -1;
                            // Clear the buffer
                            buf[0] = '\0';
                            buffer_length = 0;
                            cursor_pos = 0;
                        }
                    }
                }
            }
        } else if ((c == '\b') || (c == 127)) {
            // Backspace key
            if (cursor_pos > 0 && buffer_length > 0) {
                // Move everything after cursor_pos back one position
                for (int i = cursor_pos - 1; i < buffer_length - 1; i++) {
                    buf[i] = buf[i + 1];
                }
                cursor_pos--;
                buffer_length--;
                buf[buffer_length] = '\0';
            }
        } else if (c >= 32 && c <= 126) {

            if (is_history == 1) {
                // Exit history navigation
                is_history = 0;
                history_index = -1;
            }

            // Printable characters
            if (buffer_length < size - 1) {
                // Move characters to make room for the new character
                for (int i = buffer_length; i > cursor_pos; i--) {
                    buf[i] = buf[i - 1];
                }
                buf[cursor_pos] = c;
                cursor_pos++;
                buffer_length++;
                buf[buffer_length] = '\0';
            }
        }

        // Re-render the line with inverted character at cursor position
        // Move cursor to the beginning of the line
        uart_putc('\r');
        // Clear the line
        uart_puts(ANSI_CLEAR_LINE);

        // Output prompt symbol
        uart_puts("> ");

        // Output the buffer with inversion at cursor position
        for (int i = 0; i < buffer_length; i++) {
            if (i == cursor_pos) {
                // Start inversion
                uart_puts(ANSI_INVERT);
                uart_putc(buf[i]);
                // End inversion
                uart_puts(ANSI_RESET);
            } else {
                uart_putc(buf[i]);
            }
        }

        // If cursor is at the end, invert a space character
        if (cursor_pos == buffer_length) {
            uart_puts(ANSI_INVERT " " ANSI_RESET);
        }

        // No need to move the cursor since we render the inversion
    }

    // Show terminal cursor
    uart_puts(ANSI_SHOW_CURSOR);
}

int parse_hexadecimal(const char *buf)
{
    int num = 0;
    while (*buf) {
        num *= 16;
        if (*buf >= '0' && *buf <= '9') {
            num += *buf - '0';
        } else if (*buf >= 'a' && *buf <= 'f') {
            num += *buf - 'a' + 10;
        } else if (*buf >= 'A' && *buf <= 'F') {
            num += *buf - 'A' + 10;
        } else {
            /* Invalid character */
            return -1;
        }
        buf++;
    }
    return num;
}

int parse_octal(const char *buf)
{
    int num = 0;
    while (*buf) {
        if (*buf >= '0' && *buf <= '7') {
            num = num * 8 + (*buf - '0');
        } else {
            /* Invalid character */
            return -1;
        }
        buf++;
    }
    return num;
}

int parse_decimal(const char *buf)
{
    int num = 0;
    while (*buf) {
        if (*buf >= '0' && *buf <= '9') {
            num = num * 10 + (*buf - '0');
        } else {
            /* Invalid character */
            return -1;
        }
        buf++;
    }
    return num;
}

int parse_number(const char *buf)
{
    if (buf[0] == '0' && (buf[1] == 'x' || buf[1] == 'X')) {
        return parse_hexadecimal(buf + 2);
    } else if (buf[0] == '0') {
        return parse_octal(buf + 1);
    } else {
        return parse_decimal(buf);
    }
}

void display_command_history(void)
{
    if (command_history_gear == NULL) {
        uart_puts("No command history available\n");
        return;
    }

    axel_t *history_axel = (axel_t *)command_history_gear->data;
    for (unsigned int i = 0; i < history_axel->count; i++) {
        gear_t *command_gear = history_axel->gears[i];
        uart_puts("  ");
        uart_puts((const char *)command_gear->data);
        uart_puts("\n");
    }
}

void init_command_history(axel_t *root)
{
    /* Allocate a gear to hold the command history axel */
    command_history_gear = galloc(sizeof(axel_t));
    if (command_history_gear == NULL) {
        uart_puts("Failed to allocate command history gear\n");
        return;
    }

    /* Initialize the command history gear */
    command_history_gear->type = GEAR_TYPE_AXEL;
    command_history_gear->permissions = 0; /* Set permissions as needed */
    command_history_gear->size = sizeof(axel_t);

    /* Initialize the axel within the gear */
    axel_t *history_axel = (axel_t *)command_history_gear->data;
    history_axel->size = 1024; /* Capacity for 1024 commands */
    history_axel->count = 0;

    /* Adjust mem_free to account for the gears array in the axel */
    unsigned int history_axel_size = sizeof(axel_t) + (history_axel->size * sizeof(gear_t *));
    mem_free = (uintptr_t)command_history_gear->data + history_axel_size;
}

void add_command_to_history(const char *command)
{
    /* Ensure command_history_gear is initialized */
    if (command_history_gear == NULL) {
        uart_puts("Command history gear not initialized\n");
        return;
    }

    /* Get the history axel from the gear */
    axel_t *history_axel = (axel_t *)command_history_gear->data;

    /* Check if there's space in the command history */
    if (history_axel->count >= history_axel->size) {
        uart_puts("Command history is full\n");
        return;
    }

    /* Allocate a gear to store the command string */
    unsigned int command_length = strlen(command) + 1; /* Include null terminator */
    gear_t *command_gear = galloc(command_length);
    if (command_gear == NULL) {
        uart_puts("Failed to allocate command gear\n");
        return;
    }

    /* Initialize the command gear */
    command_gear->type = GEAR_TYPE_GEAR;
    command_gear->permissions = 0; /* Set permissions as needed */
    command_gear->size = command_length;

    /* Copy the command into the gear's data */
    memcpy(command_gear->data, command, command_length);

    /* Store the command gear in the history axel */
    history_axel->gears[history_axel->count] = command_gear;
    history_axel->count += 1;
}

void parse_command(char *buf)
{
    char *command;
    char *arg1 = NULL;
    char *arg2 = NULL;

    /* Trim leading whitespace */
    while (*buf == ' ' || *buf == '\t')
        buf++;

    /* Empty input */
    if (*buf == '\0')
        return;

    /* Extract command */
    command = buf;

    /* Find first whitespace to separate command */
    while (*buf && *buf != ' ' && *buf != '\t')
        buf++;

    if (*buf) {
        *buf = '\0'; /* Null-terminate the command */
        buf++;
        /* Skip whitespace before arguments */
        while (*buf == ' ' || *buf == '\t')
            buf++;
        arg1 = buf;
        /* Find separator between arg1 and arg2 (for commands with two arguments) */
        char *comma = NULL;
        while (*buf && *buf != ' ' && *buf != '\t' && *buf != ',')
            buf++;
        if (*buf == ',') {
            comma = buf;
            *buf = '\0'; /* Null-terminate arg1 */
            buf++;
            /* Skip whitespace before arg2 */
            while (*buf == ' ' || *buf == '\t')
                buf++;
            arg2 = buf;
            /* Null-terminate arg2 at the end */
            while (*buf && *buf != ' ' && *buf != '\t')
                buf++;
            *buf = '\0';
        } else if (*buf) {
            *buf = '\0'; /* Null-terminate arg1 */
        }
    }

    /* Process command */
    if (strcmp(command, "echo") == 0) {
        if (arg1) {
            uart_puts(arg1);
            uart_puts("\n");
        } else {
            uart_puts("Usage: echo <string>\n");
        }
    } else if (strcmp(command, "galloc") == 0) {
        if (arg1) {
            int size = parse_number(arg1);
            if (size > 0) {
                gear_t *gear = galloc(size);
                if (gear != NULL) {
                    uart_puts("Allocated gear at address 0x");
                    char addr_str[17];
                    int_to_string_upper(addr_str, (uintptr_t)gear, 16);
                    uart_puts(addr_str);
                    uart_puts("\n");
                } else {
                    uart_puts("Allocation failed\n");
                }
            } else {
                uart_puts("Invalid size\n");
            }
        } else {
            uart_puts("Usage: galloc <size>\n");
        }
    } else if (strcmp(command, "gears") == 0) {
        /* Print all gears */
        axel_t *root = (axel_t *)MEM_START;
        for (unsigned int i = 0; i < root->count; i++) {
            gear_t *gear = root->gears[i];
            uart_puts("Gear ");
            char id_str[9];
            int_to_string(id_str, gear->id, 10);
            uart_puts(id_str);
            uart_puts(" at address 0x");
            char addr_str[17];
            int_to_string_upper(addr_str, (uintptr_t)gear, 16);
            uart_puts(addr_str);
            uart_puts(", size ");
            char size_str[9];
            int_to_string(size_str, gear->size, 10);
            uart_puts(size_str);
            uart_puts(" bytes\n");
        }
    } else if (strcmp(command, "help") == 0) {
        uart_puts("Available commands:\n");
        uart_puts("  echo <string>         - Print <string>\n");
        uart_puts("  galloc <size>         - Allocate a gear of <size> bytes\n");
        uart_puts("  gears                 - List all allocated gears\n");
        uart_puts("  history               - Show command history\n");
        uart_puts("  help                  - Show this help message\n");
    } else if (strcmp(command, "history") == 0) {
        display_command_history();
    } else {
        uart_puts("Unknown command. Type 'help' for a list of commands.\n");
    }
}

void repl(void)
{
    char buf[BUFFER_SIZE];
    while (1) {
        uart_simple_input(buf, sizeof(buf));
        add_command_to_history(buf);
        parse_command(buf);
    }
}
