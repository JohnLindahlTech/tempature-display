"""Attach to the display's encrypted log stream."""

from __future__ import annotations

import argparse
import base64
import os
import socket
import sys

from noise.connection import NoiseConnection

from m5log.protocol import (
    DEFAULT_PORT,
    HANDSHAKE_NAME,
    PSK_LENGTH,
    read_frame,
    write_frame,
)


def load_psk() -> bytes:
    """Read the pre-shared key from the environment.

    Deliberately not a command-line flag: a key passed as an argument lands in
    shell history and is visible in `ps` to every user on the machine.
    """
    encoded = os.environ.get("LOG_NOISE_PSK")
    if not encoded:
        raise SystemExit(
            "set LOG_NOISE_PSK to the same value as src/credentials.h, e.g.\n"
            "  export LOG_NOISE_PSK='...'"
        )
    try:
        psk = base64.b64decode(encoded, validate=True)
    except Exception as exc:
        raise SystemExit(f"LOG_NOISE_PSK is not valid base64: {exc}") from exc
    if len(psk) != PSK_LENGTH:
        raise SystemExit(
            f"LOG_NOISE_PSK decodes to {len(psk)} bytes, expected {PSK_LENGTH} "
            "- generate one with: openssl rand -base64 32"
        )
    return psk


def handshake(sock: socket.socket, psk: bytes) -> NoiseConnection:
    noise = NoiseConnection.from_name(HANDSHAKE_NAME)
    noise.set_as_initiator()
    noise.set_psks(psk)
    noise.start_handshake()

    # NNpsk0 is two messages: initiator speaks first, responder replies, both
    # sides then split into transport ciphers.
    write_frame(sock, noise.write_message())
    noise.read_message(read_frame(sock))
    if not noise.handshake_finished:
        raise SystemExit("handshake did not complete")
    return noise


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("host", help="device hostname or IP address")
    parser.add_argument("-p", "--port", type=int, default=DEFAULT_PORT)
    parser.add_argument(
        "--connect-timeout", type=float, default=10.0, metavar="SECONDS"
    )
    args = parser.parse_args()

    psk = load_psk()

    # Ctrl-C is the normal way to stop watching a log stream, so it is a clean
    # exit rather than an error. KeyboardInterrupt is not an OSError, so it
    # would otherwise unwind straight through the handlers below and print a
    # traceback from whichever recv() happened to be blocked at the time.
    try:
        return _stream(args, psk)
    except KeyboardInterrupt:
        print("\n-- detached --", file=sys.stderr)
        return 0


def _stream(args: argparse.Namespace, psk: bytes) -> int:
    try:
        sock = socket.create_connection((args.host, args.port), args.connect_timeout)
    except OSError as exc:
        raise SystemExit(f"cannot reach {args.host}:{args.port} - {exc}") from exc

    with sock:
        try:
            noise = handshake(sock, psk)
        except (ConnectionError, OSError) as exc:
            # The device closes the connection on a bad key rather than saying
            # so, which is the correct behaviour but does make this ambiguous.
            raise SystemExit(f"handshake failed ({exc}) - wrong PSK?") from exc

        print(f"-- attached to {args.host}:{args.port} --", file=sys.stderr)
        sock.settimeout(None)
        while True:
            try:
                payload = noise.decrypt(read_frame(sock))
            except (ConnectionError, OSError) as exc:
                print(f"-- {exc} --", file=sys.stderr)
                return 0
            sys.stdout.write(payload.decode("utf-8", "replace") + "\n")
            sys.stdout.flush()


if __name__ == "__main__":
    raise SystemExit(main())
