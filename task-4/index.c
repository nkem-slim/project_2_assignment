#include <errno.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

struct work
{
    const char *pattern;
    char **files;
    size_t file_count;
    size_t next_file;
    size_t files_scanned;
    size_t total_matches;
    size_t total_lines;
    FILE *report;
    pthread_mutex_t queue_mutex;
    pthread_mutex_t report_mutex;
    pthread_mutex_t totals_mutex;
};

struct worker_args
{
    struct work *work;
    size_t id;
};

static int scan_file(const char *path, const char *pattern, size_t *matches, size_t *lines)
{
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        perror(path);
        return 1;
    }

    char chunk[4096];
    char *line = NULL;
    size_t length = 0;
    size_t capacity = 0;
    int failed = 0;

    while (fgets(chunk, sizeof chunk, file) != NULL)
    {
        size_t count = strlen(chunk);
        if (count >= SIZE_MAX - length)
        {
            errno = EOVERFLOW;
            perror(path);
            failed = 1;
            break;
        }

        size_t needed = length + count + 1;
        if (needed > capacity)
        {
            char *larger = realloc(line, needed);
            if (larger == NULL)
            {
                perror(path);
                failed = 1;
                break;
            }

            line = larger;
            capacity = needed;
        }

        // Join chunks until we have a complete line.
        memcpy(line + length, chunk, count + 1);
        length += count;

        if (length > 0 && line[length - 1] == '\n')
        {
            line[length - 1] = '\0';
            (*lines)++;
            *matches += strstr(line, pattern) != NULL;
            length = 0;
        }
    }

    if (ferror(file))
    {
        perror(path);
        failed = 1;
    }

    // Count the last line even if it has no newline.
    if (!failed && length > 0)
    {
        (*lines)++;
        *matches += strstr(line, pattern) != NULL;
    }

    free(line);

    if (fclose(file) == EOF)
    {
        perror(path);
        failed = 1;
    }

    return failed;
}

static void *worker(void *argument)
{
    struct worker_args *args = argument;
    struct work *work = args->work;

    for (;;)
    {
        pthread_mutex_lock(&work->queue_mutex);

        if (work->next_file == work->file_count)
        {
            pthread_mutex_unlock(&work->queue_mutex);
            return NULL;
        }

        const char *path = work->files[work->next_file++];
        pthread_mutex_unlock(&work->queue_mutex);

        size_t matches = 0;
        size_t lines = 0;
        int failed = scan_file(path, work->pattern, &matches, &lines);

        pthread_mutex_lock(&work->report_mutex);
        if (failed)
        {
            fprintf(work->report, "%s: scan failed (thread %zu)\n", path, args->id);
        }
        else
        {
            fprintf(work->report, "%s: %zu matching lines out of %zu (thread %zu)\n", path, matches, lines, args->id);
        }
        pthread_mutex_unlock(&work->report_mutex);

        if (!failed)
        {
            pthread_mutex_lock(&work->totals_mutex);
            work->files_scanned++;
            work->total_matches += matches;
            work->total_lines += lines;
            pthread_mutex_unlock(&work->totals_mutex);
        }
    }
}

static double current_time(void)
{
    struct timespec now;

    if (clock_gettime(CLOCK_MONOTONIC, &now) == -1)
    {
        perror("clock_gettime");
        exit(1);
    }

    return now.tv_sec + now.tv_nsec / 1e9;
}

int main(int argc, char *argv[])
{
    if (argc < 5)
    {
        fprintf(stderr, "Usage: %s pattern report.txt threads file1.log ...\n", argv[0]);
        return 1;
    }

    char *end;
    errno = 0;
    unsigned long long requested = strtoull(argv[3], &end, 10);

    if (argv[3][0] < '0' || argv[3][0] > '9' || *end != '\0' || errno != 0 || requested == 0 || requested > SIZE_MAX)
    {
        fprintf(stderr, "Thread count must be a positive integer.\n");
        return 1;
    }

    size_t thread_count = (size_t)requested;
    pthread_t *threads = calloc(thread_count, sizeof *threads);
    struct worker_args *args = calloc(thread_count, sizeof *args);

    if (threads == NULL || args == NULL)
    {
        perror("calloc");
        free(threads);
        free(args);
        return 1;
    }

    struct work work =
    {
        .pattern = argv[1],
        .files = argv + 4,
        .file_count = (size_t)(argc - 4),
        .report = fopen(argv[2], "w")
    };

    if (work.report == NULL)
    {
        perror(argv[2]);
        free(threads);
        free(args);
        return 1;
    }

    pthread_mutex_t *mutexes[] =
    {
        &work.queue_mutex, &work.report_mutex, &work.totals_mutex
    };

    size_t initialized = 0;
    int failed = 0;

    for (; initialized < sizeof mutexes / sizeof mutexes[0]; initialized++)
    {
        int error = pthread_mutex_init(mutexes[initialized], NULL);
        if (error != 0)
        {
            fprintf(stderr, "pthread_mutex_init: %s\n", strerror(error));
            failed = 1;
            break;
        }
    }

    if (!failed)
    {
        double start = current_time();
        size_t started = 0;

        for (; started < thread_count; started++)
        {
            args[started].work = &work;
            args[started].id = started + 1;

            int error = pthread_create(&threads[started], NULL, worker, &args[started]);
            if (error != 0)
            {
                fprintf(stderr, "pthread_create: %s\n", strerror(error));
                failed = 1;
                break;
            }
        }

        for (size_t i = 0; i < started; i++)
        {
            int error = pthread_join(threads[i], NULL);
            if (error != 0)
            {
                fprintf(stderr, "pthread_join: %s\n", strerror(error));
                return 1;
            }
        }

        if (fprintf(work.report, "Total matching lines across %zu files: %zu\n", work.files_scanned, work.total_matches) < 0 ||
            ferror(work.report))
        {
            fprintf(stderr, "Error writing %s\n", argv[2]);
            failed = 1;
        }

        if (fclose(work.report) == EOF)
        {
            perror(argv[2]);
            failed = 1;
        }

        double elapsed = current_time() - start;
        printf("Total matching lines across %zu files: %zu\n", work.files_scanned, work.total_matches);
        printf("Elapsed time: %.9f seconds\n", elapsed);

        if (work.files_scanned != work.file_count)
        {
            failed = 1;
        }
    }
    else
    {
        fclose(work.report);
    }

    while (initialized > 0)
    {
        pthread_mutex_destroy(mutexes[--initialized]);
    }

    free(threads);
    free(args);
    return failed;
}
