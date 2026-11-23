
#include "common.h"

#include <errno.h>
#include <sys/socket.h>

// складываем два числа по модулю без переполнения
uint64_t AddModulo(uint64_t a, uint64_t b, uint64_t mod) {
    if (a >= mod - b) {
        return a - (mod - b);
    }

    return a + b;
}

// умножаем два числа по модулю без переполнения
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod) {
    uint64_t result = 0;
    a %= mod;

    while (b > 0) {
        if (b % 2 == 1) {
            result = AddModulo(result, a, mod);
        }

        a = AddModulo(a, a, mod);
        b /= 2;
    }

    return result;
}

// отправляем все байты сообщения
int SendAll(int fd, const void *buffer, size_t size) {
    size_t sent = 0;
    const char *data = (const char *)buffer;

    while (sent < size) {
        ssize_t n = send(fd, data + sent, size - sent, MSG_NOSIGNAL);

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        if (n == 0) {
            return -1;
        }

        sent += (size_t)n;
    }

    return 0;
}

// получаем все байты сообщения
int RecvAll(int fd, void *buffer, size_t size) {
    size_t received = 0;
    char *data = (char *)buffer;

    while (received < size) {
        ssize_t n = recv(fd, data + received, size - received, 0);

        if (n == 0) {
            return -1;
        }

        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }

            return -1;
        }

        received += (size_t)n;
    }

    return 0;
}
