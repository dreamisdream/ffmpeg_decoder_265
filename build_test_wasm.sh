echo "emconfigure"
emconfigure ./configure --cc="emcc" --cxx="em++" --ar="emar" --ranlib="emranlib" --prefix=$(pwd)/../open_decoder_wasm/ffmpeg_wasm \
         --enable-cross-compile --target-os=none \
        --arch=x86_32 --cpu=generic --enable-gpl --enable-version3 \
        --disable-swresample --disable-postproc --disable-logging --disable-everything \
        --disable-programs --disable-asm --disable-doc --disable-network --disable-debug \
        --disable-iconv --disable-sdl2 \
        --disable-avdevice \
        --disable-avformat \
        --disable-avfilter \
        --disable-decoders \
        --disable-encoders \
        --disable-muxers \
        --disable-demuxers \
        --disable-parsers \
        --disable-protocols \
        --disable-bsfs \
        --disable-indevs \
        --disable-outdevs \
        --disable-filters \
        --enable-decoder=hevc \
        --enable-parser=hevc
make
echo "make install"
make install
