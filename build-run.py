import os

res = os.system("xmake")
if res != 0:
    exit(1)
os.system("xmake run")
