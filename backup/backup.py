#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>

import argparse
import ftplib
import os
from datetime import date
from tempfile import TemporaryDirectory
from pathlib import Path
from shutil import rmtree
from subprocess import run, CalledProcessError


def execute_command(args, shell=False):
    run(args, check=True, shell=shell)


def make_tar(
    src,
    dst,
    archive_name,
    rm_sourcefile=False,
    sendto="local",
    host=None,
    user=None,
    password=None,
):
    archive_name += ".tar.gz"
    src_relative_path = "." if src.is_dir() else src.name

    if src.is_dir():
        os.chdir(src)
    else:
        os.chdir(src.parent)

    if sendto == "local":
        execute_command(
            ["tar", "-zcf", dst / archive_name, src_relative_path]
        )
    else:
        dst = dst if dst.is_absolute() else ("/" / dst)
        execute_command(
            f"tar -zcf - {src_relative_path} | "
            f"curl -k -T - --user {user}:{password} --ftp-create-dirs {sendto}://{host}{dst}/{archive_name}",
            shell=True,
        )

    if rm_sourcefile:
        if src.is_dir():
            rmtree(src)
        else:
            src.unlink()


def backup_data(workdir):
    backup_dir = Path(
        f"/var/lib/docker/volumes/{workdir.name}_postgres/_data/backup"
    )
    backup_dir.mkdir(exist_ok=True)

    os.chdir(workdir)
    execute_command(
        [
            "docker-compose",
            "exec",
            "postgres",
            "sh",
            "-c",
            "backup > /var/lib/postgresql/data.sql",
        ]
    )

    execute_command(
        [
            "cp",
            "-a",
            f"/var/lib/docker/volumes/{workdir.name}_own_id/_data/.",
            backup_dir,
        ]
    )
    execute_command(
        [
            "mv",
            f"/var/lib/docker/volumes/{workdir.name}_postgres/_data/data.sql",
            backup_dir,
        ]
    )


def backup_metrics(workdir):
    os.chdir(workdir)

    for table in ("graphite", "graphite_tagged"):
        execute_command(
            [
                "docker-compose",
                "exec",
                "clickhouse",
                "clickhouse-client",
                "--query",
                f"ALTER TABLE {table} FREEZE",
            ]
        )


def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--backup_workdir", action="store_true")
    parser.add_argument("--backup_data", action="store_true")
    parser.add_argument("--backup_metrics", action="store_true")
    parser.add_argument("--dst_dir", type=Path, required=True)
    parser.add_argument("--workdir", type=Path, required=True)

    subparsers = parser.add_subparsers(dest="dst")
    dir = subparsers.add_parser("dir")
    ftp = subparsers.add_parser("ftp")
    sftp = subparsers.add_parser("sftp")

    for subparser in (ftp, sftp):
        subparser.add_argument("--host", required=True)
        subparser.add_argument("--user", required=True)
        subparser.add_argument("--password", required=True)

    return parser.parse_args()


def main():
    args = parse_args()

    dst = Path(args.dst_dir) / str(date.today())
    if args.dst == "dir":
        dst.mkdir(exist_ok=True)
        dst = dst.absolute()

    if args.backup_workdir:
        print(f"creating workdir backup in {dst}")

        try:
            if args.dst == "dir":
                make_tar(args.workdir, dst, "workdir")
            else:
                make_tar(
                    src=args.workdir,
                    dst=dst,
                    archive_name="workdir",
                    sendto=args.dst,
                    host=args.host,
                    user=args.user,
                    password=args.password,
                )
        except CalledProcessError:
            print("failed to backup workdir")
            exit(1)

    if args.backup_data:
        data_backup_tmp_path = Path(
            f"/var/lib/docker/volumes/{args.workdir.name}_postgres/_data/backup"
        )

        print(f"creating data backup in {data_backup_tmp_path}")
        try:
            backup_data(args.workdir)
        except CalledProcessError:
            rmtree(
                f"/var/lib/docker/volumes/{args.workdir.name}_postgres/_data/backup"
            )
            print("failed to backup data")
            exit(1)

        if args.dst == "dir":
            make_tar(data_backup_tmp_path, dst, "data", rm_sourcefile=True)
        else:
            make_tar(
                src=data_backup_tmp_path,
                dst=dst,
                archive_name="data",
                sendto=args.dst,
                host=args.host,
                user=args.user,
                password=args.password,
                rm_sourcefile=True,
            )

    if args.backup_metrics:
        metrics_backup_tmp_path = Path(
            f"/var/lib/docker/volumes/{args.workdir.name}_clickhouse/_data/shadow/1/data/default"
        )

        print(f"creating metrics backup in {metrics_backup_tmp_path}")
        try:
            backup_metrics(args.workdir)
        except CalledProcessError:
            rmtree(metrics_backup_tmp_path.parents[2])
            print("failed to backup metrics")
            exit(1)

        print(f"creating .tar.gz metrics backup in {dst}")

        if args.dst == "dir":
            make_tar(
                metrics_backup_tmp_path, dst, "metrics", rm_sourcefile=True
            )
        else:
            make_tar(
                src=metrics_backup_tmp_path,
                dst=dst,
                archive_name="metrics",
                sendto=args.dst,
                host=args.host,
                user=args.user,
                password=args.password,
            )

        print(f"removing backup tempdir {metrics_backup_tmp_path.parents[2]}")
        rmtree(metrics_backup_tmp_path.parents[2])


if __name__ == "__main__":
    main()
