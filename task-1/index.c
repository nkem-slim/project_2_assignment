#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

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


int main(void)
{
    int pipes[PIPE_COUNT][2];
    char *cat_args[] = {"cat", "/etc/passwd", NULL};
    char *cut_args[] = {"cut", "-d:", "-f7", NULL};
    char *sort_args[] = {"sort", "-u", NULL};
    char **commands[] = {cat_args, cut_args, sort_args};
    pid_t children[PIPE_COUNT];
    int started = 0;
    int result = 0;

    if (create_pipes(pipes, PIPE_COUNT) == -1)
    {
        return 1;
    }

    for (int i = 0; i < PIPE_COUNT; i++)
    {
        int input_fd = i == 0 ? -1 : pipes[i - 1][0];

        children[i] = start_stage(commands[i], input_fd, pipes[i][1], pipes);
        if (children[i] == -1)
        {
            result = 1;
            break;
        }

        started++;
    }

    // Keep only the read end of the final pipe.
    close_pipes(pipes, PIPE_COUNT - 1);
    close(pipes[PIPE_COUNT - 1][1]);

    if (result == 0)
    {
        char buffer[4096];

        for (;;)
        {
            ssize_t bytes = read(pipes[PIPE_COUNT - 1][0], buffer, sizeof buffer);

            if (bytes == 0)
            {
                break;
            }

            if (bytes == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                perror("read");
                result = 1;
                break;
            }

            if (fwrite(buffer, 1, (size_t)bytes, stdout) != (size_t)bytes)
            {
                perror("fwrite");
                result = 1;
                break;
            }
        }
    }

    close(pipes[PIPE_COUNT - 1][0]);

    for (int i = 0; i < started; i++)
    {
        int status;
        pid_t waited;

        do {
            waited = waitpid(children[i], &status, 0);
        } while (waited == -1 && errno == EINTR);

        if (waited == -1)
        {
            perror("waitpid");
            result = 1;
        } else if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        {
            result = 1;
        }
    }

    return result;
}
