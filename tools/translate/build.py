"""Arma memorytext_<lang>.dat: memorytext_en.dat con los textos traducidos de <dir>/*chunk*.<lang>.json + index.json.
El inglés de referencia sale del propio memorytext_en.dat (el pack del release no trae textos del juego).
Uso: python3 tools/translate/build.py game_app/memorytext_en.dat translation/ es|pt salida.dat"""
import glob, json, os, re, sys
sys.path.insert(0, os.path.dirname(__file__))
from memorytext import unpack, pack, text

src, work, lang, dst = sys.argv[1:5]
entries = unpack(open(src, 'rb').read())
index = json.load(open(os.path.join(work, 'index.json')))
codes = re.compile(r'\$[A-Z]|&[NP]|%[0-9.]*[a-z%]')
done = bad = 0
for f in sorted(glob.glob(os.path.join(work, f'*chunk*.{lang}.json'))):
    tr = json.load(open(f, encoding='utf-8'))
    en = {k: text(entries[index[k][0]]) for k in tr if k in index}
    for k, t in tr.items():
        if k not in en or not isinstance(t, str) or not t.strip():
            continue
        if sorted(codes.findall(t)) != sorted(codes.findall(en[k])):  # códigos de color/salto/formato intactos
            bad += 1
            print('códigos distintos, queda en inglés:', k, repr(en[k][:60]), '->', repr(t[:60]))
            continue
        for i in index[k]:
            entries[i] = t.encode('utf-8') + b'\0'
        done += 1
open(dst, 'wb').write(pack(entries))
print(f'{lang}: {done} textos traducidos, {bad} descartados -> {dst}')
