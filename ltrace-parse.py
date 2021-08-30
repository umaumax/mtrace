#!/usr/bin/env python3

import json
import argparse
import re
import sys


def remove_prefix(text, prefix):
    if text.startswith(prefix):
        return text[len(prefix):]
    return text


def trace_lock(addr, timestamp, pid, tid):
    return [
        # for local scope
        {
            "name": "mutex {}".format(addr),
            "cat": "locking",
            "ph": "b",
            "ts": timestamp,
            "dur": 0,
            "pid": pid,
            "tid": tid,
            "id": addr,
            "args": {}
        },
        # for global scope
        {
            "name": "global mutex {}".format(addr),
            "cat": "locking",
            "ph": "b",
            "ts": timestamp,
            "dur": 0,
            "pid": pid,
            "tid": pid,
            "id": "{}{}".format(pid, addr),
            "args": {}
        },
        # flow event
        # NOTE: first flow event 'f' will be ignored
        # because it has no 's' pair
        {
            "name": "{}".format(addr),
            "cat": "unlocking",
            "ph": "f",
            "ts": timestamp,
            "dur": 0,
            "pid": pid,
            "tid": tid,
            "id": addr,
            "args": {}
        }]


def trace_unlock(addr, timestamp, pid, tid):
    return [
        # for local scope
        {
            "name": "mutex {}".format(addr),
            "cat": "locking",
            "ph": "e",
            "ts": timestamp,
            "dur": 0,
            "pid": pid,
            "tid": tid,
            "id": addr,
            "args": {}
        },
        # for global scope
        {
            "name": "global mutex {}".format(addr),
            "cat": "locking",
            "ph": "e",
            "ts": timestamp,
            "dur": 0,
            "pid": pid,
            "tid": pid,
            "id": "{}{}".format(pid, addr),
            "args": {}
        },
        # flow event
        {
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


def parse_line(line):
    ret = re.search(r'<(?P<duration>[0-9]+\.[0-9]+)>', line)
    duration = 1
    if ret is not None:
        # second to micro seconds
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
    return (duration, cond_addr, mutex_addr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        '-o',
        '--output-filepath',
        type=argparse.FileType("w"),
        default=sys.stdout)
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('input')
    parser.add_argument('args', nargs='*')

    trace_list = []
    args, extra_args = parser.parse_known_args()
    mutex_lock_dict = {}
    pid = 1234  # duumy value
    with open(args.input) as file:
        # input format is ltrace output
        for line in file:
            cols = line.split()
            if len(cols) < 2:
                print("invalid format [{}]".format(line), file=sys.stderr)
                return 1
            tid = int(cols[0])
            # unit is us
            timestamp = float(cols[1]) * 1000.0 * 1000.0
            name = None
            if args.verbose:
                print(line, end="", file=sys.stderr)
            for func_name in ["pthread_mutex_lock", "pthread_mutex_unlock", "pthread_cond_wait",
                              "pthread_cond_timedwait", "pthread_cond_signal", "pthread_cond_broadcast"]:
                if func_name in line:
                    name = func_name

            (duration, cond_addr, mutex_addr) = parse_line(line)

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
                            tid, timestamp, name, mutex_addr), file=sys.stderr)
            elif "resumed" in line:
                timestamp, cond_addr, mutex_addr = mutex_lock_dict[tid]
                if args.verbose:
                    print(
                        "{} {} {}[end] {}".format(
                            tid, timestamp, name, mutex_addr), file=sys.stderr)
            else:
                if args.verbose:
                    print(
                        "{} {} {}[once] {}".format(
                            tid, timestamp, name, addr), file=sys.stderr)

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
                trace_list += trace_unlock(addr, timestamp, pid, tid)
            timestamp += duration
            if name in ["pthread_mutex_lock", "pthread_cond_wait",
                        "pthread_cond_timedwait"]:
                trace_list += trace_lock(addr, timestamp, pid, tid)

    with args.output_filepath as f:
        data = json.dumps(list(trace_list))
        f.write(data)

    return 0


if __name__ == '__main__':
    sys.exit(main())
