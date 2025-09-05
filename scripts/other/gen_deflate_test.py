import zlib

base_len = 6007
repeats = 2

base = bytes(((i * 31 + 7) & 0xFF) for i in range(base_len))
data = base * repeats

cobj = zlib.compressobj(level=9, method=zlib.DEFLATED, wbits=-15, memLevel=9)
out = cobj.compress(data) + cobj.flush()

if out:
    fb=out[0]
    if (fb>>1)&0x3 != 2:
        print("Not dynamic block!")

hexlist = ", ".join(f"0x{b:02x}" for b in out)
print(f"uint8_t stream[] = {{{hexlist}}};")
