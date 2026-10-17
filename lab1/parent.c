#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define NAME_SIZE 4096
#define BUF_SIZE 4096

static int write_all(int fd, const char *buf, unsigned long count) {
    unsigned long written = 0;

    while (written < count) {
        ssize_t result = write(fd, buf + written, count - written);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        written += (unsigned long)result;
    }

    return 0;
}

static void print_error(const char *text) {
    unsigned long length = 0;

    while (text[length] != '\0') {
        length++;
    }

    write_all(STDERR_FILENO, text, length);
}

static int read_filename(char *name, unsigned long size) {
    unsigned long length = 0;

    while (1) {
        char symbol;
        ssize_t result = read(STDIN_FILENO, &symbol, 1);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }

        if (result == 0) {
            break;
        }

        if (symbol == '\n') {
            break;
        }

        if (length + 1 >= size) {
            return -2;
        }

        name[length++] = symbol;
    }

    name[length] = '\0';
    return length == 0 ? -2 : 0;
}

int main(void) {
    char filename[NAME_SIZE];
    char buffer[BUF_SIZE];
    int pipe_fd[2];
    int file_fd;
    pid_t pid;
    int status;

    if (write_all(STDOUT_FILENO, "Введите имя входного файла: ",
              sizeof("Введите имя входного файла: ") - 1) < 0) {
        return 1;
    }

    if (read_filename(filename, NAME_SIZE) != 0) {
        print_error("Ошибка: некорректное имя файла.\n");
        return 1;
    }

    file_fd = open(filename, O_RDONLY);
    if (file_fd < 0) {
        print_error("Ошибка: не удалось открыть файл.\n");
        return 1;
    }

    if (pipe(pipe_fd) < 0) {
        print_error("Ошибка: не удалось создать pipe.\n");
        close(file_fd);
        return 1;
    }

    pid = fork();
    if (pid < 0) {
        print_error("Ошибка: не удалось создать дочерний процесс.\n");
        close(file_fd);
        close(pipe_fd[0]);
        close(pipe_fd[1]);
        return 1;
    }

    if (pid == 0) {
        if (dup2(file_fd, STDIN_FILENO) < 0 ||
            dup2(pipe_fd[1], STDOUT_FILENO) < 0) {
            print_error("Ошибка: не удалось перенаправить поток.\n");
            _exit(1);
        }

        close(file_fd);
        close(pipe_fd[0]);
        close(pipe_fd[1]);

        execl("./child", "child", (char *)0);

        print_error("Ошибка: не удалось запустить child.\n");
        _exit(1);
    }

    close(file_fd);
    close(pipe_fd[1]);

    while (1) {
        ssize_t result = read(pipe_fd[0], buffer, BUF_SIZE);

        if (result < 0) {
            if (errno == EINTR) {
                continue;
            }

            print_error("Ошибка: не удалось прочитать данные из pipe.\n");
            close(pipe_fd[0]);
            return 1;
        }

        if (result == 0) {
            break;
        }

        if (write_all(STDOUT_FILENO, buffer, (unsigned long)result) < 0) {
            print_error("Ошибка: не удалось вывести результат.\n");
            close(pipe_fd[0]);
            return 1;
        }
    }

    close(pipe_fd[0]);

    while (waitpid(pid, &status, 0) < 0) {
        if (errno != EINTR) {
            print_error("Ошибка: не удалось дождаться дочернего процесса.\n");
            return 1;
        }
    }

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        print_error("Дочерний процесс завершился с ошибкой.\n");
        return 1;
    }

    return 0;
}