# Task 4: Thread-pool log scanner

## Method and verification

The program scans text logs and counts lines containing a literal substring. I tested it with `ERROR` across 12 generated log files of different sizes, from approximately 1 to 48 MiB. I ran the same workload three times each with 1, 2, 4, and 8 workers. The one-worker configuration served as the baseline. The program measured elapsed time with `clock_gettime(CLOCK_MONOTONIC)`.

I obtained the reference count with:

```sh
grep -F -h -c -- ERROR logs/input-*.log | awk '{total += $1} END {print total}'
```

`grep -F` searches for a literal substring, matching the behavior of `strstr()` in the C program. The reference count was **229,376**, and every benchmark run reported **229,376 matching lines across 12 files**. The eight-worker output file contained one entry for each input file, with its matching line count, total line count, and worker ID. The final summary matched the terminal output.

## Timing results

Speedup is the mean one-worker time divided by the mean time for the given worker count.

| Workers | Run 1 (s) | Run 2 (s) | Run 3 (s) | Mean (s) | Speedup |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 0.407846904 | 0.420116085 | 0.405998511 | 0.411320500 | 1.000× |
| 2 | 0.253691914 | 0.251559327 | 0.263479814 | 0.256243685 | 1.605× |
| 4 | 0.173744473 | 0.178058067 | 0.200656096 | 0.184152879 | 2.234× |
| 8 | 0.262379211 | 0.247293594 | 0.242050119 | 0.250574308 | 1.642× |

## Synchronization and workload balance

The program uses three mutexes. The queue mutex protects the index of the next file, so two workers cannot claim the same entry or lose an index update. The report mutex keeps each per-file entry together. The totals mutex prevents concurrent updates from overwriting each other. After joining every worker, the main thread writes the summary.

Workers release the queue mutex before scanning. A worker that finishes a small file can claim another while other workers are still scanning. In the eight-worker report, worker 2 processed `input-01.log` and later `input-09.log`, illustrating how the shared queue distributes more work as workers become available. A single worker still scans each file, so a large file can keep one worker busy after the queue empties and the others finish.

## Performance analysis

Four workers produced the lowest mean time at 0.184 s, about 55% below the one-worker baseline. Increasing the pool to eight raised the mean to 0.251 s, about 36% slower than four workers. The eight-worker configuration still improved on the baseline by 1.642×.

More workers can scan files concurrently, but they also share CPU time, memory bandwidth, and storage bandwidth. Repeated scans can benefit from the operating system's page cache, which reduces physical disk reads and can make CPU and memory costs more important. Workers also contend briefly for the queue, report, and totals mutexes. These locks cover short operations, and the benchmark did not measure lock wait time, disk activity, or cache hits separately. The measured result establishes that four workers were fastest for this workload.
