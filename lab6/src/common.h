#ifndef COMMON_H
#define COMMON_H

#include <stdint.h>
#include <stddef.h>

// складываем два числа по модулю
uint64_t AddModulo(uint64_t a, uint64_t b, uint64_t mod);

// умножаем два числа по модулю
uint64_t MultModulo(uint64_t a, uint64_t b, uint64_t mod);

// отправляем все байты сообщения
int SendAll(int fd, const void *buffer, size_t size);

// получаем все байты сообщения
int RecvAll(int fd, void *buffer, size_t size);

#endif
