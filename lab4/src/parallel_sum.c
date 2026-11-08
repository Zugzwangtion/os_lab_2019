#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>

#include <pthread.h>

#include "utils.h"
#include "sum.h"



// функция которую будет выполнять поток
void *ThreadSum(void *args) {

    // приводим void указатель обратно к нашей структуре
    struct SumArgs *sum_args = (struct SumArgs *)args;

    // считаем сумму своего участка
    int sum = Sum(sum_args);

    // возвращаем результат работы потока
    return (void *)(intptr_t)sum;
}


int main(int argc, char **argv) {

    // проверяем что передали все нужные аргументы
    if (argc != 7) {
        printf("Usage: %s --threads_num NUM --array_size NUM --seed NUM\n",
               argv[0]);
        return 1;
    }


    // сюда сохраняем параметры запуска
    uint32_t threads_num = 0;
    uint32_t array_size = 0;
    uint32_t seed = 0;


    // читаем аргументы по два имя параметра и его значение
    for (int i = 1; i < argc; i += 2) {

        if (strcmp(argv[i], "--threads_num") == 0) {
            threads_num = (uint32_t)atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--array_size") == 0) {
            array_size = (uint32_t)atoi(argv[i + 1]);
        }
        else if (strcmp(argv[i], "--seed") == 0) {
            seed = (uint32_t)atoi(argv[i + 1]);
        }
        else {
            printf("Unknown argument: %s\n", argv[i]);
            return 1;
        }
    }


    // проверяем что значения нормальные
    if (threads_num == 0 || array_size == 0 ||
        threads_num > array_size) {

        printf("All numbers must be positive.\n");
        printf("threads_num must not exceed array_size.\n");
        return 1;
    }


    // выделяем память под весь массив
    int *array = malloc(sizeof(int) * array_size);

    if (array == NULL) {
        perror("malloc");
        return 1;
    }


    // заполняем массив числами
    // генерация не входит в замер времени
    GenerateArray(array, array_size, seed);


    // здесь будут храниться созданные потоки
    pthread_t *threads = malloc(sizeof(pthread_t) * threads_num);

    if (threads == NULL) {
        perror("malloc");
        free(array);
        return 1;
    }


    // отдельные аргументы для каждого потока
    struct SumArgs *args =
        malloc(sizeof(struct SumArgs) * threads_num);

    if (args == NULL) {
        perror("malloc");
        free(threads);
        free(array);
        return 1;
    }


    struct timeval start_time;
    struct timeval end_time;


    // начинаем засекать время подсчета
    gettimeofday(&start_time, NULL);


    // создаем нужное количество потоков
    for (uint32_t i = 0; i < threads_num; i++) {

        // все потоки работают с одним массивом
        args[i].array = array;

        // считаем границы куска для этого потока
        args[i].begin =
            i * array_size / threads_num;

        args[i].end =
            (i + 1) * array_size / threads_num;


        // создаем поток и передаем ему его аргументы
        if (pthread_create(
                &threads[i],
                NULL,
                ThreadSum,
                &args[i]) != 0) {

            printf("Error: pthread_create failed!\n");

            free(args);
            free(threads);
            free(array);

            return 1;
        }
    }


    int total_sum = 0;


    // ждем каждый поток и забираем его результат
    for (uint32_t i = 0; i < threads_num; i++) {

        void *thread_result;


        // join ждет пока поток закончит работу
        if (pthread_join(
                threads[i],
                &thread_result) != 0) {

            printf("Error: pthread_join failed!\n");

            free(args);
            free(threads);
            free(array);

            return 1;
        }


        // приводим результат обратно к числу
        int sum = (int)(intptr_t)thread_result;

        // добавляем частичную сумму к общей
        total_sum += sum;
    }


    // заканчиваем замер времени
    gettimeofday(&end_time, NULL);


    // считаем сколько прошло микросекунд
    long elapsed_time =
        (end_time.tv_sec - start_time.tv_sec) * 1000000L +
        (end_time.tv_usec - start_time.tv_usec);


    printf("Total: %d\n", total_sum);
    printf("Elapsed time: %ld microseconds\n", elapsed_time);


    // освобождаем выделенную память
    free(args);
    free(threads);
    free(array);

    return 0;
}
