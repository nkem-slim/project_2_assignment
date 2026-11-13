#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <fcntl.h>

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

// save output function
static int save_output(int input_fd)
{
    int output_fd = open("result.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (output_fd == -1)
    {
        perror("open result.txt");
        return 1;
    }

    char buffer[4096];
    int result = 0;

    for (;;) {
        ssize_t bytes_read = read(input_fd, buffer, sizeof buffer);

        if (bytes_read == 0)
        {
            break;
        }

        if (bytes_read == -1)
        {
            if (errno == EINTR)
            {
                continue;
            }

            perror("read pipeline");
            result = 1;
            break;
        }

        // Keep writing until the entire chunk is saved.
        ssize_t offset = 0;
        while (offset < bytes_read)
        {
            ssize_t bytes_written = write(output_fd, buffer + offset, (size_t)(bytes_read - offset));

            if (bytes_written == -1)
            {
                if (errno == EINTR)
                {
                    continue;
                }

                perror("write result.txt");
                result = 1;
                break;
            }

            offset += bytes_written;
        }

        if (result != 0)
        {
            break;
        }
    }

    if (close(output_fd) == -1)
    {
        perror("close result.txt");
        return 1;
    }

    return result;
}

// function to reopen the file and show the frst five lines
static int display_output(void)
{
    int fd = open("result.txt", O_RDONLY);
    if (fd == -1)
    {
        perror("open result.txt");
        return 1;
    }

    char buffer[4096];
    size_t lines = 0;
    int unfinished_line = 0;
    int result = 0;

    for (;;)
    {
        ssize_t bytes = read(fd, buffer, sizeof buffer);
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

            perror("read result.txt");
            result = 1;
            break;
        }

        for (ssize_t i = 0; i < bytes; i++)
        {
            // Display the first five lines and count the entire file.
            if (lines < 5 && putchar((unsigned char)buffer[i]) == EOF)
            {
                perror("putchar");
                result = 1;
                break;
            }

            unfinished_line = buffer[i] != '\n';
            if (buffer[i] == '\n')
            {
                lines++;
            }
        }

        if (result != 0)
        {
            break;
        }
    }

    if (close(fd) == -1)
    {
        perror("close result.txt");
        return 1;
    }

    if (result == 0)
    {
        if (unfinished_line && lines < 5)
        {
            putchar('\n');
        }

        printf("Total lines: %zu\n", lines + (size_t)unfinished_line);
    }

    return result;
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

    if(result == 0)
    {
        result = save_output(pipes[PIPE_COUNT - 1][0]);
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

    if(result == 0)
    {
        result = display_output();
    }

    return result;
}
