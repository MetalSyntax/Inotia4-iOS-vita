"""Revisa una traducción: claves faltantes, JSON inválido y códigos ($R $S $G $A $B &N &P %d...) distintos al inglés.
Uso: python3 tools/translate/check.py translation/chunk0.es.json"""
import json, re, sys
codes = re.compile(r'\$[A-Z]|&[NP]|%[0-9.]*[a-z%]')
t = json.load(open(sys.argv[1], encoding='utf-8'))
en = json.load(open(re.sub(r'\.(es|pt)\.json$', '.en.json', sys.argv[1]), encoding='utf-8'))
miss = [k for k in en if k not in t]
bad = [k for k in en if k in t and sorted(codes.findall(t[k])) != sorted(codes.findall(en[k]))]
for k in bad[:30]:
    print('códigos:', k, repr(en[k][:70]), '->', repr(t[k][:70]))
print(f'{len(en)} textos, faltan {len(miss)} {miss[:20]}, códigos distintos {len(bad)}')
sys.exit(1 if miss or bad else 0)
