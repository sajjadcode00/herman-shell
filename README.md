# Herman Shell

**Herman** is a minimal Linux shell written in C.  
It supports built-in commands, I/O redirection, signal handling, and command history.  
This project is primarily a **student project** for learning Linux system programming, but it is also open-source for anyone interested.  

---

## What is "Herman"?
**Herman** is the ache of holding a wish you cannot reach — a quiet yet profound wound born from hope.

---

## ✨ Features
- Built-in commands:
  - `pwd` → show current directory
  - `cd` → change directory
  - `history` → show command history
  - `echo` → print text
  - `date` → show system time
  - `clear` → clear the screen
  - `exit` → close the shell
- Command execution using `execvp`
- I/O redirection (`>`)
- Pipes (`|`)
- Colored `ls` output
- Signal handling:
  - `Ctrl+C` → kill foreground process
  - `Ctrl+Z` → stop foreground process
- Command history saved in `~/.herman/history`
- Basic job control structure (placeholders for `fg`, `bg`, `jobs`)

---

## 📂 Project Structure
```
herman-shell/
├── herman.c        # main source code
├── herman_log.c    # logging functions
├── herman_log.h    # logging header
├── my_lib.h        # custom helper library
├── config.h        # configuration constants /macros/
├── Makefile        # build instructions
└── README.md       # project documentation
```

---

## ⚙️ Build & Run
To compile the shell:
```bash
make
```

This will produce an executable:
```bash
./herman
```

---

## 📖 Usage Examples
```bash
herman>> pwd
/home/sajjad

herman>> ls -l > files.txt
herman>> cat files.txt

herman>> date
Mon Sep 29 12:34:56 2025

herman>> echo Hello World
Hello World

herman>> clear
# screen is cleared

herman>> history
1 pwd
2 ls -l > files.txt
3 date
```

---

## 🚀 Future Work
- Implement `jobs`, `fg`, `bg`
- Improve error handling
- Add tab-completion
- More built-in commands

---

## 📜 License
This project is open-source (for educational purposes).  
Feel free to use and modify it with credit.

---
