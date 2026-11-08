#include "sum.h"

// считаем сумму своего куска массива
int Sum(const struct SumArgs *args) {
    int sum = 0;

    // идем по своему участку массива и складываем числа
    for (int i = args->begin; i < args->end; i++) {
        sum += args->array[i];
    }

    return sum;
}

