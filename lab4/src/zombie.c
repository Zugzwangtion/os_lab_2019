#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

int main(void)
{
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        perror("fork");
        return 1;
    }

    if (pid == 0) {
        /* Дочерний процесс */

        printf("Дочерний процесс\n");
        printf("PID = %d\n", getpid());
        printf("Я завершаюсь...\n");

        exit(0);
    }
    else {
        /* Родительский процесс */

        printf("Родительский процесс\n");
        printf("PID родителя = %d\n", getpid());
        printf("PID дочернего = %d\n", pid);

        printf("\nДочерний процесс уже завершится.\n");
        printf("Теперь он будет зомби 20 секунд.\n");
        printf("В другом терминале выполните:\n");
        printf("ps -el | grep Z\n\n");

        sleep(20);

        printf("Теперь родитель вызывает wait().\n");

        wait(NULL);

        printf("Дочерний процесс обработан. Зомби больше нет.\n");
    }

    return 0;
}
