"""memorytext*.dat de Inotia 4: [0x01 0x00][cabecera .lzma: props 5 B + tamaño 8 B][LZMA]. Descomprimido:
u32 cantidad, cantidad offsets de 3 bytes (LE, desde el inicio), textos UTF-8 terminados en 0.
El juego (LZMA_Decode, SDK 4.x de una pasada) usa el tamaño de la cabecera; el fin de flujo sobrante no molesta."""
import lzma


def unpack(raw):
    d = lzma.decompress(raw[2:], format=lzma.FORMAT_ALONE)
    n = int.from_bytes(d[:4], 'little')
    off = [int.from_bytes(d[4 + 3 * i:7 + 3 * i], 'little') for i in range(n)] + [len(d)]
    return [d[off[i]:off[i + 1]] for i in range(n)]  # cada uno con su 0 final (o vacío)


def pack(entries):
    head = len(entries).to_bytes(4, 'little')
    pos = 4 + 3 * len(entries)
    offs, body = [], []
    for e in entries:
        offs.append(pos.to_bytes(3, 'little'))
        body.append(e)
        pos += len(e)
    assert pos < 1 << 24
    d = head + b''.join(offs) + b''.join(body)
    z = lzma.compress(d, format=lzma.FORMAT_ALONE, filters=[{'id': lzma.FILTER_LZMA1, 'dict_size': 1 << 20}])
    z = z[:5] + len(d).to_bytes(8, 'little') + z[13:]  # tamaño real en vez de "desconocido"
    out = b'\x01\x00' + z
    assert unpack_sized(out) == d
    return out


def unpack_sized(raw):  # verificación: decodifica exactamente el tamaño de la cabecera
    size = int.from_bytes(raw[7:15], 'little')
    dec = lzma.LZMADecompressor(format=lzma.FORMAT_RAW, filters=[lzma._decode_filter_properties(lzma.FILTER_LZMA1, raw[2:7])])
    return dec.decompress(raw[15:], max_length=size)


def text(e):
    return e.rstrip(b'\0').decode('utf-8')
