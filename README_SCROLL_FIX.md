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
- Implemented a **100-line History Buffer** (replacing the single 25-line screen buffer).
- Implemented a **Viewport System**. The screen now acts as a window into this larger buffer.
- **Auto-Scroll**: When typing past the bottom, the view scrolls down, but old lines are preserved in memory.
- **Reverse Deletion**: You can now backspace all the way back up to previous lines ("remonter en supprimant"). The view will scroll up automatically.
- **Manual Scrolling**: Use `PageUp` (Keypad 9) and `PageDown` (Keypad 3) to browse history without deleting.
