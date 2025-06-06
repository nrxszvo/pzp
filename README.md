# pzp
Decode and parse zst archives containing pgn files, such as those from the [lichess database](https://database.lichess.org/), and save the data to [parquet files](https://parquet.apache.org/).  

## Usage
```python
from pzp import PyParserPool

pool = PyParserPool(
    nReaders=2, # Number of zst reader threads
    nMoveProcessors=2, # Number of pgn-parser threads
    minSec=0, # Minimum time control in seconds
    maxSec=7200, # Maximum time control in seconds
    maxInc=60, # Maximum increment in seconds
    outdir="test-out", # Output directory
)

pool.enqueue("example.pgn.zst", "example")
pool.join()
```

