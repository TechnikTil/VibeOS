# Using VibeOS

## Boot Sequence

1. Kernel initializes hardware (framebuffer, keyboard, mouse, disk, network)
2. Boot splash shows VibeOS logo with progress bar
3. Desktop launches with dock and menu bar

If desktop fails to load, you'll drop to `vibesh` (the shell). If that fails, you get the recovery shell.

## Desktop

### Window Management

- **Drag title bar** to move windows
- **Drag edges** to resize windows
- **Close button** (red) closes the window
- **Minimize button** (yellow) minimizes to dock
- **Maximize button** (green) toggles fullscreen

### Dock

Click icons to launch apps:

| Icon | App | Description |
|------|-----|-------------|
| Terminal | `/bin/term` | Terminal emulator |
| TextEdit | `/bin/textedit` | Text editor |
| Music | `/bin/music` | Music player |
| Browser | `/bin/browser` | Web browser |
| Calculator | `/bin/calc` | Calculator |
| System Monitor | `/bin/sysmon` | CPU/memory stats |
| Files | `/bin/files` | File manager |
| VibeCode | `/bin/vibecode` | IDE |

Minimized windows appear as thumbnails in the dock. Click to restore.

### Menu Bar

- **VibeOS menu** - About, Quit Desktop
- **File menu** - New Window (for some apps)
- **Edit menu** - Cut, Copy, Paste (app-dependent)

### Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+S | Save (in editors) |
| Ctrl+R | Run (in VibeCode) |
| Escape | Close dialogs, toggle help |

## Shell (vibesh)

The shell supports readline-style editing:

| Key | Action |
|-----|--------|
| Up/Down | Browse command history |
| Tab | Complete commands/paths |
| Ctrl+C | Clear current line |
| Ctrl+U | Clear line before cursor |
| Ctrl+L | Clear screen |
| Ctrl+R | Reverse search history |
| Ctrl+D | Exit shell (EOF) |
| Left/Right | Move cursor |
| Home/End | Jump to start/end |
| `!!` | Repeat last command |

### Built-in Commands

| Command | Description |
|---------|-------------|
| `cd <dir>` | Change directory |
| `exit` | Exit shell |
| `clear` | Clear screen |
| `help` | Show help |
| `time <cmd>` | Measure execution time |
| `mpy [file]` | Run MicroPython |

### File Commands

| Command | Description |
|---------|-------------|
| `ls [path]` | List directory |
| `cat <file>` | Show file contents |
| `cp [-r] <src> <dst>` | Copy files |
| `mv <src> <dst>` | Move/rename files |
| `rm <file>` | Remove file |
| `mkdir <dir>` | Create directory |
| `touch <file>` | Create empty file |
| `pwd` | Print working directory |
| `find <dir> -name <pattern>` | Find files |
| `stat <file>` | Show file info |

### Text Commands

| Command | Description |
|---------|-------------|
| `echo <text>` | Print text (supports `> file`) |
| `grep <pattern> <file>` | Search in files |
| `head [-n N] <file>` | First N lines |
| `tail [-n N] <file>` | Last N lines |
| `wc [-lwc] <file>` | Count lines/words/chars |
| `hexdump [-C] <file>` | Hex dump |

### System Commands

| Command | Description |
|---------|-------------|
| `ps` | List processes |
| `kill <pid>` | Terminate process |
| `uptime` | Show uptime |
| `date` | Show date/time |
| `free [-h]` | Memory usage |
| `df [-h]` | Disk usage |
| `du [-hs] <path>` | Directory size |
| `uname [-a]` | System info |
| `lscpu` | CPU info |
| `lsusb` | USB devices |
| `dmesg` | Kernel log |

### Network Commands

| Command | Description |
|---------|-------------|
| `ping <host>` | Ping host |
| `fetch <url>` | HTTP/HTTPS GET |

### Other Commands

| Command | Description |
|---------|-------------|
| `vim <file>` | Edit file |
| `sleep <seconds>` | Sleep |
| `seq <n>` | Print 1 to n |
| `which <cmd>` | Find command |
| `hostname` | Show hostname |
| `whoami` | Show user |

## Applications

### Terminal (`/bin/term`)

Terminal emulator running vibesh. Supports:
- 500-line scrollback buffer
- Mouse drag to scroll
- Full keyboard input

### vim (`/bin/vim`)

Modal text editor:

**Normal mode:**
| Key | Action |
|-----|--------|
| `h/j/k/l` | Move cursor |
| `w/b/e` | Word movement |
| `0/$` | Line start/end |
| `gg/G` | File start/end |
| `i/a/o` | Enter insert mode |
| `d<motion>` | Delete |
| `y<motion>` | Yank (copy) |
| `p` | Paste |
| `u` | Undo |
| `/<pattern>` | Search |

**Insert mode:**
- Type normally
- Escape to return to normal mode

**Command mode (`:`):**
| Command | Action |
|---------|--------|
| `:w` | Save |
| `:q` | Quit |
| `:wq` | Save and quit |
| `:q!` | Quit without saving |

### TextEdit (`/bin/textedit`)

Simple GUI text editor:
- Ctrl+S to save
- Save As dialog for new files
- Warns before closing unsaved files
- Syntax highlighting for C and Python

### File Manager (`/bin/files`)

- Click to select, double-click to open
- Right-click for context menu (New, Rename, Delete)
- Backspace to go up a directory
- Double-click files to open with associated app

File associations:
- `.c`, `.h`, `.py`, `.txt` - TextEdit
- `.png`, `.jpg`, `.bmp` - Viewer
- `.mp3`, `.wav` - Music

### Browser (`/bin/browser`)

Web browser with HTML/CSS rendering:
- Address bar at top (click to edit, Enter to go)
- Back button for history
- Click links to navigate
- Supports HTTP and HTTPS
- Arrow keys or j/k to scroll

### Music Player (`/bin/music`)

Two modes:
1. **Album mode** - Browse `/home/user/Music/` for album/playlist folders
2. **Single file mode** - `music /path/to/song.mp3`

Controls:
| Key | Action |
|-----|--------|
| Space | Play/Pause |
| N | Next track |
| P | Previous track |
| Up/Down | Select track |
| Enter | Play selected |

### Calculator (`/bin/calc`)

Standard calculator with floating-point support:
- Click buttons or use keyboard
- Supports +, -, *, /
- Decimal point
- C or Escape to clear

### System Monitor (`/bin/sysmon`)

Shows:
- Uptime
- Memory usage (used/free)
- Process list
- Heap debug info

### VibeCode (`/bin/vibecode`)

IDE for writing programs:
- File tree sidebar
- Code editor with syntax highlighting
- Output panel for program output
- Run button executes with TCC (.c) or MicroPython (.py)

Shortcuts:
| Key | Action |
|-----|--------|
| Ctrl+S | Save |
| Ctrl+R | Run |
| Ctrl+N | New file |
| Escape | Toggle help |

### DOOM (`/bin/doom`)

The classic FPS. Requires `doom1.wad` at `/games/doom1.wad`.
Meaning you need copy it to vibeos_root/games/

Controls:
| Input | Action |
|-------|--------|
| Arrow keys / WASD | Move |
| Ctrl / Left mouse | Fire |
| Shift | Run |
| Space / E | Use (open doors) |
| Mouse | Turn |
| 1-9 | Select weapon |
| Escape | Menu |

### MicroPython (`/bin/micropython`)

Python interpreter with kernel API access:

```bash
# Interactive REPL
mpy

# Run script
mpy /path/to/script.py
```

Example:
```python
import vibe

vibe.clear()
vibe.set_color(vibe.GREEN, vibe.BLACK)
vibe.puts("Hello from Python!")

# Graphics
w, h = vibe.screen_size()
vibe.fill_rect(100, 100, 50, 50, vibe.RED)

# Files
files = vibe.listdir("/bin")
for f in files:
    print(f)
```

See [PROGRAMMING.md](PROGRAMMING.md) for the full API.

### TCC (`/bin/tcc`)

C compiler that runs on VibeOS:

```bash
cd /home/user
tcc hello.c -o hello
./hello
```

See [PROGRAMMING.md](PROGRAMMING.md) for writing programs.

## Filesystem Layout

```
/
├── bin/           # Programs
├── lib/
│   └── tcc/       # TCC runtime (headers, libraries)
├── home/
│   └── user/      # Home directory
│       └── Music/ # Music player looks here for albums
├── games/
│   └── doom1.wad  # DOOM WAD file (you provide this)
├── usr/
│   └── src/       # Source code (for reference/editing)
└── etc/
    └── motd       # Message of the day
```
