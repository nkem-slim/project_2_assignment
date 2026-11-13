#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static int copy_file(const char *source, const char *destination, unsigned char *buffer, size_t size)
{
    int in = open(source, O_RDONLY);
    if (in == -1)
    {
        perror(source);
        return 1;
    }

    int out = open(destination, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out == -1)
    {
        perror(destination);
        close(in);
        return 1;
    }

    int failed = 0;

    for (;;)
    {
        ssize_t count = read(in, buffer, size);
        if (count == 0)
        {
            break;
        }

        if (count == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror(source);
            failed = 1;
            break;
        }

        // Finish the chunk if write() accepts only part of it.
        ssize_t offset = 0;
        while (offset < count)
        {
            ssize_t written = write(out, buffer + offset, (size_t)(count - offset));
            if (written == -1 && errno == EINTR)
            {
                continue;
            }

            if (written <= 0)
            {
                if (written == 0)
                {
                    errno = EIO;
                }

                perror(destination);
                failed = 1;
                break;
            }

            offset += written;
        }

        if (failed)
        {
            break;
        }
    }

    if (close(out) == -1)
    {
        perror(destination);
        failed = 1;
    }

    if (close(in) == -1)
    {
        perror(source);
        failed = 1;
    }

    return failed;
}


int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        fprintf(stderr, "Usage: %s source destination buffer_bytes\n", argv[0]);
        return 1;
    }

    char *end;
    errno = 0;
    unsigned long long size = strtoull(argv[3], &end, 10);

    if (argv[3][0] < '0' || argv[3][0] > '9' || *end != '\0' || errno != 0 || size == 0 || size > SIZE_MAX)
    {
        fprintf(stderr, "Buffer size must be a positive integer.\n");
        return 1;
    }

    unsigned char *buffer = malloc((size_t)size);
    if (buffer == NULL)
    {
        perror("malloc");
        return 1;
    }

    struct timespec start, finish;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1)
    {
        perror("clock_gettime");
        free(buffer);
        return 1;
    }

    int failed = copy_file(argv[1], argv[2], buffer, (size_t)size);

    if (clock_gettime(CLOCK_MONOTONIC, &finish) == -1)
    {
        perror("clock_gettime");
        free(buffer);
        return 1;
    }

    free(buffer);

    if (failed)
    {
        return 1;
    }

    double elapsed = (finish.tv_sec - start.tv_sec) + (finish.tv_nsec - start.tv_nsec) / 1e9;

    printf("Elapsed time: %.9f seconds\n", elapsed);
    return 0;
}
