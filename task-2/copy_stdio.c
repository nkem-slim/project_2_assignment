#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static int copy_file(const char *source, const char *destination, unsigned char *buffer, size_t size)
{
    FILE *in = fopen(source, "rb");
    if (in == NULL)
    {
        perror(source);
        return 1;
    }

    FILE *out = fopen(destination, "wb");
    if (out == NULL)
    {
        perror(destination);
        fclose(in);
        return 1;
    }

    int failed = 0;
    size_t count;

    while ((count = fread(buffer, 1, size, in)) > 0)
    {
        if (fwrite(buffer, 1, count, out) != count)
        {
            perror(destination);
            failed = 1;
            break;
        }
    }

    if (ferror(in))
    {
        perror(source);
        failed = 1;
    }

    // Closing the output flushes any buffered bytes.
    if (fclose(out) == EOF)
    {
        perror(destination);
        failed = 1;
    }

    if (fclose(in) == EOF)
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
