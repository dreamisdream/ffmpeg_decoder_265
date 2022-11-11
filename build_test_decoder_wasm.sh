export TOTAL_MEMORY=67108864
export EXPORTED_FUNCTIONS="[ \
    '_init_decoder', \
    '_decode_buffer',\
    '_close_decoder'
]"

echo "Running Emscripten..."
# 入口文件+3个依赖库文件
emcc test_decoder.c ffmpeg_wasm/lib/libavcodec.a ffmpeg_wasm/lib/libavutil.a ffmpeg_wasm/lib/libswscale.a \
    -O3 \
    -I "ffmpeg_wasm/include" \
    -s WASM=1 \ 
    -s ASSERTIONS=1 \
    -s LLD_REPORT_UNDEFINED \
    -s NO_EXIT_RUNTIME=1 \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    -s TOTAL_MEMORY=${TOTAL_MEMORY} \
    -s EXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS}" \
    -s EXTRA_EXPORTED_RUNTIME_METHODS="['addFunction', 'removeFunction']" \
    -s RESERVED_FUNCTION_POINTERS=14 \
    -s FORCE_FILESYSTEM=1 \
    -o ./wasm/libffmpeg265.js
echo "Finished Build"