# Environment & Build-up

ESC servers with 128-core 2.6GHz AMD EPYC™ROME 7H12 CPUs and 256 GiB RAM. The machine runs on Linux 4.15.0-147-generic Ubuntu 18.04 with llvm-13.

## Env Setup & CDFuzz Compilation

```bash
# Install LLVM-13
wget https://apt.llvm.org/llvm.sh
sudo chmod +x llvm.sh
sudo ./llvm.sh 13

# Compile the CDFuzz
cd ../path-to-repo/CDFuzz
export PATH=/usr/lib/llvm-13/bin:$PATH
make clean && make
cd llvm-mode
make clean && make
```

## Compile target program & Extract dictionary

```bash
cd ../path-to-repo/Demo/jhead
export PATH=/usr/lib/llvm-13/bin:$PATH
export CC=../path-to-repo/CDFuzz/afl-clang-fast
make clean && make

# Start Fuzzing
cd ../path-to-repo/Demo/fuzzing
../path-to-repo/CDFuzz/afl-fuzz -d -i ./jhead-initial-seed -o ./fuzz-out -x ../jhead/jhead_dict -e -- ../jhead/jhead @@
```

# Benchmark Selection

| bench\fuzzer | AFL++ | FairFuzz | MOPT | QSYM | Meuzz | Pangolin | Angora | RedQueen | COUNT |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| readelf |  | 1 |  |  | 1 | 1 |  | 1 | 4 |
| nm |  | 1 |  |  |  | 1 | 1 | 1 | 4 |
| objdump |  | 1 | 1 | 1 | 1 | 1 | 1 | 1 | 7 |
| size |  |  |  |  |  |  | 1 | 1 | 2 |
| strip |  |  |  |  |  |  |  | 1 | 1 |
| djpeg | 1 | 1 |  |  | 1 | 1 | 1 |  | 5 |
| tcpdump | 1 | 1 |  | 1 | 1 | 1 |  |  | 5 |
| xmllint |  | 1 |  |  | 1 | 1 |  | 1 | 4 |
| jhead |  |  | 1 |  |  |  | 1 | 1 | 3 |
| pngfix |  |  |  |  |  |  | 1 |  | 1 |
| tiffinfo |  |  | 1 |  |  |  |  | 1 | 2 |
| xmlwf | 1 |  |  |  |  |  | 1 |  | 2 |
| tiff2bw |  |  | 1 |  |  |  |  | 1 | 2 |
| mutool |  | 1 |  |  |  |  |  |  | 1 |

# Bug report

| Program | Bug Type | Number | Status | Link |
| --- | --- | --- | --- | --- |
| bison | Use-of-uninitialized-value | 1 | reported | https://github.com/akimd/bison/issues/94 |
| objdump | Infinite loop | 1 | fixed | https://sourceware.org/bugzilla/show_bug.cgi?id=29647 |
| bsdtar | Use-of-uninitialized-value | 1 | reported | https://github.com/libarchive/libarchive/issues/1789 |
| jasper | Assertion failure | 1 | fixed | https://github.com/jasper-software/jasper/issues/345 |
| zzdir | Stack buffer overflow | 1 | reported | https://github.com/gdraheim/zziplib/issues/143 |
| unzzip | Stack buffer overflow | 1 | reported | https://github.com/gdraheim/zziplib/issues/144 |
| lou_translation | Infinite loop | 1 | fixed | https://github.com/liblouis/liblouis/issues/1256 |
| libtiff | Use-of-uninitialized-value | 7 | reported | https://gitlab.com/libtiff/libtiff/-/issues/451 |
| objcopy | Use-of-uninitialized-value | 1 | fixed | https://sourceware.org/bugzilla/show_bug.cgi?id=29613 |
| jhead | Use-of-uninitialized-value | 6 | reported | https://github.com/Matthias-Wandel/jhead/issues/59 |
| precomp | Bad-malloc | 1 | reported | https://github.com/schnaader/precomp-cpp/issues/139 |
| nm | Memory leaked | 1 | confirmed | https://sourceware.org/bugzilla/show_bug.cgi?id=29492 |
| jpeginfo | Heap-buffer-overflow | 1 | fixed | https://github.com/tjko/jpeginfo/issues/13 |
| jpeginfo | Use-of-uninitialized-value | 1 | confirmed | https://github.com/tjko/jpeginfo/issues/12 |
| cmix | Alloc-dealloc-mismatch | 1 | fixed | https://github.com/byronknoll/cmix/issues/53 |
| cmix | Memcpy-param-overlap | 1 | fixed | https://github.com/byronknoll/cmix/issues/54 |
| cmix | Use-of-uninitialized-value | 1 | confirmed | https://github.com/byronknoll/cmix/issues/55 |
| bento4 | Allocation-size-too-big | 1 | reported | https://github.com/axiomatic-systems/Bento4/issues/748 |
| bento4 | Out-of-memory | 2 | reported | https://github.com/axiomatic-systems/Bento4/issues/747 |
| bento4 | Memory leaked | 1 | reported | https://github.com/axiomatic-systems/Bento4/issues/746 |
| bento4 | Heap-buffer-overflow | 3 | reported | https://github.com/axiomatic-systems/Bento4/issues/745 |
| bento4 | Segmentation fault | 1 | reported | https://github.com/axiomatic-systems/Bento4/issues/741 |
| bento4 | Heap-use-after-free | 1 | reported | https://github.com/axiomatic-systems/Bento4/issues/740 |

# Dictionary Tokens Extraction
The process by which we extract dictionary tokens is shown in the accompanying code: https://github.com/SophrosyneX/Fuzzing-empirical-study/blob/main/CDFuzz/llvm_mode/afl-llvm-dict-analysis.cpp


# Performance overhead of CDFuzz vs. AFL

CDFuzz obtaining CFG and all tokens via compilation stage 

compilation time of CDFuzz vs. original AFL afl-clang-fast

| Benchmark | AFL | CDFuzz |
| --- | --- | --- |
| binutils | 184.6s | 260.8s |
| jhead | 1.6s | 2.0s |
| pngfix | 11.2s | 11.9s |
| djpeg | 11.2s | 12.0s |
| xmllint | 26.5s | 27.2s |
| llibtiff | 19.0s | 24.2s |
| xmlwf | 4.8s | 7.3s |
| mutools | 240.1s | 245.6s |
| re2 | 32.5s | 35.2s |
| jsoncpp | 18.0s | 19.6s |
| sqlite3 | 68.9s | 72.0s |
| bloaty | 338.0s | 371.6s |
| libxml2 | 26.6s | 29.4s |
| libjpeg-turbo | 41.0s | 51.6s |
| libpng | 6.1s | 7.4s |
| tcpdump | 124.4s | 135.5s |
| Average | 72.2s | 82.1s |
