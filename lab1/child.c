#include <unistd.h>
#include <errno.h>
#include <limits.h>

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

static int print_number(long long number) {
    char output[32];
    unsigned long length = 0;
    unsigned long long value;

    if (number < 0) {
        output[length++] = '-';
        value = (unsigned long long)(-(number + 1)) + 1;
    } else {
        value = (unsigned long long)number;
    }

    if (value == 0) {
        output[length++] = '0';
    } else {
        char digits[24];
        unsigned long digits_count = 0;

        while (value > 0) {
            digits[digits_count++] = (char)('0' + value % 10);
            value /= 10;
        }

        while (digits_count > 0) {
            output[length++] = digits[--digits_count];
        }
    }

    output[length++] = '\n';
    return write_all(STDOUT_FILENO, output, length);
}

static int finish_number(long long *sum, long long value, int negative) {
    long long number;

    if (negative) {
        number = -value;
    } else {
        number = value;
    }

    *sum += number;
    return 0;
}

int main(void) {
    char buffer[BUF_SIZE];
    long long sum = 0;
    long long value = 0;
    int has_number = 0;
    int negative = 0;
    int line_has_numbers = 0;

    while (1) {
        ssize_t read_count = read(STDIN_FILENO, buffer, BUF_SIZE);

        if (read_count < 0) {
            if (errno == EINTR) {
                continue;
            }

            print_error("Ошибка: не удалось прочитать входной файл.\n");
            return 1;
        }

        if (read_count == 0) {
            break;
        }

        for (ssize_t i = 0; i < read_count; i++) {
            char symbol = buffer[i];

            if (symbol >= '0' && symbol <= '9') {
                if (!has_number) {
                    has_number = 1;
                    value = 0;
                }

                value = value * 10 + (symbol - '0');

                if ((!negative && value > INT_MAX) ||
                    (negative && value > (long long)INT_MAX + 1)) {
                    print_error("Ошибка: число не помещается в int.\n");
                    return 1;
                }

                continue;
            }

            if (symbol == '-' && !has_number) {
                has_number = 1;
                negative = 1;
                value = 0;
                continue;
            }

            if (symbol == ' ' || symbol == '\t' || symbol == '\n' ||
                symbol == '\r') {
                if (has_number) {
                    if (finish_number(&sum, value, negative) != 0) {
                        return 1;
                    }

                    line_has_numbers = 1;
                    has_number = 0;
                    negative = 0;
                    value = 0;
                }

                if (symbol == '\n' && line_has_numbers) {
                    if (print_number(sum) < 0) {
                        print_error("Ошибка: не удалось записать результат в pipe.\n");
                        return 1;
                    }

                    sum = 0;
                    line_has_numbers = 0;
                }

                continue;
            }

            print_error("Ошибка: в файле обнаружен недопустимый символ.\n");
            return 1;
        }
    }

    if (has_number) {
        finish_number(&sum, value, negative);
        line_has_numbers = 1;
    }

    if (line_has_numbers && print_number(sum) < 0) {
        print_error("Ошибка: не удалось записать результат в pipe.\n");
        return 1;
    }

    return 0;
}