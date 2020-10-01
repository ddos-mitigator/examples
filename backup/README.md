# Резервное копирование и восстановление

Для запуска необходимо:
* `python` версии 3.5 и выше
* Утилита `curl`
* Возможность создания пользователем каталогов на FTP/SFTP-сервере
* Права суперпользователя либо права на запись в каталог `/var/lib/docker`

## Резервное копирование

Запуск:

`./backup.py [список глобальных опций] <команда> [список опций команды]`

Глобальные опции:

* `--backup_workdir`: флаг, если выставлен — сохранять содержимое рабочего каталога;
* `--backup_data`: флаг, если выставлен — сохранять данные;
* `--backup_metrics`: флаг, если выставлен — сохранять метрики;
* `--dst_dir`: конечная директория (в ней будет создан каталог с датой создания резервной копии);
* `--workdir`: рабочий каталог.

Команды:

* `dir`: сохранить бэкап в локальную директорию;
* `ftp`: отправить бэкап через FTP;
* `sftp`: отправить бэкап через SFTP.

Опции команд ftp/sftp:

* `--host`: адрес сервера;
* `--user`: имя пользователя для подключения к серверу;
* `--password`: пароль для подключения к серверу.

### Ежедневное резервное копирование

При необходимости можно добавить выполнение резервного копирования по таймеру.

Пример с использованием `cron`:

* Сделать скрипт исполняемым:

  `chmod +x /mitigator_backups/backup.py`

* Запустить редактор событий `cron`:

  `crontab -e`

* Добавить строку с запуском скрипта каждый день в `03:00`:

  `0 3 * * * /mitigator_backups/backup.py --workdir /srv/mitigator --dst_dir /mitigator_backups --backup_data dir`

## Восстановление

Условия:
* Предварительно настроенная для запуска митигатора система, либо система с уже настроенным, но остановленным митигатором
* Существующий или заранее созданный каталог с конфигурационными файлами (далее - рабочий каталог), по умолчанию и далее в инструкции `/srv/mitigator`

Перед началом восстановления для удобства можно задать переменную с директорией восстанавливаемой резервной копии, например:

`BACKUP_DIR=/tmp/2022-01-01`

Далее предполагается наличие установленной переменной `$BACKUP_DIR`.

### Конфигурация

Распаковать архив `workdir.tar.gz` в рабочий каталог:

`tar -xvzf $BACKUP_DIR/workdir.tar.gz -C /srv/mitigator`

### Метрики

* Запустить Clickhouse:

  `docker-compose up -d clickhouse && docker-compose logs -f clickhouse`

  Дождаться сообщения `/entrypoint.sh: running /docker-entrypoint-initdb.d/03-create-graphite_tagged-table.sql`, нажать `Ctrl+C`.

* Распаковать архив `metrics.tar.gz` в том контейнера (требуются права на запись в `/var/lib/docker`):

  `mkdir -p /var/lib/docker/volumes/mitigator_clickhouse/_data/backup && tar -xvzf $BACKUP_DIR/metrics.tar.gz -C /var/lib/docker/volumes/mitigator_clickhouse/_data/backup`

* Переместить данные архива в системные директории `clickhouse` внутри контейнера:

  `docker-compose exec clickhouse bash -c 'cd /var/lib/clickhouse/backup; for dir in *; do cp -r $dir/* /var/lib/clickhouse/data/default/$dir/detached; done'`

* Выдать права пользователю внутри контейнера:

  `docker-compose exec clickhouse chown -R clickhouse:clickhouse /var/lib/clickhouse/data/default`

* Восстановить данные:

  `docker-compose exec clickhouse clickhouse-client --format=TSVRaw -q"select 'ALTER TABLE ' || database || '.' || table || ' ATTACH PARTITION ID \'' || partition_id || '\';\n' from system.detached_parts group by database, table, partition_id order by database, table, partition_id;" | docker-compose exec -T clickhouse clickhouse-client -nm`

### Данные

* Запустить Postgres:

  `docker-compose up -d postgres && docker-compose logs -f postgres`

  Дождаться сообщения `database system is ready to accept connections`, нажать `Ctrl+C`.

* Распаковать архив `data.tar.gz` в том контейнера (требуются права на запись в `/var/lib/docker`):

  `tar --strip-components=1 -xvzf $BACKUP_DIR/data.tar.gz -C /var/lib/docker/volumes/mitigator_postgres/_data`

* Выдать права пользователю внутри контейнера:

  `docker-compose exec postgres chown postgres:postgres /var/lib/postgresql/data.sql`

* Восстановить данные:

  `docker-compose exec postgres sh -c 'psql mitigator </var/lib/postgresql/data.sql >/tmp/restore.log'`

* Переместить данные экземпляра в том:

  `mkdir -p /var/lib/docker/volumes/mitigator_own_id/_data && mv /var/lib/docker/volumes/mitigator_postgres/_data/{log.go,own_id.conf} /var/lib/docker/volumes/mitigator_own_id/_data`
