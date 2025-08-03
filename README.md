# pzp
Decode and parse zst archives containing pgn files, such as those from the [lichess database](https://database.lichess.org/), and save the data to [parquet files](https://parquet.apache.org/).  

## Requirements
### for C++
- CMake 3
- libcurl
- libre2
- libarrow
- libparquet
- libzstd
### for python
- Python 3.13
- setuptools
- cython
- wheel

## Installation
```bash
python setup.py bdist_wheel
pip install dist/<pzp-build>.whl
```

## Usage
```python
from pzp import ParserPool

pool = ParserPool(outdir='parquet-output')
pool.enqueue("example.pgn.zst", "example")
pool.join()
```

