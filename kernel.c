/*
 * kernel.c - Full Featured Kernel (Bonuses Implemented)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "io.h"
#include "keyboard.h"

/* --- System Constants --- */
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static const size_t HISTORY_LINES = 100;
uint16_t* vga_buffer = (uint16_t*) 0xB8000;

/* --- State Management --- */
typedef struct {
	size_t row;       // Logical cursor row (0 .. HISTORY_LINES-1)
	size_t column;
    size_t view_row;  // Top visible row (0 .. HISTORY_LINES - VGA_HEIGHT)
	uint8_t color;
	uint16_t buffer[80 * 100]; // History buffer
} ScreenState;

ScreenState screens[3]; // We support 3 screens (F1, F2, F3)
int current_screen = 0;

/* Global current state (matches the visible hardware) */
size_t terminal_row;
size_t terminal_column;
size_t terminal_view_row;
uint8_t terminal_color;

/* Protection boundaries */
size_t input_start_row = 0;
size_t input_start_col = 0;

/* --- Hardware Colors --- */
enum vga_color {
	VGA_COLOR_BLACK = 0, VGA_COLOR_BLUE = 1, VGA_COLOR_GREEN = 2, VGA_COLOR_CYAN = 3,
	VGA_COLOR_RED = 4, VGA_COLOR_MAGENTA = 5, VGA_COLOR_BROWN = 6, VGA_COLOR_LIGHT_GREY = 7,
	VGA_COLOR_DARK_GREY = 8, VGA_COLOR_LIGHT_BLUE = 9, VGA_COLOR_LIGHT_GREEN = 10,
	VGA_COLOR_LIGHT_CYAN = 11, VGA_COLOR_LIGHT_RED = 12, VGA_COLOR_LIGHT_MAGENTA = 13,
	VGA_COLOR_YELLOW = 14, VGA_COLOR_WHITE = 15,
};

static inline uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
	return fg | bg << 4;
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
	return (uint16_t) uc | (uint16_t) color << 8;
}

/* --- Helper: Memory Move (since we have no libc) --- */
void* memmove(void* dstptr, const void* srcptr, size_t size) {
	unsigned char* dst = (unsigned char*) dstptr;
	const unsigned char* src = (const unsigned char*) srcptr;
	if (dst < src) {
		for (size_t i = 0; i < size; i++)
			dst[i] = src[i];
	} else {
		for (size_t i = size; i != 0; i--)
			dst[i-1] = src[i-1];
	}
	return dstptr;
}

size_t strlen(const char* str) {
	size_t len = 0;
	while (str[len]) len++;
	return len;
}

/* --- Hardware Cursor --- */
/* --- Hardware Cursor --- */
/* Update cursor to physical position on screen. Hide if off-screen. */
void update_cursor(int x, int y) {
    /* Calculate physical row relative to viewport */
    int physical_row = y - terminal_view_row;
    
    if (physical_row >= 0 && physical_row < (int)VGA_HEIGHT) {
        uint16_t pos = physical_row * VGA_WIDTH + x;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t) (pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
    } else {
        /* Move cursor off-screen (e.g. 26, 0) if it's not in view */
        /* standard trick: set to 2000 (after 80*25) */
        uint16_t pos = 2000;
        outb(0x3D4, 0x0F);
        outb(0x3D5, (uint8_t) (pos & 0xFF));
        outb(0x3D4, 0x0E);
        outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));
    }
}

/* --- Terminal Logic --- */
/* --- Terminal Logic --- */
/* Copy current viewport from history buffer to VGA memory */
void refresh_screen() {
    uint16_t* history = screens[current_screen].buffer;
    /* Determine start offset in history buffer */
    size_t start_offset = terminal_view_row * VGA_WIDTH;
    
    /* Copy VGA_HEIGHT lines to VGA buffer */
    /* Note: If history is not full or view_row is large, ensure we don't read past end? 
       HISTORY_LINES is fixed, view_row bounded. */
    memmove(vga_buffer, &history[start_offset], VGA_WIDTH * VGA_HEIGHT * 2);
    
    /* Also update cursor since view changed */
    update_cursor(terminal_column, terminal_row);
}

void terminal_initialize(void) {
	/* init screens */
	for(int i=0; i<3; i++) {
		screens[i].row = 0;
		screens[i].column = 0;
        screens[i].view_row = 0;
		screens[i].color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
		for (size_t y = 0; y < HISTORY_LINES; y++) {
			for (size_t x = 0; x < VGA_WIDTH; x++) {
				screens[i].buffer[y * VGA_WIDTH + x] = vga_entry(' ', screens[i].color);
			}
		}
	}
	/* load screen 0 */
	terminal_row = 0;
	terminal_column = 0;
    terminal_view_row = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	/* copy blank state to vga memory */
	refresh_screen();
}

/* Updated scroll logic: 
   1. If we are just moving down but within history limits, scroll the VIEWPORT.
   2. If we hit the absolute end of history buffer, shift the BUFFER. */
void terminal_scroll() {
    uint16_t* history = screens[current_screen].buffer;
    
    if (terminal_row >= HISTORY_LINES) {
        /* We hit the hard limit of our history buffer.
           Shift everything up by 1 line (discarding top line of history) */
           
        for (size_t y = 0; y < HISTORY_LINES - 1; y++) {
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                history[y * VGA_WIDTH + x] = history[(y + 1) * VGA_WIDTH + x];
            }
        }
        /* Clear last line */
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            history[(HISTORY_LINES - 1) * VGA_WIDTH + x] = vga_entry(' ', terminal_color);
        }
        
        terminal_row = HISTORY_LINES - 1;
        
        /* Protection boundary must slide back too, since the world moved up */
        if (input_start_row > 0) input_start_row--;
    }
    
    /* Ensure view follows cursor if it went off the bottom */
    if (terminal_row >= terminal_view_row + VGA_HEIGHT) {
        terminal_view_row = terminal_row - VGA_HEIGHT + 1;
    }
    
    refresh_screen();
}



void set_input_boundary() {
    input_start_row = terminal_row;
    input_start_col = terminal_column;
}

void terminal_putchar(char c) {
    uint16_t* history = screens[current_screen].buffer;
    
	if (c == '\n') {
		terminal_row++;
		terminal_column = 0;
	} else if (c == '\b') {
	} else if (c == '\b') {
        /* Check Protection */
        if (terminal_row < input_start_row || (terminal_row == input_start_row && terminal_column <= input_start_col)) {
            return;
        }

        if (terminal_column > 0) {
             /* Handle "Full Line" Edge Case: 
                If we are at column 79 and there is a character there (e.g. we just wrapped back to a full line),
                backspace should delete THIS character first, rather than moving left. */
             if (terminal_column == VGA_WIDTH - 1) {
                 uint16_t entry = history[terminal_row * VGA_WIDTH + terminal_column];
                 if ((entry & 0xFF) != ' ') {
                     history[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(' ', terminal_color);
                     refresh_screen();
                     return;
                 }
             }

            terminal_column--;
            
            /* Ripple Delete (in history buffer) */
            size_t start_pos = terminal_row * VGA_WIDTH + terminal_column;
            
            /* Protect Heartbeat: If on row 0 (logical), don't pull index 79 into 78 */
            size_t max_col = (terminal_row == 0) ? (VGA_WIDTH - 2) : (VGA_WIDTH - 1);
            size_t end_of_line = terminal_row * VGA_WIDTH + max_col;
            
            for (size_t i = start_pos; i < end_of_line; i++) {
                history[i] = history[i+1];
            }
            history[end_of_line] = vga_entry(' ', terminal_color);
            
        } else if (terminal_row > 0) {
            /* Backspace Wrap */
            /* Scan previous line for end of text to avoid jumping to empty void */
            size_t prev_row = terminal_row - 1;
            int found_col = -1;
            for (int x = VGA_WIDTH - 1; x >= 0; x--) {
                uint16_t entry = history[prev_row * VGA_WIDTH + x];
                if ((entry & 0xFF) != ' ') {
                    found_col = x;
                    break;
                }
            }

            terminal_row--;
            
            if (found_col == -1) {
                terminal_column = 0;
            } else {
                terminal_column = found_col + 1;
                 /* If the previous line is full (found at 79), we land AT 79.
                    Standard logic would force us to 80 -> 79.
                    Logic: if found at 79, we land on 79. 
                    Unlike before, we do NOT delete the char at 79 immediately.
                    Users can press backspace again to trigger the "Full Line" logic above. */
                if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1;
            }
            // Non-destructive wrap: Do not overwrite with space yet.
        }
        
        /* Auto-scroll up if we backspaced out of view */
        if (terminal_row < terminal_view_row) {
             terminal_view_row = terminal_row;
        }
        
    } else {
		history[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(c, terminal_color);
		terminal_column++;
	}

	if (terminal_column >= VGA_WIDTH) {
		terminal_column = 0;
		terminal_row++;
	}
    
    /* Handle scrolling / viewing logic */
    if (terminal_row >= HISTORY_LINES) {
		terminal_scroll(); // Hard shift
	} else if (terminal_row >= terminal_view_row + VGA_HEIGHT) {
        terminal_scroll(); // View shift
    }

	refresh_screen();
}

void terminal_write(const char* data, size_t size) {
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
	terminal_write(data, strlen(data));
}

/* --- Printf Implementation --- */
/* A minimal implementation of printf supporting %s, %d, %x, %c */
void printk(const char* format, ...) {
	va_list args;
	va_start(args, format);

	for (const char* p = format; *p != '\0'; p++) {
		if (*p != '%') {
			terminal_putchar(*p);
			continue;
		}
		p++; // Skip '%'
		switch (*p) {
			case 'c': {
				char c = (char) va_arg(args, int);
				terminal_putchar(c);
				break;
			}
			case 's': {
				const char* s = va_arg(args, const char*);
				terminal_writestring(s);
				break;
			}
			case 'd': {
				int d = va_arg(args, int);
				if (d < 0) {
					terminal_putchar('-');
					d = -d;
				}
				char buf[16];
				int i = 0;
				if (d == 0) buf[i++] = '0';
				while (d > 0) {
					buf[i++] = (d % 10) + '0';
					d /= 10;
				}
				while (i > 0) terminal_putchar(buf[--i]);
				break;
			}
			case 'x': {
				unsigned int x = va_arg(args, unsigned int);
				char buf[16];
				int i = 0;
				if (x == 0) buf[i++] = '0';
				while (x > 0) {
					int digit = x % 16;
					buf[i++] = (digit < 10) ? (digit + '0') : (digit - 10 + 'a');
					x /= 16;
				}
				while (i > 0) terminal_putchar(buf[--i]);
				break;
			}
		}
	}
	va_end(args);
}

/* --- Screen Switching --- */
/* --- Screen Switching --- */
void switch_screen(int screen_index) {
	if (screen_index == current_screen) return;
	
	/* Save current screen state */
	screens[current_screen].row = terminal_row;
	screens[current_screen].column = terminal_column;
    screens[current_screen].view_row = terminal_view_row;
	screens[current_screen].color = terminal_color;
    /* Buffer is already up to date since we write to it directly */

	/* Switch index */
	current_screen = screen_index;

	/* Restore new screen state */
	terminal_row = screens[current_screen].row;
	terminal_column = screens[current_screen].column;
    terminal_view_row = screens[current_screen].view_row;
	terminal_color = screens[current_screen].color;
	
	refresh_screen();
}

/* --- Keyboard Handling --- */
void keyboard_handler() {
    /* Read status from keyboard controller */
    uint8_t status = inb(0x64);
    
    /* If status bit 0 is set, data is available */
    if (status & 0x01) {
        uint8_t scancode = inb(0x60);
        
        /* Check if key released (highest bit set) - ignore for now */
        if (scancode & 0x80) {
            // Key release logic
        } else {
			/* Key Pressed */
			
            /* F1, F2, F3 for screen switching */
            if (scancode == 0x3B) { switch_screen(0); return; }
            if (scancode == 0x3C) { switch_screen(1); return; }
            if (scancode == 0x3D) { switch_screen(2); return; }

            /* Arrow Keys (Extended 0xE0 scancodes usually, but simpler set 1 check) */
            /* Up: 0x48, Left: 0x4B, Right: 0x4D, Down: 0x50 */
            if (scancode == 0x4B) { // Left
                /* Constraint: Don't move left into protected area */
                if (terminal_row == input_start_row && terminal_column <= input_start_col) return;
                
                if (terminal_column > 0) terminal_column--;
                refresh_screen(); // Calls update_cursor
                return;
            }
            if (scancode == 0x4D) { // Right
                /* Constraint: Don't move right into empty void */
                /* Check if there is a character at the current position */
                uint16_t* history = screens[current_screen].buffer;
                uint16_t entry = history[terminal_row * VGA_WIDTH + terminal_column];
                if ((entry & 0xFF) == ' ') return; // End of text on this line?
                
                if (terminal_column < VGA_WIDTH - 1) terminal_column++;
                refresh_screen(); // Calls update_cursor
                return;
            }
            if (scancode == 0x48) { // Up
                /* Constraint: Don't go up into read-only history */
                if (terminal_row <= input_start_row) return;

                if (terminal_row > 0) terminal_row--;
                
                /* Clamp column to end of text on new line */
                /* (logic omitted for simplicity, just letting it float is standard for basic terminals) */
                if (terminal_row > 0) terminal_row--;
                
                /* Auto-scroll up */
                if (terminal_row < terminal_view_row) terminal_view_row = terminal_row;

                /* Clamp column to end of text on new line */
                /* (logic omitted for simplicity, just letting it float is standard for basic terminals) */
                refresh_screen();
                return;
            }
            if (scancode == 0x50) { // Down
                /* Constraint: Don't go down into empty void */
                 uint16_t* history = screens[current_screen].buffer;
                 uint16_t entry = history[(terminal_row + 1) * VGA_WIDTH + 0];
                 if ((entry & 0xFF) == ' ' && terminal_row >= input_start_row) return; /* Next line is empty */

                if (terminal_row < HISTORY_LINES - 1) terminal_row++;
                
                /* Auto-scroll down */
                if (terminal_row >= terminal_view_row + VGA_HEIGHT) terminal_view_row++;

                refresh_screen();
                return;
            }
            if (scancode == 0x49) { // Page Up
                if (terminal_view_row > 0) {
                    terminal_view_row--;
                    refresh_screen();
                }
                return;
            }
            if (scancode == 0x51) { // Page Down
                /* Don't scroll past the logical bottom of the filled buffer? 
                   Actually, allow scrolling down to where cursor is, basically.
                   Limit: terminal_view_row + VGA_HEIGHT < HISTORY_LINES */
                if (terminal_view_row + VGA_HEIGHT < HISTORY_LINES) {
                     terminal_view_row++;
                     refresh_screen();
                }
                return;
            }

            /* Normal Typing */
            if (scancode < 128 && kbdus[scancode]) {
                terminal_putchar(kbdus[scancode]);
            }
        }
    }
}

/* --- Main --- */
void kernel_main(void) {
	/* Initialize terminal interface */
	terminal_initialize();

	printk("KFS-1 with Bonus 42\n");
	printk("--------------------------------\n");
	printk("Features: %s, %s, %s\n", "Scroll", "Colors", "Printf");
	printk("Press F1/F2/F3 to switch screens.\n");
	printk("Arrow Keys to move, Backspace to delete.\n");
	printk("Type something:");
    
    /* Set the boundary so user cannot delete the text above */
    void set_input_boundary();
    set_input_boundary();

    /* Disable Interrupts: We are polling, and we have no IDT yet.
     * If we enable interrupts (sti), the CPU will crash on the first timer tick! */
    asm volatile("cli");

	/* Heartbeat counter to prove kernel is running */
    unsigned char spinner[] = {'|', '/', '-', '\\'};
    int spin_idx = 0;
    int tick = 0;

    /* Flush keyboard buffer before starting */
    while(inb(0x64) & 0x1) inb(0x60);

	while(1) {
        keyboard_handler();
        
        /* Update heartbeat every ~10000 loops */
        /* direct buffer access used */
        tick++;
        if (tick % 10000 == 0) {
            /* We need to update the history buffer AND the VGA buffer if visible */
            size_t heartbeat_idx = 0 * VGA_WIDTH + 79;
            uint16_t val = vga_entry(spinner[spin_idx], vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            
            screens[current_screen].buffer[heartbeat_idx] = val;
            
            /* If row 0 is visible, update VGA directly */
            if (screens[current_screen].view_row == 0) {
                 vga_buffer[heartbeat_idx] = val;
            }
            
            spin_idx = (spin_idx + 1) % 4;
        }
	}
}
