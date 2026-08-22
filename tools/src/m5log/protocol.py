"""Wire format for the log stream.

Every frame is a 2-byte big-endian length followed by that many bytes. The first
two frames are the Noise handshake; everything after is one encrypted log line
per frame.

Kept separate from the CLI so the tests can drive both ends of it.
"""

from __future__ import annotations

import socket

DEFAULT_PORT = 6053

# Matches LOG_NOISE_MAX_FRAME in src/Config.h. The device drops anything larger
# rather than fragmenting, so a mismatch here shows up as truncated log lines.
MAX_FRAME = 512

HANDSHAKE_NAME = b"Noise_NNpsk0_25519_ChaChaPoly_SHA256"
PSK_LENGTH = 32


def read_exactly(sock: socket.socket, count: int) -> bytes:
    chunks = []
    while count > 0:
        chunk = sock.recv(count)
        if not chunk:
            raise ConnectionError("device closed the connection")
        chunks.append(chunk)
        count -= len(chunk)
    return b"".join(chunks)


def read_frame(sock: socket.socket) -> bytes:
    header = read_exactly(sock, 2)
    length = (header[0] << 8) | header[1]
    if length > MAX_FRAME:
        raise ConnectionError(f"frame of {length} bytes exceeds the {MAX_FRAME} limit")
    return read_exactly(sock, length)


def write_frame(sock: socket.socket, payload: bytes) -> None:
    if len(payload) > MAX_FRAME:
        raise ValueError(f"frame of {len(payload)} bytes exceeds the {MAX_FRAME} limit")
    sock.sendall(bytes([len(payload) >> 8, len(payload) & 0xFF]) + payload)
