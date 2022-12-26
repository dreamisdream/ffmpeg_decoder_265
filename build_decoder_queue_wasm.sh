rm libffmpeg_265_queue_va.js
export TOTAL_MEMORY=67108864
export EXPORTED_FUNCTIONS="[ \
	'_init_decoder', \
	'_decode_buffer', \
	'_close_decoder', \
    '_decode_one_packet', \
    '_malloc',
    '_free',
    '_main'
]"

echo "Running Emscripten..."
emcc  test_decoder_queue_va.c ffmpeg_wasm/lib/libavcodec.a ffmpeg_wasm/lib/libavutil.a ffmpeg_wasm/lib/libswscale.a \
    -O3 \
    -I "ffmpeg_wasm/include" \
    -s WASM=1 \
    -s ASSERTIONS=1 \
    -s LLD_REPORT_UNDEFINED=1 \
    -s NO_EXIT_RUNTIME=1 \
    -s DISABLE_EXCEPTION_CATCHING=1 \
    -s TOTAL_MEMORY=${TOTAL_MEMORY} \
   	-s EXPORTED_FUNCTIONS="${EXPORTED_FUNCTIONS}" \
   	-s EXPORTED_RUNTIME_METHODS="['addFunction','removeFunction']" \
	-s RESERVED_FUNCTION_POINTERS=14 \
	-s FORCE_FILESYSTEM=1 \
    -msimd128 -Os -msse \
    -o libffmpeg_265_queue_va.js

echo "Finished Build"
