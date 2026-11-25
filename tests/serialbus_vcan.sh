#!/bin/sh
# MIT License - see LICENSE
#
# Runs the Nt SerialBus tests with a vcan0 of their own: in a user and network namespace, where an unprivileged user
# may add the interface, with /sys mounted again so that it lists the interfaces of that namespace. Where the machine
# allows no such namespace or has no vcan module, the test is skipped rather than failed.
test="$1"
if [ -z "$test" ]; then
    echo "usage: $0 <nt_serialbus_tests>" >&2
    exit 2
fi
if ! command -v unshare >/dev/null 2>&1 || ! command -v ip >/dev/null 2>&1; then
    echo "skipped: unshare or ip is missing"
    exit 0
fi
if ! unshare -rnm sh -c 'mount -t sysfs none /sys && ip link add vcan0 type vcan' >/dev/null 2>&1; then
    echo "skipped: this machine allows no network namespace with a vcan interface"
    exit 0
fi
exec unshare -rnm sh -c 'mount -t sysfs none /sys && ip link add vcan0 type vcan && ip link set up vcan0 && exec "$0"' "$test"
