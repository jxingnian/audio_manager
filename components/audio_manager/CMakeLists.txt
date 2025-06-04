idf_component_register(
    SRCS    "audio_manager.c"
    INCLUDE_DIRS    "include"
    REQUIRES    audio_pipeline
                audio_stream
) 