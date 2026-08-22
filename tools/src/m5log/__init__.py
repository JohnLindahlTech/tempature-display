"""Client for the temperature display's encrypted log stream."""

from m5log.protocol import DEFAULT_PORT, read_frame, write_frame

__all__ = ["DEFAULT_PORT", "read_frame", "write_frame"]
