from pzp import ParserPool
import subprocess
import os
import time

devnull = open(os.devnull, 'w')
# Create a parser pool with 2 readers, 2 move processors
pool = ParserPool(outdir='test/output', printOffset=3, nSimultaneous=3, nReadersPerFile=2, nParsersPerFile=4)

# Try adding a file to process
nfiles = 0
with open("test/list.txt") as f:
    for line in f:
        url = line.strip()
        fn = url.split("/")[-1]
        print(f"\033[0HDownloading {fn}...", end='\r')
        fn = os.path.join('test', fn)
        if not os.path.exists(fn):
            subprocess.call(["curl", "-o", fn, url], stdout=devnull, stderr=devnull) 
        print("\033[0HFinished downloading".rjust(100), end='\r')
        pool.enqueue(fn, os.path.basename(fn).split('.')[0])
        nfiles += 1

while len(pool.get_completed()) < nfiles:
    time.sleep(1)
    print('\033[2J' + '\033[0H', end='')
    for line in pool.get_info():
        print(line)

# Wait for processing to complete
pool.join()

print("Done!")    # Output to current directory
