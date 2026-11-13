#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

enum { PIPE_COUNT = 3};

// closing pipes
static void close_pipes(int pipes[][2], int count)
{
    for(int i = 0; i < count; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

// pipe creation
static int create_pipes(int pipes[][2], int count)
{
    for(int i = 0; i < count; i++)
    {
        if(pipe(pipes[i]) == -1)
        {
            perror("pipe");
            // close only the pipes opened before the failure
        close_pipes(pipes, i);
        return -1;
        }
    }
}

int main()
{
    int pipes[PIPE_COUNT][2];

    if(create_pipes(pipes, PIPE_COUNT) == -1)
    {
        return EXIT_FAILURE;
    }

    close_pipes(pipes, PIPE_COUNT);
    return EXIT_SUCCESS;

}
