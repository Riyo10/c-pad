# C-Pad: Native Win32 Notepad

A high-performance, multi-tabbed text editor written in **C** using the **Win32 API**. Designed for Windows and Linux (via Wine compatibility).

## 🚀 Build & Run

### 1. Requirements

* **Linux:** `mingw-w64` (for compiling) and `wine` (for running).
* **Windows:** `gcc` (MinGW-w64).

### 2. Compilation

To compile the application with all necessary libraries (Common Dialogs, GDI, and Common Controls), use:

```bash
x86_64-w64-mingw32-gcc notepad.c -o notepad.exe -lcomdlg32 -lgdi32 -lcomctl32 -mwindows

```

### 3. Execution

* **On Windows:** Double-click `notepad.exe`.
* **On Linux:** Run `wine notepad.exe`.

---

## 🛠 Automation (Makefile)

To avoid typing long commands, create a file named `Makefile` in your directory and paste this:

```makefile
# Variables
CC = x86_64-w64-mingw32-gcc
CFLAGS = -Wall -mwindows
LIBS = -lcomdlg32 -lgdi32 -lcomctl32
TARGET = notepad.exe
SRC = notepad.c

# Default rule: Build
all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(SRC) -o $(TARGET) $(CFLAGS) $(LIBS)

# Run rule
run: $(TARGET)
	wine $(TARGET)

# Clean rule
clean:
	rm -f $(TARGET)

```

### How to use the Makefile:

1. **To Build:** Just type `make`.
2. **To Build and Run immediately:** Type `make run`.
3. **To Delete the exe:** Type `make clean`.

---

## 📂 Features

* **Multi-Tabbed Interface:** Support for up to 16 simultaneous document slots.
* **Font Customization:** Integrated Windows Font Picker.
* **Accelerator Keys:** * `Ctrl+N`: New File
* `Ctrl+T`: New Tab
* `Ctrl+S`: Save
* `Ctrl+W`: Close Tab
* `Ctrl+O`: Open File