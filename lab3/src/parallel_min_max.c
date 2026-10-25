#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <getopt.h>

#include "find_min_max.h"
#include "utils.h"

int main(int argc, char **argv) { //argc — количество аргументов командной строки.
    int seed = -1;                //argv — массив строк с аргументами командной строки.
    int array_size = -1;
    int pnum = -1;
    bool with_files = false;

    // Разбираем аргументы командной строки
    while (true) {
        static struct option options[] = {  //задаем распознаюзиеся параметры 
            {"seed", required_argument, 0, 0}, // required_argument - обязательное поле
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'}, //no_argument не обязательное
            {0, 0, 0, 0} //завершающая запись
        };

        int option_index = 0; //индекс параметра из строки 

        int c = getopt_long(argc, argv, "f", options, &option_index);
        //если функция вернет 0 используем option_index
        //чтобы выяснить какой  длинный параметр распознан

        if (c == -1) { //когда обработаны все параметры getopt выдаст -1
            break;
        }

        switch (c) {
            case 0: //распознан один из длин параметров 
                switch (option_index) { //определяем какой именно 
                    case 0:
                        seed = atoi(optarg); //преобразуем из строки число
                        break;

                    case 1: //индексы opt ьерутся из структуры
                        array_size = atoi(optarg);
                        break;

                    case 2:
                        pnum = atoi(optarg);
                        break;

                    default:
                        printf("Unknown option\n");
                        return 1;
                }
                break;

            case 'f':
                with_files = true;
                break;

            case '?':
                printf("Unknown argument\n");
                return 1;

            default:
                return 1;
        }
    }

    // Проверяем входные параметры
    if (optind < argc || //обработка лишних аргументов 
        seed <= 0 ||     // обработка некорректных аргументов
        array_size <= 0 || 
        pnum <= 0 ||
        pnum > array_size) {

        printf("Usage: %s --seed NUM --array_size NUM --pnum NUM [--by_files]\n",
               argv[0]);

        printf("NUM must be positive, and pnum must not exceed array_size.\n");

        return 1;
    }

    // Создаём массив
    int *array = malloc(sizeof(int) * array_size); //выделяем память 

    if (array == NULL) { //если памяти нет возвращаем 0 
        perror("malloc");
        return 1;
    }

    GenerateArray(array, array_size, seed); //создаем массив дл форка

    // Запоминаем PID родительского процесса
    pid_t parent_pid = getpid();

    // Храним PID всех дочерних процессов
    pid_t *child_pids = malloc(sizeof(pid_t) * pnum); //getpid() возвращает PID текущего процесса (который выполняет эту строку)

    if (child_pids == NULL) { //проверяем выделилась ли память для хранения id
        perror("malloc");
        free(array);
        return 1;
    }

    // Для режима pipe создаём отдельный канал каждому ребёнку
    int (*pipes)[2] = NULL; //pipes - указатель на массивы из 2 эдементо тк
    //pipes[i][0] - для чтения а с [1] - для записи 

    if (!with_files) {
        pipes = malloc(sizeof(int[2]) * pnum); 

        if (pipes == NULL) {
            perror("malloc");
            free(child_pids);
            free(array);
            return 1;
        }

        for (int i = 0; i < pnum; i++) {
            if (pipe(pipes[i]) == -1) { //обработка ошибки создания канала
                perror("pipe");

                for (int j = 0; j < i; j++) { 
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                free(pipes);  //освобождаем выделенную память тк была оштбка и оно нам не надо
                free(child_pids);
                free(array);

                return 1;
            }
        }
    }

    struct timeval start_time; //для засекания времени 
    gettimeofday(&start_time, NULL); //календарное время записываем в структуру

    // Создаём дочерние процессы
    int created = 0; //количесто созданных процессов

    for (int i = 0; i < pnum; i++) {
        pid_t child_pid = fork(); //создаем дочерний процесс
        // форк в родительском поцессе возвращает id ребенка
        //в дочернем процессе он возвращает 0


        if (child_pid == -1) {
            perror("fork");
            break;
        }

        if (child_pid == 0) { 
            // Код дочернего процесса

            // Закрываем ненужные концы каналов
            if (!with_files) {
                for (int j = 0; j < pnum; j++) {
                    close(pipes[j][0]);

                    if (j != i) {
                        close(pipes[j][1]);
                    }
                }
            }

            // Вычисляем границы участка массива
            int begin = i * array_size / pnum;
            int end = (i + 1) * array_size / pnum;

            // Находим минимум и максимум своего участка
            struct MinMax result = GetMinMax(array, begin, end);

            if (with_files) {
                // Режим передачи результатов через файлы
                char filename[128]; //резервируем 128 символов для названия файла

                snprintf(filename, sizeof(filename),
                         "/tmp/parallel_min_max_%ld_%d.txt",
                         (long)parent_pid, i);

                FILE *file = fopen(filename, "w");

                if (file == NULL) {
                    perror("fopen");
                    _exit(1);
                }

                fprintf(file, "%d %d\n", result.min, result.max);

                if (fclose(file) != 0) {
                    perror("fclose");
                    _exit(1);
                }
            } else {
                // Режим передачи результатов через pipe
                ssize_t written = write(
                    pipes[i][1],
                    &result,
                    sizeof(result)
                );

                close(pipes[i][1]);

                if (written != (ssize_t)sizeof(result)) { //совпадает ли количество записанных байтов с размером структуры
                    perror("write");
                    _exit(1);
                }
            }

            // Завершаем дочерний процесс
            _exit(0);
        }

        // Этот код выполняется только родителем
        child_pids[created] = child_pid;
        created++;
    }

    // Родитель закрывает каналы для записи тк дочерние процессы уже передали результат выше
    if (!with_files) {
        for (int i = 0; i < pnum; i++) {
            close(pipes[i][1]);
        }
    }

    // Ждём завершения созданных дочерних процессов
    bool children_ok = true;

    for (int i = 0; i < created; i++) {
        int status; //статус завершения дочернего процесса

        if (waitpid(child_pids[i], &status, 0) == -1) { //waitpid докладывает о завершении в status
            perror("waitpid");
            children_ok = false;
            continue;
        }

        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            printf("Child process %d failed\n", i);
            children_ok = false;
        }
    }

    if (created != pnum) {
        children_ok = false;
    }

    // Общие минимум и максимум
    struct MinMax min_max;
    min_max.min = INT_MAX;
    min_max.max = INT_MIN;

    // Собираем результаты дочерних процессов
    for (int i = 0; i < created; i++) {
        struct MinMax result;
        bool result_ok = false;

        if (with_files) {
            // Читаем результат из файла
            char filename[128];

            snprintf(filename, sizeof(filename),
                     "/tmp/parallel_min_max_%ld_%d.txt",
                     (long)parent_pid, i);

            FILE *file = fopen(filename, "r");

            if (file != NULL) {
                if (fscanf(file, "%d %d",
                           &result.min, &result.max) == 2) {
                    result_ok = true;
                }

                fclose(file);
            }

            // Удаляем временный файл
            unlink(filename);
        } else {
            // Читаем результат из pipe
            ssize_t received = read(
                pipes[i][0],
                &result,
                sizeof(result)
            );

            if (received == (ssize_t)sizeof(result)) {
                result_ok = true;
            }

            close(pipes[i][0]);
        }

        if (!result_ok) {
            printf("Failed to receive result from child %d\n", i);
            children_ok = false;
            continue;
        }

        if (result.min < min_max.min) {
            min_max.min = result.min;
        }

        if (result.max > min_max.max) {
            min_max.max = result.max;
        }
    }

    struct timeval finish_time;
    gettimeofday(&finish_time, NULL);

    double elapsed_time =
        (finish_time.tv_sec - start_time.tv_sec) * 1000.0;

    elapsed_time +=
        (finish_time.tv_usec - start_time.tv_usec) / 1000.0;

    free(array);
    free(child_pids);
    free(pipes);

    if (!children_ok) {
        return 1;
    }

    printf("Min: %d\n", min_max.min);
    printf("Max: %d\n", min_max.max);
    printf("Elapsed time: %fms\n", elapsed_time);

    return 0;
}

