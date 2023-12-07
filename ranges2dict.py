#!/usr/bin/env python3

ranges = ["16:24=49:57", "25:40=128:143", "41:44=148:151", "45:47=236:238", "56:61=59:64", "63:77=65:79", "78:79=5:6", "248:248=239:239", "203:207=147:151", "240:243=76:79", "244:244=239:239", "245:245=1:1"]

mapping = [0]*256
for i in range(256):
    mapping[i] = i
print(mapping)

for r in ranges:
    l, r = tuple(r.split('='))
    s1, e1 = tuple(l.split(':'))
    s2, e2 = tuple(r.split(':'))

    s1, e1 = int(s1), int(e1)+1
    s2, e2 = int(s2), int(e2)+1

    i = s1
    while i < e1:
        mapping[i] = s2+(i-s1)
        i += 1

print(mapping)
