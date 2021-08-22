#!/usr/bin/env python3

import json
import argparse
import re


def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text  # or whatever


def main():
    parser = argparse.ArgumentParser()
    # parser.add_argument('-o', '--output-filepath', default='')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('input')
    parser.add_argument('args', nargs='*')  # any length of args is ok

    trace_list = []
    args, extra_args = parser.parse_known_args()
    mutex_lock_dict = {}
    pid = 1234
    with open(args.input) as file:
        for line in file:
            cols = line.split()
            tid = int(cols[0])
            timestamp = float(cols[1]) * 1000.0 * 1000.0
            name = None
            if args.verbose:
                print(line, end="")
            if "pthread_mutex_lock" in line:
                name = "pthread_mutex_lock"
            if "pthread_mutex_unlock" in line:
                name = "pthread_mutex_unlock"
            if "pthread_cond_wait" in line:
                name = "pthread_cond_wait"
            if "pthread_cond_timedwait" in line:
                name = "pthread_cond_timedwait"
            if "pthread_cond_signal" in line:
                name = "pthread_cond_signal"
            if "pthread_cond_broadcast" in line:
                name = "pthread_cond_broadcast"

            ret = re.search(r'<(?P<duration>[0-9]+\.[0-9]+)>', line)
            duration = 1
            if ret is not None:
                duration = float(ret.group("duration")) * 1000.0 * 1000.0
            cond_addr = None
            mutex_addr = None
            ret = re.search(
                r'cond=(?P<cond_addr>0x[0-9a-f]+)', line)
            if ret is not None:
                cond_addr = ret.group("cond_addr")
            ret = re.search(
                r'mutex=(?P<mutex_addr>0x[0-9a-f]+)', line)
            if ret is not None:
                mutex_addr = ret.group("mutex_addr")

            if name is None:
                continue
            if name in ["pthread_cond_signal", "pthread_cond_broadcast"]:
                trace_list += [{
                    "name": "{}({})".format(remove_prefix(name, "pthread_cond_"), cond_addr),
                    "cat": "{}".format(name),
                    "ph": "i",
                    "ts": timestamp,
                    "dur": duration,
                    "pid": pid,
                    "tid": tid,
                    "s": "g",
                    "args": {}
                }]
                continue

            if "unfinished" in line:
                mutex_lock_dict[tid] = (timestamp, cond_addr, mutex_addr)
                if args.verbose:
                    print(
                        "{} {} {}[start] {}".format(
                            tid, timestamp, name, mutex_addr))
            elif "resumed" in line:
                timestamp, cond_addr, mutex_addr = mutex_lock_dict[tid]
                if args.verbose:
                    print(
                        "{} {} {}[end] {}".format(
                            tid, timestamp, name, mutex_addr))
            else:
                if args.verbose:
                    print(
                        "{} {} {}[once] {}".format(
                            tid, timestamp, name, addr))

            addr = mutex_addr
            if cond_addr is not None:
                addr = cond_addr
            if duration == 0:
                duration = 1
            trace_list += [{
                # "name": "{}({})".format(remove_prefix(name, "pthread_mutex_"), addr),
                "name": "{}({})".format(name, addr),
                "cat": "{}".format(name),
                "ph": "X",
                "ts": timestamp,
                "dur": duration,
                "pid": pid,
                "tid": tid,
                "args": {}
            }]
            addr = mutex_addr
            if name in ["pthread_mutex_unlock",
                        "pthread_cond_wait", "pthread_cond_timedwait"]:
                # for local scope
                trace_list += [{
                    "name": "mutex {}".format(addr),
                    "cat": "locking",
                    "ph": "e",
                    "ts": timestamp,
                    "dur": 0,
                    "pid": pid,
                    "tid": tid,
                    "id": addr,
                    "args": {}
                }]
                # for global scope
                trace_list += [{
                    "name": "global mutex {}".format(addr),
                    "cat": "locking",
                    "ph": "e",
                    "ts": timestamp,
                    "dur": 0,
                    "pid": pid,
                    "tid": pid,
                    "id": "{}{}".format(pid, addr),
                    "args": {}
                }]
                # flow event
                trace_list += [{
                    "name": "{}".format(addr),
                    "cat": "unlocking",
                    "ph": "s",
                    "ts": timestamp,
                    "dur": 0,
                    "pid": pid,
                    "tid": tid,
                    "id": addr,
                    "args": {}
                }]
            if name in ["pthread_mutex_lock", "pthread_cond_wait",
                        "pthread_cond_timedwait"]:
                # for local scope
                trace_list += [{
                    "name": "mutex {}".format(addr),
                    "cat": "locking",
                    "ph": "b",
                    "ts": timestamp + duration,
                    "dur": 0,
                    "pid": pid,
                    "tid": tid,
                    "id": addr,
                    "args": {}
                }]
                # for global scope
                trace_list += [{
                    "name": "global mutex {}".format(addr),
                    "cat": "locking",
                    "ph": "b",
                    "ts": timestamp + duration,
                    "dur": 0,
                    "pid": pid,
                    "tid": pid,
                    "id": "{}{}".format(pid, addr),
                    "args": {}
                }]
                # flow event
                # NOTE: first flow event 'f' will be ignored
                # because it has no 's' pair
                trace_list += [{
                    "name": "{}".format(addr),
                    "cat": "unlocking",
                    "ph": "f",
                    "ts": timestamp + duration,
                    "dur": 0,
                    "pid": pid,
                    "tid": tid,
                    "id": addr,
                    "args": {}
                }]

    print(json.dumps(list(trace_list)))


if __name__ == '__main__':
    main()