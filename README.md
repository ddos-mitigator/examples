# Скрипты и программы для BIFIT MITIGATOR

Вопросы, комментарии, предложения? Откройте Issue!

Код распространяется по лицензии BSD-3-Clause.


## [Скрипт резервного копирования](backup)


## Программы BPF

* [`Makefile`](Makefile) — команды для сборки
    (скачает [`mitigator_bpf.h`](https://docs.mitigator.ru/kb/bpf/mitigator_bpf.h)).
* [Документация](https://docs.mitigator.ru/kb/bpf/)
    по возможностям BPF и полное описание API.


## [`tcpopt.c`](tcpopt.c): фильтр по TCP MSS

Программа-пример сбрасывает SYN и SYN+ACK со значением MSS меньше заданного
через параметры.


## [`ompc.c`](ompc.c): фильтр по байтовой маске

Программа позволяет сбрасывать пакеты, у которых по определенному смещению
от начала заголовка IPv4 находятся заданные байты (Offset-Mask-Pattern
Criteria, OMPC). Такие шаблоны используются иногда в описаниях атак,
программа позволяет воспользоваться ими. Также OMPC позволяет искать
байты в заголовках L3 и L4.

На данные пакета накладывается маска и результат сравнивается с шаблоном,
например, чтобы сбросить пакет с `ABC` по смещению 44: offset=44,
mask=FFFFFF00 (три байта из четырех), pattern=41424300 (41 — код символа `A`
в шестнадцатеричной системе). Чтобы настраивать эти параметры, достаточно
передать смещение, маску и шаблон скрипту:

```
./ompc.py 44 FFFFFF00 41424300
```

Результат для указания в параметрах через UI: `2c000000ffffff0041414300`


## [`http.c`](http.c): пример защиты L7

В качестве примера, как с помощью BPF можно реализовать защиту L7,
реализован простейший вариант контрмеры HTTP — схема challenge-response
на базе HTTP 307 Redirect.


## [`router.c`](router.c): простейший роутер

Программа позволяет использовать схему включения MITIGATOR´а,
когда в защищаемой сети расположено несколько маршрутизаторов,
и стандартной схемы включения «L3-роутер» недостаточно.

Параметрами задается таблица маршрутизации, а программа меняет MAC назначения.
Формировать параметры помогает скрипт:

```
$ ./router.py -r 192.0.2.0/25=02:01:aa:aa:aa:aa -r 0.0.0.0/0=02:01:bb:bb:bb:bb
00000000
02000000
c0000200 ffffff80 0201aaaaaaaa 0000
00000000 00000000 0201bbbbbbbb 0000
```

Порядок правил должен быть о более к менее специфичному. Если не задан
маршрут по умолчанию, ключ `-d` позволяет задать действие для пакетов,
не идущих ни в какую из перечисленных сетей.


# BIFIT MITIGATOR Code Hub

This repository contains sample code and useful scripts
for [BIFIT MITIGATOR](https://mitigator.ru) anti-DDoS software suite.

Currently, the bulk of the materials is in Russian, translation coming soon.

License: BSD-3-Clause
