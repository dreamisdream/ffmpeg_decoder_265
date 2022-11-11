echo "Beginning Build:"

make clean

emconfigure ./configure --cc="emcc" --cxx="em++" --ar="emar" --ranlib="emranlib" --prefix=$(pwd)/../ffmpeg_wasm/ffmpeg \
    --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --enable-gpl --enable-version3 \
    --disable-swresample --disable-postproc --disable-logging --disable-everything \
    --disable-programs --disable-asm --disable-doc --disable-network --disable-debug \
    --disable-iconv --disable-sdl2 \ # 三方库
    --disable-avdevice \  # 设备
    --disable-avformat \ # 格式
    --disable-avfilter \  # 滤镜
    --disable-decoders \  # 解码器
    --disable-encoders \  # 编码器
    --disable-hwaccels \ # 硬件加速
    --disable-demuxers \ # 解封装
    --disable-muxers \  # 封装
    --disable-parsers \ # 解析器
    --disable-protocols \  # 协议
    --disable-bsfs \  # bit stream filter，码流转换
    --disable-indevs \  # 输入设备
    --disable-outdevs \ #输出设备
    --disable-filters \ # 滤镜
    --enable-decoder=hevc \ 
    --enable-parser=hevc
make
make install