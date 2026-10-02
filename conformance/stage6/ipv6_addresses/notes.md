# Notes

## Why nothing listens on every interface

Every listener in this program is on a loopback address (`127.0.0.1` or `::1`), which no firewall asks about.
`listen_everywhere` and `open_everywhere` are checked by the addresses they would bind (`::`, or `0.0.0.0` where
the system has no IPv6), never by listening: a socket on every interface makes Windows Firewall ask whether to let
the program through, and `check.sh` builds a new executable at a new path on every run, so it asked on every run.
That the `::` socket takes IPv4 connections too is the system's `IPV6_V6ONLY` setting, which the library turns off
([the standard library](../../../docs/standard_library.md#udpsocket)).
