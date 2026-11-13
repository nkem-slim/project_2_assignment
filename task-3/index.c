#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

enum { LIMIT = 100000, THREAD_COUNT = 8 };

struct range
{
    int start;
    int end;
};

static pthread_mutex_t mutex;
static int perfect_numbers[LIMIT];
static int stored_count;
static int total_count;

static int is_perfect(int number)
{
    if (number < 2)
    {
        return 0;
    }

    int sum = 1;

    // Check each divisor pair once, up to the square root.
    for (int divisor = 2; divisor <= number / divisor; divisor++)
    {
        if (number % divisor == 0)
        {
            int partner = number / divisor;
            sum += divisor;

            if (partner != divisor)
            {
                sum += partner;
            }
        }
    }

    return sum == number;
}

static void *search_range(void *argument)
{
    const struct range *range = argument;
    int local_count = 0;

    for (int number = range->start; number <= range->end; number++)
    {
        if (is_perfect(number))
        {
            pthread_mutex_lock(&mutex);
            perfect_numbers[stored_count++] = number;
            pthread_mutex_unlock(&mutex);
            local_count++;
        }
    }

    // Add this thread's count once after scanning its range.
    pthread_mutex_lock(&mutex);
    total_count += local_count;
    pthread_mutex_unlock(&mutex);

    return NULL;
}

static int compare_numbers(const void *left, const void *right)
{
    int first = *(const int *)left;
    int second = *(const int *)right;
    return first - second;
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

int main(void)
{
    double start = current_time();
    int single_count = 0;

    for (int number = 1; number <= LIMIT; number++)
    {
        single_count += is_perfect(number);
    }

    double single_time = current_time() - start;

    int error = pthread_mutex_init(&mutex, NULL);
    if (error != 0)
    {
        fprintf(stderr, "pthread_mutex_init: %s\n", strerror(error));
        return 1;
    }

    pthread_t threads[THREAD_COUNT];
    struct range ranges[THREAD_COUNT];
    int segment_size = LIMIT / THREAD_COUNT;
    int started = 0;

    start = current_time();

    for (int i = 0; i < THREAD_COUNT; i++)
    {
        ranges[i].start = i * segment_size + 1;
        ranges[i].end = (i + 1) * segment_size;

        error = pthread_create(&threads[i], NULL, search_range, &ranges[i]);
        if (error != 0)
        {
            fprintf(stderr, "pthread_create: %s\n", strerror(error));
            break;
        }

        started++;
    }

    for (int i = 0; i < started; i++)
    {
        error = pthread_join(threads[i], NULL);
        if (error != 0)
        {
            fprintf(stderr, "pthread_join: %s\n", strerror(error));
            return 1;
        }
    }

    double multi_time = current_time() - start;
    pthread_mutex_destroy(&mutex);

    if (started != THREAD_COUNT)
    {
        return 1;
    }

    if (total_count != single_count || total_count != stored_count)
    {
        fprintf(stderr, "The search counts do not match.\n");
        return 1;
    }

    qsort(perfect_numbers, stored_count, sizeof perfect_numbers[0],
          compare_numbers);

    printf("Perfect numbers found:");
    for (int i = 0; i < stored_count; i++)
    {
        printf(" %d", perfect_numbers[i]);
    }

    printf("\nThe synchronized total number of perfect numbers between 1 and 100000 is %d\n", total_count);
    printf("Single-threaded time: %.9f seconds\n", single_time);
    printf("Multi-threaded time:  %.9f seconds\n", multi_time);

    return 0;
}
