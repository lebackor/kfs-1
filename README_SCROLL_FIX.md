# KFS-1 Terminal Fixes

## 1. Scroll & Delete Barrier Fix
**Issue**: Original code had a fixed "input barrier" that forbade backspacing. When the terminal scrolled (destructively), this barrier didn't move, locking the user out of editing valid text.
**Fix**: Input barrier now tracks the logical row index in the history buffer.

## 2. Spinner Artifact Fix
**Issue**: Backspacing on the top line dragged the "Heartbeat" spinner (from column 79) across the screen.
**Fix**: `terminal_putchar` backspace logic now protects column 79 on logical Row 0, preventing the spinner from being shifted.

## 3. Scrollback History (New!)
**Issue**: Previous text was lost forever when scrolling down.
**Fix**: 
- 100-line history buffer.
- Viewport scrolling.
- Manual PageUp/PageDown support.

## 4. Input Boundary Refinement
**Issue**: Trailing space in prompt caused confusion.
**Fix**: Changed "Type something: " to "Type something:".

## 5. Intelligent Backspace
**Issue**: 
- "Dumb Wrap" jumped to empty void (col 79).
- "Old Smart Wrap" deleted characters immediately upon wrapping.
- "Full Line" characters (at col 79) were difficult to delete.

**New Fix Logic**:
1.  **Smart Scan**: When wrapping to the previous line, the cursor scans right-to-left to find the last actual character. It lands *after* it, avoiding the empty void.
2.  **Non-Destructive Wrap**: Wrapping to the previous line *moves* the cursor but does NOT delete the character it lands on.
3.  **Full Line Handler**: If the cursor lands at Column 79 (because the line is full), it sits on top of the last character. Pressing backspace *again* detects this special state ("At col 79 with text") and deletes that character in-place without moving cursor.
