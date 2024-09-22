export PATH=/usr/lib/llvm-13//bin:$PATH 

# echo "################################### AFL_TARGETED_DICT ###################################"
# export CC=../path-to-repo/CDFuzz/afl-clang-fast
# export CXX=../path-to-repo/CDFuzz/afl-clang-fast++


result="./result"
bench="./temp-bench"

if [ ! -d $bench ];then
    mkdir $bench
fi

if [ ! -d $result ];then
    mkdir $result
fi



echo "################################### binutils ###################################"
cd $bench
rm -rf binutils-gdb
rm -rf $result/nm-new
rm -rf $result/strip-new
rm -rf $result/size
rm -rf $result/readelf
rm -rf $result/objdump
rm -rf $result/objcopy
rm -rf $result/cxxfilt
git clone https://github.com/bminor/binutils-gdb.git
cd binutils-gdb
./configure --disable-shared
make clean && make

mkdir -p $result/nm $result/strip $result/size $result/readelf $result/objdump $result/objcopy $result/cxxfilt
cp ./binutils/nm-new* $result/nm
cp ./binutils/strip-new* $result/strip
cp ./binutils/size* $result/size
cp ./binutils/readelf* $result/readelf
cp ./binutils/objdump* $result/objdump
cp ./binutils/objcopy* $result/objcopy
cp ./binutils/cxxfilt* $result/cxxfilt




echo "################################### jhead ###################################"
cd $bench
rm -rf jhead
rm -rf $result/jhead
git clone https://github.com/Matthias-Wandel/jhead.git
cd jhead
make clean && make

mkdir -p $result/jhead
cp ./jhead* $result/jhead



echo "################################### libpng ###################################"
cd $bench
rm -rf libpng
rm -rf $result/pngimage
rm -rf $result/pngfix
git clone https://github.com/glennrp/libpng.git
cd libpng
./configure --disable-shared
make clean && make
mkdir -p $result/pngimage $result/pngfix
cp pngimage* $result/pngimage
cp pngfix* $result/pngfix



echo "################################### xmlwf ###################################"
cd $bench
rm -rf $result/xmlwf
rm -rf ./expat-2.4.8
rm -rf ./expat-2.4.8.tar.gz
wget  https://github.com/libexpat/libexpat/releases/download/R_2_4_8/expat-2.4.8.tar.gz
tar -xvf expat-2.4.8.tar.gz
cd expat-2.4.8
./configure --disable-shared
make clean && make
mkdir -p $result/xmlwf
cp xmlwf/xmlwf* $result/xmlwf



echo "################################### libxml2 (xmllint) ###################################"
cd $bench
rm -rf libxml2-2.9.12
rm -rf $result/xmllint
rm -rf libxml2-2.9.12.tar.gz
wget http://xmlsoft.org/sources/libxml2-2.9.12.tar.gz
tar -xvf libxml2-2.9.12.tar.gz
cd libxml2-2.9.12
./configure --disable-shared
make clean && make
mkdir -p $result/xmllint
cp xmllint* $result/xmllint



echo "################################### libjpeg (djpeg) ###################################"
cd $bench
rm -rf jpeg-9c
rm -rf $result/djpeg
rm -rf ./jpegsrc.v9c.tar.gz
wget https://www.ijg.org/files/jpegsrc.v9c.tar.gz
tar -xvf jpegsrc.v9c.tar.gz
cd jpeg-9c
./configure --disable-shared
make clean && make
mkdir -p $result/djpeg
cp djpeg* $result/djpeg



echo "################################### tcpdump ###################################"
cd $bench
rm -rf libpcap
rm -rf tcpdump
rm -rf $result/tcpdump
git clone https://github.com/the-tcpdump-group/libpcap.git
cd ./libpcap
./configure  --disable-shared
make clean && make

cd $bench
git clone https://github.com/the-tcpdump-group/tcpdump.git
cd tcpdump
./configure --disable-shared
make clean && make
mkdir -p $result/tcpdump
cp tcpdump* $result/tcpdump



echo "################################### mutool ###################################"
cd $bench
rm -rf $result/mutool
rm -rf mupdf-1.20.0-source
rm -rf ./mupdf-1.20.0-source.tar.gz
wget https://mupdf.com/downloads/archive/mupdf-1.20.0-source.tar.gz
tar -zxvf ./mupdf-1.20.0-source.tar.gz
cd mupdf-1.20.0-source
make clean && make
mkdir -p $result/mutool
cp ./build/release/mutool* $result/mutool
cp ./mutool* $result/mutool



echo "################################### libtiff ###################################"
cd $bench
rm -rf tiff-4.2.0
rm -rf $result/tiff2bw
rm -rf $result/tiff2pdf
rm -rf $result/tiff2ps
rm -rf $result/tiffinfo
rm -rf ./tiff-4.2.0.tar.gz
wget https://download.osgeo.org/libtiff/tiff-4.2.0.tar.gz
tar -xvf tiff-4.2.0.tar.gz
cd tiff-4.2.0
./configure --disable-shared
make clean && make
mkdir -p $result/tiff2bw $result/tiff2pdf $result/tiff2ps $result/tiffinfo
cp ./tools/tiff2bw* $result/tiff2bw
cp ./tools/tiff2pdf* $result/tiff2pdf
cp ./tools/tiff2ps* $result/tiff2ps
cp ./tools/tiffinfo* $result/tiffinfo



echo "################################### clean ###################################"
cd $result
find . -regex ".*\.c\|.*\.o\|.*\.1\|.*\.in\|.*\.h\|.*\.spec" -delete