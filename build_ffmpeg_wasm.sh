echo "Beginning Build:"

make clean

emconfigure ./configure --cc="emcc" --cxx="em++" --ar="emar" --ranlib="emranlib" --prefix=$(pwd)/../ffmpeg_wasm/ffmpeg \
    --enable-cross-compile --target-os=none --arch=x86_32 --cpu=generic \
    --enable-gpl --enable-version3 \
    --disable-swresample --disable-postproc --disable-logging --disable-everything \
    --disable-programs --disable-asm --disable-doc --disable-network --disable-debug \
    --disable-iconv --disable-sdl2 \ # ������
    --disable-avdevice \  # �豸
    --disable-avformat \ # ��ʽ
    --disable-avfilter \  # �˾�
    --disable-decoders \  # ������
    --disable-encoders \  # ������
    --disable-hwaccels \ # Ӳ������
    --disable-demuxers \ # ���װ
    --disable-muxers \  # ��װ
    --disable-parsers \ # ������
    --disable-protocols \  # Э��
    --disable-bsfs \  # bit stream filter������ת��
    --disable-indevs \  # �����豸
    --disable-outdevs \ #����豸
    --disable-filters \ # �˾�
    --enable-decoder=hevc \ 
    --enable-parser=hevc
make
make install