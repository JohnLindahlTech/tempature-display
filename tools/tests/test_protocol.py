"""Round-trip the handshake and framing against an in-process responder.

This covers the parts the device and this client must agree on - the NNpsk0
message flow, PSK handling, and the length-prefixed framing. What it cannot
cover is whether the firmware's C implementation agrees; only hardware proves
that.
"""

from __future__ import annotations

import base64
import os
import socket
import threading

import pytest
from noise.connection import NoiseConnection

from m5log.cli import handshake, load_psk
from m5log.protocol import HANDSHAKE_NAME, MAX_FRAME, read_frame, write_frame

PSK = bytes(range(32))


def responder(sock: socket.socket, psk: bytes, lines: list[bytes]) -> None:
    """Stand in for the device: complete the handshake, then stream lines.

    Closes the socket on any failure. Without that, a responder that rejects the
    handshake leaves the client blocked on a read that will never arrive - which
    is a deadlock in the test, not a behaviour of the device, since the firmware
    drops the connection on a bad handshake.
    """
    try:
        noise = NoiseConnection.from_name(HANDSHAKE_NAME)
        noise.set_as_responder()
        noise.set_psks(psk)
        noise.start_handshake()

        noise.read_message(read_frame(sock))
        write_frame(sock, noise.write_message())
        assert noise.handshake_finished

        for line in lines:
            write_frame(sock, noise.encrypt(line))
    finally:
        sock.close()


@pytest.fixture
def paired_sockets():
    left, right = socket.socketpair()
    # Any hang here is a bug in the test, so fail fast rather than block the run.
    left.settimeout(5)
    right.settimeout(5)
    yield left, right
    left.close()
    right.close()


def test_handshake_and_stream(paired_sockets):
    client_sock, device_sock = paired_sockets
    lines = [b"[    1204] ota: image on trial", b"[   60012] ota: image confirmed"]

    thread = threading.Thread(target=responder, args=(device_sock, PSK, lines))
    thread.start()

    noise = handshake(client_sock, PSK)
    received = [noise.decrypt(read_frame(client_sock)) for _ in lines]

    thread.join(timeout=5)
    assert received == lines


def test_wrong_psk_is_rejected(paired_sockets):
    client_sock, device_sock = paired_sockets
    wrong = bytes(32)

    thread = threading.Thread(target=responder, args=(device_sock, wrong, []))
    thread.daemon = True
    thread.start()

    # A mismatched key fails the AEAD tag on the first message, so the responder
    # cannot decrypt and the client never completes. Exactly what should happen.
    with pytest.raises(Exception):
        handshake(client_sock, PSK)


def test_frame_larger_than_the_device_accepts_is_refused(paired_sockets):
    client_sock, _ = paired_sockets
    with pytest.raises(ValueError, match="exceeds"):
        write_frame(client_sock, b"x" * (MAX_FRAME + 1))


def test_psk_must_decode_to_32_bytes(monkeypatch):
    monkeypatch.setenv("LOG_NOISE_PSK", base64.b64encode(b"tooshort").decode())
    with pytest.raises(SystemExit, match="expected 32"):
        load_psk()


def test_psk_is_read_from_the_environment(monkeypatch):
    monkeypatch.setenv("LOG_NOISE_PSK", base64.b64encode(PSK).decode())
    assert load_psk() == PSK


def test_missing_psk_explains_itself(monkeypatch):
    monkeypatch.delenv("LOG_NOISE_PSK", raising=False)
    with pytest.raises(SystemExit, match="LOG_NOISE_PSK"):
        load_psk()
