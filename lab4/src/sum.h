#ifndef SUM_H
#define SUM_H

// информация о куске массива для потока
struct SumArgs {
    int *array;
    int begin;
    int end;
};

// считает сумму указанного участка массива
int Sum(const struct SumArgs *args);

#endif
