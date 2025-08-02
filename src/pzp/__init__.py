from .pgnzstparser import PyParserPool


class ParserPool:
    def __init__(
        self,
        nSimultaneous=1,
        nReadersPerFile=2,
        nParsersPerFile=4,
        minSec=0,
        maxSec=2**31-1,
        maxInc=2**31-1,
        elo_edges=[1000,1200,1400,1600,1800,2000,2200,2400,2600,2800,3000],
        chunkSize=1024,
        printFreq=1,
        printOffset=1,
        outdir="pzp-output",
    ):
        """
        Initialize a parser pool with the given parameters.

        Args:
            nSimultaneous: Number of files to process simultaneously.
            nReadersPerFile: Number of readers per file.
            nParsersPerFile: Number of parsers per file.
            minSec: Minimum time control in seconds.
            maxSec: Maximum time control in seconds.
            maxInc: Maximum increment in seconds.
            elo_edges: Elo rating buckets.
            chunkSize: Size of chunks to process.
            printFreq: Frequency of progress printing.
            printOffset: Offset for progress printing.
            outdir: Output directory for parsed files.
        """
        assert nSimultaneous >= 1
        assert nReadersPerFile >= 1
        assert nParsersPerFile >= 1
        assert minSec >= 0
        assert maxSec >= minSec
        assert maxInc >= 0
        assert chunkSize >= 1
        assert printFreq >= 1
        assert printOffset >= 1
        assert outdir is not None

        self._pool = PyParserPool(
            nReadersPerFile,
            nParsersPerFile,
            minSec,
            maxSec,
            maxInc,
            outdir,
            elo_edges,
            chunkSize,
            printFreq,
            nSimultaneous,
            printOffset,
        )

    def enqueue(self, file_path: str, name: str):
        self._pool.enqueue(file_path, name)

    def join(self):
        self._pool.join()

    def get_completed(self):
        names = self._pool.get_completed()
        counts = self._pool.get_ngames()
        return [(name.decode('utf-8'), ngames) for name, ngames in zip(names, counts)]

