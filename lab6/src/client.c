
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
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>

struct Server {
    char ip[255];
    int port;
};

struct ClientArgs {
    struct Server server;
    uint64_t begin;
    uint64_t end;
    uint64_t mod;
    uint64_t result;
    int success;
};

// преобразуем строку в число
bool ConvertStringToUI64(const char *str, uint64_t *val) {
    if (str[0] == '-' || str[0] == '\0') {
        return false;
    }

    char *end = NULL;
    errno = 0;

    unsigned long long number = strtoull(str, &end, 10);

    if (errno != 0 || end == str || *end != '\0') {
        return false;
    }

    *val = (uint64_t)number;
    return true;
}

// читаем адреса серверов из файла
int LoadServers(
    const char *filename,
    struct Server **servers,
    size_t *count
) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        perror("fopen");
        return -1;
    }

    size_t capacity = 4;
    *count = 0;

    *servers = malloc(capacity * sizeof(struct Server));

    if (*servers == NULL) {
        fclose(file);
        return -1;
    }

    char line[512];

    while (fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';

        // пропускаем пустые строки и комментарии
        if (line[0] == '\0' || line[0] == '#') {
            continue;
        }

        char ip[255];
        int port;
        char extra;

        // разделяем строку на адрес и порт
        if (sscanf(line, " %254[^:]:%d %c", ip, &port, &extra) != 2 ||
            port < 1 || port > 65535) {
            fprintf(stderr, "Invalid server: %s\n", line);

            fclose(file);
            free(*servers);
            *servers = NULL;

            return -1;
        }

        // увеличиваем массив если места недостаточно
        if (*count == capacity) {
            size_t new_capacity = capacity * 2;

            struct Server *new_servers = realloc(
                *servers,
                new_capacity * sizeof(struct Server)
            );

            if (new_servers == NULL) {
                fclose(file);
                free(*servers);
                *servers = NULL;

                return -1;
            }

            *servers = new_servers;
            capacity = new_capacity;
        }

        strcpy((*servers)[*count].ip, ip);
        (*servers)[*count].port = port;

        (*count)++;
    }

    fclose(file);

    if (*count == 0) {
        fprintf(stderr, "No servers found\n");

        free(*servers);
        *servers = NULL;

        return -1;
    }

    return 0;
}

// функция потока для работы с одним сервером
void *ServerWorker(void *arg) {
    struct ClientArgs *task = (struct ClientArgs *)arg;

    task->success = 0;

    // если диапазон пустой результат равен единице по модулю
    if (task->begin > task->end) {
        task->result = 1 % task->mod;
        task->success = 1;
        return NULL;
    }

    char port_string[16];

    snprintf(
        port_string,
        sizeof(port_string),
        "%d",
        task->server.port
    );

    // получаем сетевой адрес сервера
    struct addrinfo hints = {0};
    struct addrinfo *addresses = NULL;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int err = getaddrinfo(
        task->server.ip,
        port_string,
        &hints,
        &addresses
    );

    if (err != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        return NULL;
    }

    // создаём сокет и подключаемся к серверу
    int sck = -1;

    for (struct addrinfo *p = addresses; p != NULL; p = p->ai_next) {
        sck = socket(
            p->ai_family,
            p->ai_socktype,
            p->ai_protocol
        );

        if (sck < 0) {
            continue;
        }

        if (connect(sck, p->ai_addr, p->ai_addrlen) == 0) {
            break;
        }

        close(sck);
        sck = -1;
    }

    freeaddrinfo(addresses);

    if (sck < 0) {
        fprintf(
            stderr,
            "Connection failed: %s:%d\n",
            task->server.ip,
            task->server.port
        );

        return NULL;
    }

    // упаковываем три числа в сообщение
    char buffer[sizeof(uint64_t) * 3];

    memcpy(
        buffer,
        &task->begin,
        sizeof(uint64_t)
    );

    memcpy(
        buffer + sizeof(uint64_t),
        &task->end,
        sizeof(uint64_t)
    );

    memcpy(
        buffer + 2 * sizeof(uint64_t),
        &task->mod,
        sizeof(uint64_t)
    );

    // отправляем задание серверу
    if (SendAll(sck, buffer, sizeof(buffer)) < 0) {
        fprintf(stderr, "Send failed\n");
        close(sck);
        return NULL;
    }

    // получаем результат от сервера
    char response[sizeof(uint64_t)];

    if (RecvAll(sck, response, sizeof(response)) < 0) {
        fprintf(stderr, "Receive failed\n");
        close(sck);
        return NULL;
    }

    memcpy(
        &task->result,
        response,
        sizeof(uint64_t)
    );

    task->success = 1;

    close(sck);

    return NULL;
}

int main(int argc, char **argv) {
    uint64_t k = 0;
    uint64_t mod = 0;

    bool has_k = false;
    bool has_mod = false;

    char servers_file[255] = {0};

    static struct option options[] = {
        {"k", required_argument, 0, 0},
        {"mod", required_argument, 0, 0},
        {"servers", required_argument, 0, 0},
        {0, 0, 0, 0}
    };

    // читаем аргументы командной строки
    while (true) {
        int option_index = 0;

        int c = getopt_long(
            argc,
            argv,
            "",
            options,
            &option_index
        );

        if (c == -1) {
            break;
        }

        if (c == '?') {
            return 1;
        }

        if (c == 0) {
            switch (option_index) {
                case 0:
                    has_k = ConvertStringToUI64(optarg, &k);
                    break;

                case 1:
                    has_mod = ConvertStringToUI64(optarg, &mod);
                    break;

                case 2:
                    if (strlen(optarg) >= sizeof(servers_file)) {
                        fprintf(stderr, "Server file path too long\n");
                        return 1;
                    }

                    strcpy(servers_file, optarg);
                    break;
            }
        }
    }

    // проверяем обязательные аргументы
    if (!has_k || !has_mod || mod == 0 || servers_file[0] == '\0') {
        fprintf(
            stderr,
            "Usage: %s --k 1000 --mod 17 --servers servers.txt\n",
            argv[0]
        );

        return 1;
    }

    // загружаем список серверов
    struct Server *servers = NULL;
    size_t servers_num = 0;

    if (LoadServers(servers_file, &servers, &servers_num) < 0) {
        return 1;
    }

    printf("Loaded servers: %zu\n", servers_num);

    // выделяем память под потоки и их задания
    pthread_t *threads = calloc(
        servers_num,
        sizeof(pthread_t)
    );

    struct ClientArgs *tasks = calloc(
        servers_num,
        sizeof(struct ClientArgs)
    );

    if (threads == NULL || tasks == NULL) {
        free(threads);
        free(tasks);
        free(servers);
        return 1;
    }

    // делим диапазон между серверами
    uint64_t base = k / servers_num;
    uint64_t remainder = k % servers_num;

    uint64_t current = 1;

    size_t created = 0;
    bool failed = false;

    for (size_t i = 0; i < servers_num; i++) {
        uint64_t length = base;

        if (i < remainder) {
            length++;
        }

        tasks[i].server = servers[i];
        tasks[i].mod = mod;

        if (length == 0) {
            tasks[i].begin = 1;
            tasks[i].end = 0;
        } else {
            tasks[i].begin = current;
            tasks[i].end = current + length - 1;

            current += length;
        }

        printf(
            "Server %zu (%s:%d): %" PRIu64 " - %" PRIu64 "\n",
            i + 1,
            servers[i].ip,
            servers[i].port,
            tasks[i].begin,
            tasks[i].end
        );

        // запускаем поток для каждого сервера
        int err = pthread_create(
            &threads[i],
            NULL,
            ServerWorker,
            &tasks[i]
        );

        if (err != 0) {
            fprintf(stderr, "pthread_create: %s\n", strerror(err));
            failed = true;
            break;
        }

        created++;
    }

    // ждём завершения всех потоков
    for (size_t i = 0; i < created; i++) {
        int err = pthread_join(threads[i], NULL);

        if (err != 0 || !tasks[i].success) {
            failed = true;
        }
    }

    if (failed) {
        fprintf(stderr, "Calculation failed");

        free(threads);
        free(tasks);
        free(servers);

        return 1;
    }

    // объединяем частичные результаты
    uint64_t answer = 1 % mod;

    for (size_t i = 0; i < servers_num; i++) {
        printf(
            "Partial result %zu: %" PRIu64 "\n",
            i + 1,
            tasks[i].result
        );

        answer = MultModulo(
            answer,
            tasks[i].result,
            mod
        );
    }

    printf("Answer: %" PRIu64 "\n", answer);

    free(threads);
    free(tasks);
    free(servers);

    return 0;
}
