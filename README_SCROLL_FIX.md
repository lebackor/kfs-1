# KFS-1 Scroll & Delete Fix Explanation

## The Issue
The original kernel implementation had a bug in the terminal scrolling logic. The `input_start_row` and `input_start_col` variables define the "barrier" that prevents the user from backspacing into the command prompt or previous output.

However, when the terminal screen filled up and scrolled (moving all text up by one line), these barrier coordinates remained static. This meant that:
1. The text physically moved up.
2. The protection barrier stayed at the old row.
3. As a result, valid user input on the new lines ended up "behind" the barrier, making it impossible to delete or modify.

## The Fix
We modified `terminal_scroll()` in `kernel.c` to dynamically update the protection boundary:

```c
    /* Update input protection boundary so it moves up with the text */
    if (input_start_row > 0) {
        input_start_row--;
    } else {
        /* Boundry scrolled off screen */
        input_start_row = 0;
        input_start_col = 0;
    }
```

Now, whenever the screen scrolls, the barrier moves up with it, preserving the relationship between the prompt and the user's input.

## Spinner Artifact Fix
When backspacing on the first line (Row 0), the "ripple delete" logic was shifting the entire line to the left. This included index 79, which contains the "Heartbeat" spinner used to prove the kernel is running. As a result, backspacing on row 0 would drag copies of the spinner across the screen.

We patched `terminal_putchar` to respect the spinner's reserved zone:
```c
/* Protect Heartbeat: If on row 0, don't pull index 79 into 78 */
size_t max_col = (terminal_row == 0) ? (VGA_WIDTH - 2) : (VGA_WIDTH - 1);
```
On row 0, the shift loop now stops at column 78, leaving the spinner at column 79 untouched.
