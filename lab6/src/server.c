
#include "common.h"

#include <errno.h>
#include <inttypes.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <getopt.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>

// параметры вычисления для одного потока
struct FactorialArgs {
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
    uint64_t result;
};

// вычисляем произведение чисел от begin до end по модулю
uint64_t Factorial(const struct FactorialArgs *args) {
    uint64_t ans = 1 % args->mod;

    if (args->begin > args->end) {
        return ans;
    }

    for (uint64_t i = args->begin;; i++) {
        ans = MultModulo(ans, i, args->mod);

        if (i == args->end) {
            break;
        }
    }

    return ans;
}

// функция которую выполняет отдельный поток
void *ThreadFactorial(void *args) {
    struct FactorialArgs *fargs = (struct FactorialArgs *)args;

    fargs->result = Factorial(fargs);

    return NULL;
}

// распределяем диапазон между потоками
int ParallelFactorial(
    uint64_t begin,
    uint64_t end,
    uint64_t mod,
    int tnum,
    uint64_t *answer
) {
    pthread_t *threads = calloc((size_t)tnum, sizeof(pthread_t));

    struct FactorialArgs *args =
        calloc((size_t)tnum, sizeof(struct FactorialArgs));

    if (threads == NULL || args == NULL) {
        free(threads);
        free(args);
        return -1;
    }

    // определяем количество чисел в диапазоне
    uint64_t count = 0;

    if (begin <= end) {
        count = end - begin + 1;
    }

    uint64_t base = count / (uint64_t)tnum;
    uint64_t remainder = count % (uint64_t)tnum;

    uint64_t current = begin;

    int created = 0;
    int failed = 0;

    for (int i = 0; i < tnum; i++) {
        uint64_t length = base;

        // распределяем оставшиеся числа между первыми потоками
        if ((uint64_t)i < remainder) {
            length++;
        }

        args[i].mod = mod;
        args[i].result = 1 % mod;

        if (length == 0) {
            // обозначаем пустой диапазон
            args[i].begin = 1;
            args[i].end = 0;
        } else {
            args[i].begin = current;
            args[i].end = current + length - 1;

            current += length;
        }

        // создаём поток
        int err = pthread_create(
            &threads[i],
            NULL,
            ThreadFactorial,
            &args[i]
        );

        if (err != 0) {
            fprintf(stderr, "pthread_create failed: %s\n", strerror(err));
            failed = 1;
            break;
        }

        created++;
    }

    // дожидаемся завершения созданных потоков
    for (int i = 0; i < created; i++) {
        int err = pthread_join(threads[i], NULL);

        if (err != 0) {
            fprintf(stderr, "pthread_join failed: %s\n", strerror(err));
            failed = 1;
        }
    }

    // объединяем результаты потоков
    if (!failed) {
        uint64_t total = 1 % mod;

        for (int i = 0; i < tnum; i++) {
            total = MultModulo(total, args[i].result, mod);
        }

        *answer = total;
    }

    free(threads);
    free(args);

    return failed ? -1 : 0;
}

int main(int argc, char **argv) {
    int tnum = -1;
    int port = -1;

    static struct option options[] = {
        {"port", required_argument, 0, 0},
        {"tnum", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

    // читаем аргументы командной строки
    while (true) {
        int option_index = 0;

        int c = getopt_long(argc, argv, "", options, &option_index);

        if (c == -1) {
            break;
        }

        if (c == '?') {
            return 1;
        }

        if (c == 0) {
            char *endptr = NULL;
            errno = 0;

            long value = strtol(optarg, &endptr, 10);

            if (errno != 0 || endptr == optarg || *endptr != '\0') {
                fprintf(stderr, "Invalid argument: %s\n", optarg);
                return 1;
            }

            if (option_index == 0) {
                if (value < 1 || value > 65535) {
                    fprintf(stderr, "Port must be from 1 to 65535\n");
                    return 1;
                }

                port = (int)value;
            } else if (option_index == 1) {
                if (value < 1 || value > 1024) {
                    fprintf(stderr, "tnum must be from 1 to 1024\n");
                    return 1;
                }

                tnum = (int)value;
            }
        }
    }

    if (port == -1 || tnum == -1) {
        fprintf(stderr, "Usage: %s --port 20001 --tnum 4\n", argv[0]);
        return 1;
    }

    // создаём tcp сокет
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (server_fd < 0) {
        perror("socket");
        return 1;
    }

    // настраиваем адрес сервера
    struct sockaddr_in server = {0};

    server.sin_family = AF_INET;
    server.sin_port = htons((uint16_t)port);
    server.sin_addr.s_addr = htonl(INADDR_ANY);

    // разрешаем повторное использование адреса
    int opt_val = 1;

    if (setsockopt(
        server_fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        &opt_val,
        sizeof(opt_val)
    ) < 0) {
        perror("setsockopt");
        close(server_fd);
        return 1;
    }

    // привязываем сокет к порту
    if (bind(
        server_fd,
        (struct sockaddr *)&server,
        sizeof(server)
    ) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }

    // начинаем прослушивать подключения
    if (listen(server_fd, 128) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Server listening at %d with %d threads\n", port, tnum);
    fflush(stdout);

    // постоянно ожидаем подключения клиентов
    while (true) {
        struct sockaddr_in client = {0};
        socklen_t client_len = sizeof(client);

        int client_fd = accept(
            server_fd,
            (struct sockaddr *)&client,
            &client_len
        );

        if (client_fd < 0) {
            if (errno == EINTR) {
                continue;
            }

            perror("accept");
            continue;
        }

        // получаем задания от подключившегося клиента
        while (true) {
            char from_client[sizeof(uint64_t) * 3];

            // читаем три числа из сокета
            int read_status = RecvAll(
                client_fd,
                from_client,
                sizeof(from_client)
            );

            if (read_status != 0) {
                if (read_status < 0) {
                    perror("recv");
                }

                break;
            }

            uint64_t begin = 0;
            uint64_t end = 0;
            uint64_t mod = 0;

            // извлекаем числа из буфера
            memcpy(
                &begin,
                from_client,
                sizeof(uint64_t)
            );

            memcpy(
                &end,
                from_client + sizeof(uint64_t),
                sizeof(uint64_t)
            );

            memcpy(
                &mod,
                from_client + 2 * sizeof(uint64_t),
                sizeof(uint64_t)
            );

            printf(
                "Receive: %" PRIu64 " %" PRIu64 " %" PRIu64 "\n",
                begin,
                end,
                mod
            );

            // проверяем корректность задания
            if (mod == 0 || begin > end) {
                fprintf(stderr, "Invalid task received\n");
                break;
            }

            uint64_t total = 0;

            // запускаем параллельное вычисление
            if (ParallelFactorial(
                begin,
                end,
                mod,
                tnum,
                &total
            ) != 0) {
                fprintf(stderr, "Parallel calculation failed\n");
                break;
            }

            printf("Total: %" PRIu64 "\n", total);

            // упаковываем результат в буфер
            char buffer[sizeof(uint64_t)];

            memcpy(buffer, &total, sizeof(total));

            // отправляем результат клиенту
            if (SendAll(client_fd, buffer, sizeof(buffer)) < 0) {
                perror("send");
                break;
            }
        }

        // закрываем соединение с клиентом
        shutdown(client_fd, SHUT_RDWR);
        close(client_fd);
    }

    close(server_fd);

    return 0;
}
