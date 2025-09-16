# Gitfly

- Built a CLI version control system encompassing Git’s core features, such as file tracking, staging/commit, branching/checkout, unified diff via Myers’ algorithm, 3-way merges via Lowest Common Ancestor (LCA) with conflict markers, and remotes both over filesystem and over network (with a custom TCP protocol).

- Designed a deduplicating store of loose, zlib-compressed objects keyed by SHA-1 OIDs (via OpenSSL EVP), with atomic file writes and comprehensive unit tests.
