# Nkem Vincent Nweke
---
## Project 2: Systems Programming

This project explores Linux system calls, file I/O performance, and POSIX threads through four C programs.

## Requirements

Linux, GCC, `strace`, and Python 3. The verification commands use standard command-line tools including `cmp`, `grep`, `awk`, `head`, and `wc`.

| Task | Program | Report |
| --- | --- | --- |
| 1 | Three-stage command pipeline | [Task 1 report](task-1/report.md) |
| 2 | System-call and standard I/O file copies | [Task 2 report](task-2/report.md) |
| 3 | Single-threaded and multithreaded perfect-number searches | [Task 3 report](task-3/report.md) |
| 4 | Thread-pool log scanner | [Task 4 report](task-4/report.md) |

## Task 1: Three-stage command pipeline

From `task-1`, build and run:

```sh
gcc index.c -o pipeline
./pipeline
```

The program runs `cat /etc/passwd | cut -d: -f7 | sort -u` through three child processes. It saves the full output to `result.txt`, reports the child exit statuses, and displays up to five lines and the total line count. The shell paths depend on the machine's `/etc/passwd`.

Verify the saved output and collect the traces:

```sh
cat /etc/passwd | cut -d: -f7 | sort -u > /tmp/project2-expected
cmp /tmp/project2-expected result.txt
head -n 5 result.txt
wc -l < result.txt
strace -f -tt -o trace.log ./pipeline
strace -f -e trace=clone,fork,execve,pipe,pipe2,dup2,open,openat,read,write,close,wait4 ./pipeline
```

## Task 2: File-copy performance

From `task-2`, build both versions and generate the 100 MiB input:

```sh
gcc copy_sys.c -o copy_sys
gcc copy_stdio.c -o copy_stdio
dd if=/dev/urandom of=big.bin bs=1M count=100
```

Each program takes a source path, a destination path, and a buffer size in bytes. Use a separate destination because each run overwrites it.

```sh
./copy_sys big.bin out.bin 4096 && cmp big.bin out.bin
./copy_stdio big.bin out.bin 4096 && cmp big.bin out.bin
```

Run each program three times at each buffer size (`512`, `4096`, and `65536`) and average the printed elapsed times. Run the syscall summaries separately:

```sh
strace -c ./copy_sys big.bin out.bin 4096
strace -c ./copy_stdio big.bin out.bin 4096
```

Repeat the trace commands with `512` and `65536` for the other syscall counts. Use the untraced runs for the timing averages.

## Task 3: Perfect numbers with POSIX threads

From `task-3`, build and run:

```sh
gcc index.c -o perfect -pthread
./perfect
```

The program searches from 1 through 100,000 using both a single-threaded search and eight workers. It should print `6 28 496 8128`, a synchronized total of `4`, and the elapsed time for both searches.

## Task 4: Thread-pool log scanner

From `task-4`, build the scanner and create the log directory:

```sh
gcc index.c -o logscan -pthread
mkdir -p logs
```

To reproduce the benchmark input, save the following as `generate_logs.py` in `task-4` and run `python3 generate_logs.py`. It creates 12 files of different sizes with one `ERROR` line per 40 lines.

```python
from pathlib import Path

block = (b"INFO request completed\n" * 39 + b"ERROR request failed\n") * 1024
sizes = (1, 2, 3, 4, 6, 8, 12, 16, 24, 32, 40, 48)

for number, mib in enumerate(sizes, 1):
    blocks = (mib * 1024 * 1024 + len(block) - 1) // len(block)
    with Path(f"logs/input-{number:02}.log").open("wb") as file:
        for _ in range(blocks):
            file.write(block)
```

Get the reference total and run each configuration:

```sh
grep -F -h -c -- ERROR logs/input-*.log | awk '{total += $1} END {print total}'
./logscan ERROR scan-1.txt 1 logs/input-*.log
./logscan ERROR scan-2.txt 2 logs/input-*.log
./logscan ERROR scan-4.txt 4 logs/input-*.log
./logscan ERROR scan-8.txt 8 logs/input-*.log
```

Run each `logscan` command three times. Each run should report **229,376 matching lines across 12 files**. Average the three elapsed times for each worker count and calculate speedup by dividing the one-worker mean by that configuration's mean. Each `scan-*.txt` contains a per-file entry and a final summary. Entry order can vary between runs.

The build and test commands generate the executables, input data, and output files locally. The linked reports contain the measured results and analysis.
