#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>

typedef struct {
    unsigned long long start;
    unsigned long long end;
    unsigned long long mod;
} ThreadData;

unsigned long long result = 1;

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void *factorial_part(void *arg) {
    ThreadData *data = (ThreadData *)arg;

    unsigned long long partial = 1;

    for (unsigned long long i = data->start; i <= data->end; i++) {
        partial = (partial * i) % data->mod;
    }

    pthread_mutex_lock(&mutex);

    result = (result * partial) % data->mod;

    pthread_mutex_unlock(&mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    unsigned long long k = 0;
    unsigned long long mod = 0;
    int pnum = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-k") == 0 && i + 1 < argc) {
            k = strtoull(argv[++i], NULL, 10);
        }
        else if (strncmp(argv[i], "--pnum=", 7) == 0) {
            pnum = atoi(argv[i] + 7);
        }
        else if (strncmp(argv[i], "--mod=", 6) == 0) {
            mod = strtoull(argv[i] + 6, NULL, 10);
        }
    }

    if (k == 0 || pnum <= 0 || mod == 0) {
        printf("Usage: %s -k NUMBER --pnum=N --mod=N\n", argv[0]);
        return 1;
    }


    pthread_t *threads = malloc(pnum * sizeof(pthread_t));
    ThreadData *data = malloc(pnum * sizeof(ThreadData));

    if (threads == NULL || data == NULL) {
        printf("Memory allocation error\n");
        free(threads);
        free(data);
        return 1;
    }

    unsigned long long base = k / pnum;
    unsigned long long remainder = k % pnum;
    unsigned long long start = 1;

    for (int i = 0; i < pnum; i++) {
        unsigned long long count = base + ((unsigned long long)i < remainder ? 1 : 0);

        data[i].start = start;
        data[i].end = start + count - 1;
        data[i].mod = mod;

        pthread_create(&threads[i], NULL, factorial_part, &data[i]);

        start += count;
    }


    for (int i = 0; i < pnum; i++) {
        pthread_join(threads[i], NULL);
    }

    printf("Result: %llu\n", result);

    free(threads);
    free(data);

    pthread_mutex_destroy(&mutex);

    return 0;
}
