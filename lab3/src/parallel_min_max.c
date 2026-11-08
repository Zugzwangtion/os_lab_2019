#include <ctype.h>
#include <limits.h>
#include <stdbool.h>
#include <signal.h>
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


/*
 * Этот флаг изменяется обработчиком сигнала SIGALRM.
 *
 * volatile sig_atomic_t используется для переменных,
 * которые могут изменяться обработчиком сигнала.
 */
volatile sig_atomic_t timeout_expired = 0;


/* Обработчик сигнала SIGALRM */
void alarm_handler(int signum)
{
    (void)signum;

    timeout_expired = 1;
}


int main(int argc, char **argv)
{
    int seed = -1;
    int array_size = -1;
    int pnum = -1;

    /*
     * timeout = 0 означает:
     * таймаут не используется.
     */
    int timeout = 0;

    bool with_files = false;


    /* Разбираем аргументы командной строки */
    while (true) {

        static struct option options[] = {
            {"seed", required_argument, 0, 0},
            {"array_size", required_argument, 0, 0},
            {"pnum", required_argument, 0, 0},
            {"timeout", required_argument, 0, 0},
            {"by_files", no_argument, 0, 'f'},
            {0, 0, 0, 0}
        };

        int option_index = 0;

        int c = getopt_long(
            argc,
            argv,
            "f",
            options,
            &option_index
        );

        if (c == -1) {
            break;
        }

        switch (c) {

            case 0:

                switch (option_index) {

                    case 0:
                        seed = atoi(optarg);
                        break;

                    case 1:
                        array_size = atoi(optarg);
                        break;

                    case 2:
                        pnum = atoi(optarg);
                        break;

                    case 3:
                        timeout = atoi(optarg);
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


    /* Проверяем входные параметры */
    if (optind < argc ||
        seed <= 0 ||
        array_size <= 0 ||
        pnum <= 0 ||
        pnum > array_size ||
        timeout < 0) {

        printf(
            "Usage: %s "
            "--seed NUM "
            "--array_size NUM "
            "--pnum NUM "
            "[--timeout NUM] "
            "[--by_files]\n",
            argv[0]
        );

        printf(
            "NUM must be positive, "
            "pnum must not exceed array_size.\n"
        );

        return 1;
    }


    /* Создаём массив */
    int *array = malloc(sizeof(int) * array_size);

    if (array == NULL) {
        perror("malloc");
        return 1;
    }


    GenerateArray(array, array_size, seed);


    /* Запоминаем PID родительского процесса */
    pid_t parent_pid = getpid();


    /* Храним PID дочерних процессов */
    pid_t *child_pids = malloc(sizeof(pid_t) * pnum);

    if (child_pids == NULL) {
        perror("malloc");
        free(array);
        return 1;
    }


    /*
     * Для режима pipe создаём отдельный канал
     * для каждого дочернего процесса.
     */
    int (*pipes)[2] = NULL;

    if (!with_files) {

        pipes = malloc(sizeof(int[2]) * pnum);

        if (pipes == NULL) {
            perror("malloc");

            free(child_pids);
            free(array);

            return 1;
        }


        for (int i = 0; i < pnum; i++) {

            if (pipe(pipes[i]) == -1) {

                perror("pipe");

                for (int j = 0; j < i; j++) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }

                free(pipes);
                free(child_pids);
                free(array);

                return 1;
            }
        }
    }


    /* Запоминаем время начала работы */
    struct timeval start_time;

    gettimeofday(&start_time, NULL);


    /*
     * Устанавливаем обработчик SIGALRM.
     *
     * Когда истечёт время alarm(),
     * ядро отправит родительскому процессу SIGALRM.
     */
    if (timeout > 0) {

        if (signal(SIGALRM, alarm_handler) == SIG_ERR) {
            perror("signal");

            if (pipes != NULL) {
                for (int i = 0; i < pnum; i++) {
                    close(pipes[i][0]);
                    close(pipes[i][1]);
                }

                free(pipes);
            }

            free(child_pids);
            free(array);

            return 1;
        }

        /*
         * Запускаем таймер.
         *
         * Через timeout секунд родитель получит SIGALRM.
         */
        alarm(timeout);
    }


    /* Создаём дочерние процессы */
    int created = 0;


    for (int i = 0; i < pnum; i++) {

        pid_t child_pid = fork();

        if (child_pid == -1) {
            perror("fork");
            break;
        }


        if (child_pid == 0) {

            /* ========================= */
            /*     ДОЧЕРНИЙ ПРОЦЕСС      */
            /* ========================= */


            /*
             * Дочернему процессу таймер родителя
             * не нужен.
             */
            alarm(0);


            /* Закрываем ненужные концы pipe */
            if (!with_files) {

                for (int j = 0; j < pnum; j++) {

                    close(pipes[j][0]);

                    if (j != i) {
                        close(pipes[j][1]);
                    }
                }
            }


            /* Вычисляем границы участка массива */
            int begin = i * array_size / pnum;
            int end = (i + 1) * array_size / pnum;


            /* Находим минимум и максимум */
            struct MinMax result =
                GetMinMax(array, begin, end);
	    sleep(3);


            if (with_files) {

                /* ===== Передача через файл ===== */

                char filename[128];

                snprintf(
                    filename,
                    sizeof(filename),
                    "/tmp/parallel_min_max_%ld_%d.txt",
                    (long)parent_pid,
                    i
                );


                FILE *file = fopen(filename, "w");

                if (file == NULL) {
                    perror("fopen");
                    _exit(1);
                }


                fprintf(
                    file,
                    "%d %d\n",
                    result.min,
                    result.max
                );


                if (fclose(file) != 0) {
                    perror("fclose");
                    _exit(1);
                }

            } else {

                /* ===== Передача через pipe ===== */

                ssize_t written = write(
                    pipes[i][1],
                    &result,
                    sizeof(result)
                );


                close(pipes[i][1]);


                if (written !=
                    (ssize_t)sizeof(result)) {

                    perror("write");

                    _exit(1);
                }
            }


            /* Завершаем дочерний процесс */
            _exit(0);
        }


        /* ========================= */
        /*    РОДИТЕЛЬСКИЙ ПРОЦЕСС   */
        /* ========================= */

        child_pids[created] = child_pid;

        created++;
    }


    /*
     * Родитель больше не записывает данные в pipe.
     */
    if (!with_files) {

        for (int i = 0; i < created; i++) {
            close(pipes[i][1]);
        }
    }


    /* Ждём завершения дочерних процессов */
    bool children_ok = true;

    int finished = 0;


    /*
     * Неблокирующее ожидание.
     *
     * WNOHANG означает:
     * "не ждать завершения процесса".
     */
    while (finished < created && !timeout_expired) {

        for (int i = 0; i < created; i++) {

            if (child_pids[i] <= 0) {
                continue;
            }


            int status;

            pid_t result = waitpid(
                child_pids[i],
                &status,
                WNOHANG
            );


            if (result == child_pids[i]) {

                finished++;

                child_pids[i] = -1;


                if (!WIFEXITED(status) ||
                    WEXITSTATUS(status) != 0) {

                    children_ok = false;
                }
            }

            else if (result == -1) {

                perror("waitpid");

                child_pids[i] = -1;

                finished++;

                children_ok = false;
            }
        }


        /*
         * Если процессы ещё работают,
         * немного ждём перед следующей проверкой.
         */
        if (finished < created &&
            !timeout_expired) {

            usleep(1000);
        }
    }


    /*
     * Если время истекло —
     * убиваем незавершившиеся дочерние процессы.
     */
    if (timeout_expired) {

        printf(
            "Timeout expired. "
            "Terminating child processes...\n"
        );


        children_ok = false;


        for (int i = 0; i < created; i++) {

            if (child_pids[i] > 0) {

                if (kill(
                        child_pids[i],
                        SIGKILL
                    ) == -1) {

                    /*
                     * ESRCH означает:
                     * процесс уже завершился.
                     */
                }
            }
        }


        /*
         * После SIGKILL обязательно
         * забираем завершившихся детей.
         *
         * Иначе могут появиться zombie-процессы.
         */
        for (int i = 0; i < created; i++) {

            if (child_pids[i] > 0) {

                int status;

                waitpid(
                    child_pids[i],
                    &status,
                    0
                );

                child_pids[i] = -1;
            }
        }
    }


    /*
     * Отменяем alarm(), если он ещё не сработал.
     */
    if (timeout > 0) {
        alarm(0);
    }


    /*
     * Если все дочерние процессы
     * завершились успешно —
     * читаем результаты.
     */
    if (children_ok) {

        struct MinMax total_result;

        total_result.min = INT_MAX;
        total_result.max = INT_MIN;


        for (int i = 0; i < created; i++) {

            struct MinMax result;


            if (with_files) {

                char filename[128];

                snprintf(
                    filename,
                    sizeof(filename),
                    "/tmp/parallel_min_max_%ld_%d.txt",
                    (long)parent_pid,
                    i
                );


                FILE *file = fopen(
                    filename,
                    "r"
                );


                if (file == NULL) {
                    perror("fopen");

                    children_ok = false;

                    break;
                }


                if (fscanf(
                        file,
                        "%d %d",
                        &result.min,
                        &result.max
                    ) != 2) {

                    printf(
                        "Error reading result "
                        "from file\n"
                    );

                    fclose(file);

                    children_ok = false;

                    break;
                }


                fclose(file);

                unlink(filename);

            } else {

                ssize_t bytes_read = read(
                    pipes[i][0],
                    &result,
                    sizeof(result)
                );


                close(pipes[i][0]);


                if (bytes_read !=
                    (ssize_t)sizeof(result)) {

                    perror("read");

                    children_ok = false;

                    break;
                }
            }


            if (result.min <
                total_result.min) {

                total_result.min =
                    result.min;
            }


            if (result.max >
                total_result.max) {

                total_result.max =
                    result.max;
            }
        }


        if (children_ok) {

            struct timeval finish_time;

            gettimeofday(
                &finish_time,
                NULL
            );


            long long elapsed_time =

                (finish_time.tv_sec -
                 start_time.tv_sec) * 1000000LL

                +

                (finish_time.tv_usec -
                 start_time.tv_usec);


            printf(
                "Min: %d\n",
                total_result.min
            );

            printf(
                "Max: %d\n",
                total_result.max
            );

            printf(
                "Elapsed time: "
                "%lld microseconds\n",
                elapsed_time
            );
        }
    }


    /*
     * Закрываем оставшиеся pipe.
     */
    if (!with_files && pipes != NULL) {

        for (int i = 0; i < created; i++) {
            close(pipes[i][0]);
        }

        free(pipes);
    }


    free(child_pids);
    free(array);


    return children_ok ? 0 : 1;
}
