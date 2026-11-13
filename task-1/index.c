#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

// closing pipes
static void close_pipes(int pipes[][2], int count)
{
    for(int i = 0; i < count; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

int main()
{
    int pipes[1][2];

    if(pipe(pipes[0]) == -1)
    {
        perror("pipe");
        return EXIT_FAILURE;
    }

    close_pipes(pipes, 1);
    return EXIT_SUCCESS;

}
