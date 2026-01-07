/*
 * kernel.c - Full Featured Kernel (Bonuses Implemented)
 */

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include "io.h"
#include "keyboard.h"

/* --- Port mapping --- */
static const uint16_t STATUS_KEYBOARD_PORT = 0x64; // Port status : lire l'état si le bit de status est a 1 -> il y'a une entree clavier a lire sur le port 0x60
static const uint16_t DATA_KEYBOARD_PORT = 0x60; // Port data : Permet de lire la touche clavier (Presse/relache)
static const uint16_t CURSOR_INDEX = 0x3D4; // Port index : On y ecris le numero de registre inerne qu'on veut modifier
static const uint16_t CURSOR_DATA  = 0x3D5; // Port data : On ecrit la valeur qu'on veut mettre dans le registre selectionne par 0X3D4

/* --- System Constants --- */
static const size_t VGA_WIDTH = 80; // Largeur du terminal 80
static const size_t VGA_HEIGHT = 25; // Hauteur du terminal 25
static const size_t HISTORY_LINES = 100;
uint16_t* vga_buffer = (uint16_t*) 0xB8000;

/* --- State Management --- */
typedef struct {
	size_t row;       // Logical cursor row (0 .. HISTORY_LINES-1)
	size_t column;
    size_t view_row;  // Top visible row (0 .. HISTORY_LINES - VGA_HEIGHT)
	uint8_t color;
	uint16_t buffer[80 * 100]; // History buffer
    size_t input_start_row;
    size_t input_start_col;
} ScreenState;

ScreenState screens[3]; // 3 screens (F1, F2, F3)
int current_screen = 0;

/* Etat actuel */
size_t terminal_row; // Position relative du curseur ligne
size_t terminal_column; // Position relative du curseur colonne
size_t terminal_view_row; // index de ligne qui dit a partir de quelle ligne de l'historique on affiche l’ecran
uint8_t terminal_color;

/* Protection */
size_t input_start_row = 0; // ligne ou debute l'entree utilisateur avant ca read-only
size_t input_start_col = 0; // colonne ou debute l'entree utilisateur avant ca read-only

/* --- Couleur --- */
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
/* Actualise la position du curseur */
void update_cursor(int x, int y) {
    /* Calcul la position du cursor par rapport a la view actuel*/
    int physical_row = y - terminal_view_row;
    
    /* Si la ROW est entre 0 et 24 on est dans l'ecran*/
    if (physical_row >= 0 && physical_row < (int)VGA_HEIGHT) {
        /* pos du curseur * VGA_WIDTH(tableau en 1D) + x(terminal column)*/
        uint16_t pos = physical_row * VGA_WIDTH + x;
        /* On doit ecrire la position du curseur dans 0x0F pour la partie basse et 0x0E pour la partie haute car le curseur peut etre place de 0 a VGA_WIDTH * VGA_HEIGHT donc plus de 255 donc besoint de 2 octects */
        outb(CURSOR_INDEX, 0x0F);
        /*  */
        outb(CURSOR_DATA, (uint8_t) (pos & 0xFF));
        outb(CURSOR_INDEX, 0x0E);
        outb(CURSOR_DATA, (uint8_t) ((pos >> 8) & 0xFF));
    } else {
        /* Cache le curseur si il est hors screen */
        uint16_t pos = 2000;
        outb(CURSOR_INDEX, 0x0F);
        outb(CURSOR_DATA, (uint8_t) (pos & 0xFF));
        outb(CURSOR_INDEX, 0x0E);
        outb(CURSOR_DATA, (uint8_t) ((pos >> 8) & 0xFF));
    }
}

/* Copie la vue actuel depuis l'historique dans la memoire VGA */
void refresh_screen() {
    uint16_t* history = screens[current_screen].buffer;
    /* Calcul de le debut de l'affichage de l'historique (* VGA_WIDTH car historique est un tableau simple donc ca permet de "simuler" un tableau en 2D) */
    size_t start_offset = terminal_view_row * VGA_WIDTH;
    
    /* Copie l'historique du screen dans le buffer VGA (Affichage sur l'ecran) a partir de l'offset de debut et pour une taille de Hauteur * Largeur * 2
       *2 car chaque cellule = 2 octects (octect 0 = caractere, octect 1 = attribut (couleur..)) */
    memmove(vga_buffer, &history[start_offset], VGA_WIDTH * VGA_HEIGHT * 2);
    
    /* Update du curseur */
    update_cursor(terminal_column, terminal_row);
}

void terminal_initialize(void) {
	/* init screens */
	for(int i=0; i<3; i++) {
		screens[i].row = 0;
		screens[i].column = 0;
		screens[i].column = 0;
        screens[i].view_row = 0;
        screens[i].input_start_row = 0;
        screens[i].input_start_col = 0;
        
        /* Color Themes per Screen */
        if (i == 0) screens[i].color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
        else if (i == 1) screens[i].color = vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        else screens[i].color = vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK);
		for (size_t y = 0; y < HISTORY_LINES; y++) {
			for (size_t x = 0; x < VGA_WIDTH; x++) {
				screens[i].buffer[y * VGA_WIDTH + x] = vga_entry(0, screens[i].color);
			}
		}
	}
	/* load screen 0 */
	terminal_row = 0;
	terminal_column = 0;
    terminal_view_row = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);

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
            history[(HISTORY_LINES - 1) * VGA_WIDTH + x] = vga_entry(0, terminal_color);
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
    
    /* Si retour a la ligne on passe a la ligne suivante */
	if (c == '\n') {
		terminal_row++;
		terminal_column = 0;
    /* Si backspace */
	} else if (c == '\b') {
        /* Impossible d'effacer si on est dans un zone read-only */
        if (terminal_row < input_start_row || (terminal_row == input_start_row && terminal_column <= input_start_col)) {
            return;
        }

        /* Si on est pas en tout debut de ligne */
        if (terminal_column > 0) {
            /* Permet si on est en column 79 et qu'il y'a un caractere de le supprimer sans reculer le cursor. (Si on est sur le heartbeat on ne le supprime pas on passe a la suite) */
             if (terminal_column == VGA_WIDTH - 1 && !(terminal_row == 0 && terminal_column == 79)) {
                 uint16_t entry = history[terminal_row * VGA_WIDTH + terminal_column];
                 if ((entry & 0xFF) != 0) {
                     history[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(0, terminal_color);
                     refresh_screen();
                     return;
                 }
            }
            
            /* Recul le curseur */
            terminal_column--;
            
            /* Position qu'on va supprimer */
            size_t start_pos = terminal_row * VGA_WIDTH + terminal_column;
            
            /* Si on est sur le heartbeat on ne le decale pas d'ou la ternaire */
            size_t max_col = (terminal_row == 0) ? (VGA_WIDTH - 2) : (VGA_WIDTH - 1);
            size_t end_of_line = terminal_row * VGA_WIDTH + max_col;
            
            /* On decale toute la ligne a partir de start_pos lors d'une suppression*/
            for (size_t i = start_pos; i < end_of_line; i++) {
                history[i] = history[i+1];
            }
            /* C'est le dernier caractere qui sera mis a 0 */
            history[end_of_line] = vga_entry(0, terminal_color);
            
        } else if (terminal_row > 0) { // Si on peut remonter dans les lignes
            /* Si on doit remonter on cherche le premier caractere qui n'est pas un 0 et on deplace le curseur a cet endroit comme si on supprimait le /n */
            size_t prev_row = terminal_row - 1;
            int found_col = -1;
            /* On checher dans la ligne au dessus le premnier caractere*/
            for (int x = VGA_WIDTH - 1; x >= 0; x--) {
                /* Ignore Heartbeat at (0,79) - it is not "content" to jump to */
                if (prev_row == 0 && x == (int)VGA_WIDTH - 1) continue;

                uint16_t entry = history[prev_row * VGA_WIDTH + x];
                if ((entry & 0xFF) != 0) {
                    found_col = x;
                    break;
                }
            }

            /* On remonte le curseur d'une ligne */
            terminal_row--;
            /* Si pas de caractere trouve dans la colonne (2 /n d'affile) on met le cursor au debut de la colonne*/
            if (found_col == -1) {
                terminal_column = 0;
            } else {
                /* On met le cursor apres le caractere trouve */
                terminal_column = found_col + 1;
                 /* Si la ligne est pleine de caractere alors on met le curseur a la place 79 sur le caractere en question  plutot que a +1 ce qu ne serait pas possible */
                if (terminal_column >= VGA_WIDTH) terminal_column = VGA_WIDTH - 1;
            }
        }
        
        /* Scroll si en effacant  si terminal row et terminal_view_row ne sont plus alignee */
        if (terminal_row < terminal_view_row) {
             terminal_view_row = terminal_row;
        }
        
    } else {
		history[terminal_row * VGA_WIDTH + terminal_column] = vga_entry(c, terminal_color);
		terminal_column++;
	}
    /* Retour a la ligne si on a une ligne complete */
	if (terminal_column >= VGA_WIDTH) {
		terminal_column = 0;
		terminal_row++;
	}
    
    /* SI on a plus de ligne que de place dans le buffer on supprime la plus ancienne et on ajoute la nouvelle */
    if (terminal_row >= HISTORY_LINES) {
		terminal_scroll(); // Hard shift
    /* Encore de la place dans l'historique mais on depasse la vue de 25 lignes */
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

/* --- Printk --- */
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

void switch_screen(int screen_index) {
	if (screen_index == current_screen) return;
	
	/* Save les donnees du screens actuel avant de switch */
	screens[current_screen].row = terminal_row;
	screens[current_screen].column = terminal_column;
    screens[current_screen].view_row = terminal_view_row;
	screens[current_screen].color = terminal_color;
    screens[current_screen].input_start_row = input_start_row;
    screens[current_screen].input_start_col = input_start_col;

	/* Switch index */
	current_screen = screen_index;

	/* Update les global avec les data du nouveau screen */
	terminal_row = screens[current_screen].row;
	terminal_column = screens[current_screen].column;
    terminal_view_row = screens[current_screen].view_row;
	terminal_color = screens[current_screen].color;
    input_start_row = screens[current_screen].input_start_row;
    input_start_col = screens[current_screen].input_start_col;
	
	refresh_screen();
}

/* --- Keyboard Handling --- */
void keyboard_handler() {
    /* Lis le status du controller clavier */
    uint8_t status = inb(STATUS_KEYBOARD_PORT);
    
    /* Si le bit 0 est set -> Il y'a une data a lire sur le DATA_PORT */
    if (status & 0x01) {
        /* Lis la touche clavier */
        uint8_t scancode = inb(DATA_KEYBOARD_PORT);
        
        /* Le dernier bit du scancode permet de savoir si la touche est appuye ou relache 1 si elle est relachee 0 si elle est appuyee */
        if (scancode & 0x80) {
        } else {
			/* Touche pressee */
			
            /* SI F1, F2, F3 on switch d'ecran */
            if (scancode == 0x3B) { switch_screen(0); return; }
            if (scancode == 0x3C) { switch_screen(1); return; }
            if (scancode == 0x3D) { switch_screen(2); return; }

            /* Up: 0x48, Left: 0x4B, Right: 0x4D, Down: 0x50 */
            if (scancode == 0x4B) { // Left
                /* Si on est a la limite gauche de la ou on peut ecrire en terme de colonne et de ligne*/
                if (terminal_row == input_start_row && terminal_column <= input_start_col) return;
                /* Si on est pas sur la premiere colonne on peut revenir en arriere */
                if (terminal_column > 0) terminal_column--;
                refresh_screen();
                return;
            }
            if (scancode == 0x4D) { // Right
                /* On n'autorise pas le deplacement a droite si c'est un vide (zone non remplie) */
                uint16_t* history = screens[current_screen].buffer;
                uint16_t entry = history[terminal_row * VGA_WIDTH + terminal_column];
                /* Si le curseur est sur un espace alors on ne fait rien */
                if ((entry & 0xFF) == 0) return;
                
                /* Si le curseur est en dessous de 79 alors on accepte le deplacement vers la droite */
                if (terminal_column < VGA_WIDTH - 1) terminal_column++;
                /* Sinon on passe a la ligne du dessous */
                else {
                    terminal_row++;
                    terminal_column = 0;
                }
                refresh_screen();
                return;
            }
            if (scancode == 0x48) { // Up
                /* Si la colonne d'au dessus contient un vide alors on n'autorise pas le deplacement vers le bas */
                uint16_t* history = screens[current_screen].buffer;
                uint16_t entry = history[(terminal_row - 1) * VGA_WIDTH + terminal_column];
                if ((entry & 0xFF) == 0 && terminal_row >= input_start_row) return; /* Next line is empty */

                /* Si le curseur est dans une zone read_only on ne remonte pas */
                if (terminal_row <= input_start_row) return;
                /* Si on est pas tout en haut alors on remonte */
                if (terminal_row > 0) terminal_row--;
                
                /* Remonte l'ecran en actualisant terminal_view_row avec terminal_row */
                if (terminal_row < terminal_view_row) terminal_view_row = terminal_row;

                refresh_screen();
                return;
            }
            if (scancode == 0x50) { // Down
                /* Si la colonne d'en dessous contient un vide alors on n'autorise pas le deplacement vers le bas */
                uint16_t* history = screens[current_screen].buffer;
                uint16_t entry = history[(terminal_row + 1) * VGA_WIDTH + terminal_column];
                if ((entry & 0xFF) == 0 && terminal_row >= input_start_row) return; /* Next line is empty */

                /* Si on est pas au debut de l'historique */
                if (terminal_row < HISTORY_LINES - 1) terminal_row++;
                
                /* Auto-scroll down */
                if (terminal_row >= terminal_view_row + VGA_HEIGHT) terminal_view_row++;

                refresh_screen();
                return;
            }
            if (scancode == 0x49) { // Page Up
                /* Deplace uniquement la ligne de debut d'affichage*/
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

            /* Caractere normnal */
            if (scancode < 128 && kbdus[scancode]) {
                terminal_putchar(kbdus[scancode]);
            }
        }
    }
}

/* --- Main --- */
void kernel_main(void) {
	/* Init fonction */
	terminal_initialize();

	printk("KFS-1 with Bonus 42\n");
	printk("--------------------------------\n");
	printk("Features: %s, %s, %s\n", "Scroll", "Colors", "Printf");
	printk("Press F1/F2/F3 to switch screens.\n");
	printk("Arrow Keys to move, Backspace to delete.\n");
	printk("Type something:\n");
    
    /* Permet de delimiter la zone qui est en read_only */
    void set_input_boundary();
    set_input_boundary();

	/* Heartbeat pour montrer que ca tourne */
    unsigned char spinner[] = {'|', '/', '-', '\\'};
    int spin_idx = 0;
    int tick = 0;

    /* Clean le port de lecture clavier */
    while(inb(0x64) & 0x1) inb(0x60);

	while(1) {
        keyboard_handler();
        
        /* Update le heartbeat tout les 10000 tours */
        tick++;
        if (tick % 10000 == 0) {
            size_t heartbeat_idx = 0 * VGA_WIDTH + 79;
            uint16_t val = vga_entry(spinner[spin_idx], vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
            
            screens[current_screen].buffer[heartbeat_idx] = val;

            if (screens[current_screen].view_row == 0) {
                 vga_buffer[heartbeat_idx] = val;
            }
            
            spin_idx = (spin_idx + 1) % 4;
        }
	}
}
