#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>

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
    return 0;
}

static pid_t start_stage(char* const args[], int input_fd, int output_fd, int pipes[][2])
{
    pid_t pid = fork();
    if(pid == -1)
    {
        perror("fork");
        return -1;
    }
    if(pid > 0)
    {
        return pid;
    }

    if(input_fd != -1 && dup2(input_fd, STDIN_FILENO) == -1)
    {
        perror("dup2 stdin");
        _exit(1); // terminate the process immediately without running the usual C Library cleanup process
    }
    if(output_fd != -1 && dup2(output_fd, STDOUT_FILENO) == -1)
    {
        perror("dup2 stdout");
        _exit(1);
    }

    // keep the redirected descriptors and close the original pipe ends
    close_pipes(pipes, PIPE_COUNT);

    execvp(args[0], args);
    perror(args[0]);
    _exit(127); // immediate termination and returning "command not found"
}

int main()
{
    int pipes[PIPE_COUNT][2];
    char* cat_args[] = {"cat", "/etc/passwd", NULL};

    if(create_pipes(pipes, PIPE_COUNT) == -1)
    {
        return 0;
    }

    pid_t child = start_stage(cat_args, -1, -1, pipes);
    close_pipes(pipes, PIPE_COUNT);

    if(child == -1)
    {
        return 0;
    }

    int status;
    if(waitpid(child, &status, 0) == -1)
    {
        perror("waitpid");
        return 0;
    }

    if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
    {
        return 1;
    }

    return 0;
}
