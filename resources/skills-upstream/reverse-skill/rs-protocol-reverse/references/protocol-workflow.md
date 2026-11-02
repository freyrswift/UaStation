# Protocol Reverse Quick Reference

> Applies to: `protocol-reverse` skill · 2026-07-18

## Common Layout Patterns

| Pattern | Signature | Hint |
|------|------|------|
| Fixed-length header + body | First 2/4 bytes are length | Watch whether it includes the header length |
| Magic number | Fixed `0xDEAD` etc. | Helps stream resynchronization |
| TLV | Repeating type-length-value | The type enumeration is the message dictionary |
| Protobuf | Field numbers as varint | `protoc --decode_raw` |
| Encrypted frames | High entropy, no cleartext URLs | Find the nonce/IV neighborhood first |

## Minimal Python Skeleton

```python
import struct
def parse_frame(buf: bytes):
    magic, length, msg_type = struct.unpack_from(">IHI", buf, 0)
    body = buf[10:10+length]
    return {"magic": magic, "type": msg_type, "body": body}
```

## Extracting TCP payload from PCAP

```bash
tshark -r cap.pcap -Y "tcp.port==4433" -T fields -e tcp.payload | head
```
