"""Extrae los textos a traducir de memorytext_en.dat en trozos JSON (chunkN.en.json) y agrega sus índices a index.json.
--dialogo: solo el diálogo de la historia (claves "dN", trozos dchunkN), sin tocar lo ya extraído.
Uso: python3 tools/translate/extract.py game_app/memorytext_en.dat translation/ [trozos] [--dialogo]"""
import json, os, sys
sys.path.insert(0, os.path.dirname(__file__))
from memorytext import unpack, text

DIALOG = '--dialogo' in sys.argv
argv = [a for a in sys.argv if a != '--dialogo']
# nombres/objetos/habilidades/mapas; misiones/sistema. 3895-32359: diálogo
RANGES = [(3895, 32360)] if DIALOG else [(0, 3895), (32360, 35452)]
src, out = argv[1], argv[2]
parts = int(argv[3]) if len(argv) > 3 else 6
pre = 'd' if DIALOG else ''
en = [text(e) for e in unpack(open(src, 'rb').read())]
uniq = {}
for a, b in RANGES:
    for i in range(a, min(b, len(en))):
        s = en[i]
        if s.strip() and s != 'NO TEXT' and any(c.isalpha() for c in s):
            uniq.setdefault(s, []).append(i)
keys = list(uniq)
os.makedirs(out, exist_ok=True)
ip = os.path.join(out, 'index.json')
index = json.load(open(ip)) if DIALOG and os.path.exists(ip) else {}
index.update({pre + str(k): uniq[s] for k, s in enumerate(keys)})
json.dump(index, open(ip, 'w'))
size = sum(map(len, keys)) / parts
cur, n, acc = {}, 0, 0
for k, s in enumerate(keys):
    cur[pre + str(k)] = s
    acc += len(s)
    if acc >= size * (n + 1) and n < parts - 1:
        json.dump(cur, open(os.path.join(out, f'{pre}chunk{n}.en.json'), 'w'), ensure_ascii=False, indent=0)
        cur, n = {}, n + 1
json.dump(cur, open(os.path.join(out, f'{pre}chunk{n}.en.json'), 'w'), ensure_ascii=False, indent=0)
print(len(keys), 'textos únicos en', n + 1, 'trozos')
