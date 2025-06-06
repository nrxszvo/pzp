from pzp import ParserPool

# Create a parser pool with 2 readers, 2 move processors
pool = ParserPool()

# Try adding a file to process
pool.enqueue("test_pool/test.pgn.zst", "test1")
pool.enqueue("test_pool/test2.pgn.zst", "test2")
pool.enqueue("test_pool/test3.pgn.zst", "test3")

# Wait for processing to complete
pool.join()

print("Done!")    # Output to current directory
