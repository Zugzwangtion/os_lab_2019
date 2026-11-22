#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

// создаем два мьютекса
pthread_mutex_t mutex1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex2 = PTHREAD_MUTEX_INITIALIZER;

// функция первого потока
void *thread_func1(void *arg) {
    (void)arg;

    // захватываем первый мьютекс
    pthread_mutex_lock(&mutex1);
    printf("Thread 1: mutex1 locked\n");

    // даем второму потоку время захватить mutex2
    sleep(1);

    // пытаемся захватить второй мьютекс
    printf("Thread 1: waiting for mutex2\n");
    pthread_mutex_lock(&mutex2);

    // до этих строк при deadlock выполнение не дойдет
    pthread_mutex_unlock(&mutex2);
    pthread_mutex_unlock(&mutex1);

    return NULL;
}

// функция второго потока
void *thread_func2(void *arg) {
    (void)arg;

    // захватываем второй мьютекс
    pthread_mutex_lock(&mutex2);
    printf("Thread 2: mutex2 locked\n");

    // даем первому потоку время захватить mutex1
    sleep(1);

    // пытаемся захватить первый мьютекс
    printf("Thread 2: waiting for mutex1\n");
    pthread_mutex_lock(&mutex1);

    // до этих строк при deadlock выполнение не дойдет
    pthread_mutex_unlock(&mutex1);
    pthread_mutex_unlock(&mutex2);

    return NULL;
}

int main() {
    pthread_t thread1, thread2;

    // создаем два потока
    pthread_create(&thread1, NULL, thread_func1, NULL);
    pthread_create(&thread2, NULL, thread_func2, NULL);

    // ожидаем завершения потоков
    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    printf("Program finished\n");

    return 0;
}
