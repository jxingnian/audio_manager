set(COMPONENT_SRCS 
                    ./play_mp3_control_example.c
                    ./opus_decode_play.c
                    ./opus_encode_recorder.c
                    ./audio_resample_test.c
)
set(COMPONENT_ADD_INCLUDEDIRS "")

register_component(components/* )
