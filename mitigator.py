#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 BIFIT Mitigator Team <info@mitigator.ru>

import os
import sys
from argparse import ArgumentParser
from http import HTTPStatus
from ipaddress import IPv4Network
from json import dump, load
from urllib.parse import urlparse

import urllib3
from requests import Session

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)


DEFAULT_URL = ""
DEFAULT_TOKEN = ""
DEFAULT_USERNAME, DEFAULT_PASSWORD = "", ""
DEFAULT_POLICY = 0
DEFAULT_TTL = 300


class BackendApi:
    def __init__(self, url, token=None, username=None, password=None, insecure=False):
        self.s = Session()

        if insecure:
            self.s.verify = False
            self.s.trust_env = False

        self.url = url
        self.api_prefix = f"{self.url}/api/v4"

        if token:
            self.__update_token(token)

            if self.ping().status_code == HTTPStatus.OK:
                print("auth succeeded")

                return

            if username and password:
                print("invalid token, trying to auth with credentials")
            else:
                exit("auth failed")
        else:
            print("token was not specified, auth with credentials")

        self.__update_token(self.get_token(username, password))

        if self.ping().status_code != HTTPStatus.OK:
            exit("auth failed")

        print("auth succeeded")

    def __update_token(self, token):
        self.token = token
        self.s.headers.update({"x-auth-token": self.token})

    def __query(self, method, path, body=None):
        req = self.s.request(method=method, url=f"{self.api_prefix}/{path}", json=body)

        if req.status_code == HTTPStatus.MISDIRECTED_REQUEST:
            self.__handle_leader_change(req)
            req = self.s.request(
                method=method, url=f"{self.api_prefix}/{path}", json=body
            )

        return req

    def __handle_leader_change(self, req):
        redirect_url = req.headers.get("Location")
        if redirect_url:
            new_leader_url = urlparse(redirect_url)

        self.url = f"{new_leader_url.scheme}://{new_leader_url.netloc}"
        self.api_prefix = f"{self.url}/api/v4"

        print(f"leader has changed, redirecting request to {self.url}")

    def ping(self):
        return self.__query("GET", "backend/ping")

    def get_token(self, username, password):
        req = self.__query(
            "POST", "users/session", {"username": username, "password": password}
        ).json()

        return req.get("data", {}).get("token")

    def add_tbl_prefixes(self, policy, prefixes, ttl):
        return self.__query(
            "POST",
            f"tbl/items?policy={policy}&no_logs=true&source=1",
            {"items": [{"prefix": prefix, "ttl": ttl} for prefix in prefixes]},
        )


def parse_args():
    parser = ArgumentParser()

    parser.add_argument(
        "--insecure", help="Allow insecure connections", action="store_true"
    )

    parser.add_argument("--url", help="API URL", type=str, default=DEFAULT_URL)
    parser.add_argument("--token", help="API token", type=str, default=DEFAULT_TOKEN)
    parser.add_argument("--username", type=str, default=DEFAULT_USERNAME)
    parser.add_argument("--password", type=str, default=DEFAULT_PASSWORD)
    parser.add_argument("--policy", help="", type=int, default=DEFAULT_POLICY)
    parser.add_argument(
        "--ttl", help="Prefixes block time", type=int, default=DEFAULT_TTL
    )
    parser.add_argument(
        "--prefixes",
        help="File with prefixes to block, supported separators: newline, comma, semicolon, whitespace",
    )
    parser.add_argument("--prefix", help="Single prefix to block", type=str)

    parser.add_argument(
        "--clear",
        help="Clear source file when prefixes added successfully",
        action="store_true",
    )

    return parser.parse_args()


class Config:
    def __init__(self, token=None, leader=None, insecure=False):
        self.token = token
        self.leader = leader
        self.insecure = insecure

        self.file = "tbl_config.json"

    def save(self):
        with open(self.file, "w") as f:
            dump(
                {"token": self.token, "leader": self.leader, "insecure": self.insecure},
                f,
            )

        print(f"config file with auth token and leader address saved as ./{self.file}")

    def load(self):
        try:
            with open(self.file) as f:
                json = load(f)

            self.token = json.get("token")
            self.leader = json.get("leader")
            self.insecure = json.get("insecure")
        except FileNotFoundError:
            pass

        return self


def split_prefixes(line):
    for sep in (",", ";", " "):
        if sep in line:
            return line.split(sep)

    return [line]


def validate_prefix(prefix):
    try:
        IPv4Network(prefix)
    except:
        return False
    else:
        return True


if __name__ == "__main__":
    args = parse_args()

    config = Config().load()

    token = None
    if args.token:
        token = args.token
    else:
        if config.token:
            token = config.token
            print("auth token was loaded from config file")

    url = None
    if args.url:
        url = args.url
    else:
        if config.leader:
            url = config.leader
            print("leader address was loaded from config file")

    insecure = args.insecure or config.insecure

    api = BackendApi(url, token, args.username, args.password, insecure)

    prefixes = []

    if args.prefix:
        if validate_prefix(args.prefix):
            prefixes.append(args.prefix)

    if args.prefixes:
        with open(args.prefixes) as f:
            for line in f:
                line = line.strip()

                for p in split_prefixes(line):
                    if validate_prefix(p):
                        prefixes.append(p)

    response = api.add_tbl_prefixes(args.policy, prefixes, args.ttl).json()
    if response.get("error"):
        exit(f"TBL add failed: {response.get('error')}")

    config.token = api.token
    config.leader = api.url
    config.insecure = insecure
    config.save()

    if args.prefixes and args.clear:
        with open(args.prefixes, "w"):
            pass
