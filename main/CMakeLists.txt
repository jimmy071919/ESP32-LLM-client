set(BOARD_DIR "m5stack/atoms3r-echo-base")
set(BUILTIN_TEXT_FONT font_noto_sans_basic_16_4)
set(BUILTIN_ICON_FONT font_material_symbols_16_4)
set(DEFAULT_EMOJI_COLLECTION noto-color-emoji_64)

set(SOURCES "audio/audio_codec.cc"
            "audio/audio_debugger.cc"
            "audio/audio_service.cc"
            "audio/demuxer/ogg_demuxer.cc"
            "audio/codecs/es8311_audio_codec.cc"
            "audio/engines/afe_audio_engine.cc"
            "notify/notify_player.cc"
            "display/display.cc"
            "display/text_glyph.cc"
            "display/markdown_text.cc"
            "display/zh_convert.cc"
            "display/lcd_display.cc"
            "display/lvgl_display/lvgl_display.cc"
            "display/lvgl_display/emoji_collection.cc"
            "display/lvgl_display/lvgl_theme.cc"
            "display/lvgl_display/lvgl_font.cc"
            "display/lvgl_display/dynamic_glyph_cache.cc"
            "display/lvgl_display/lvgl_image.cc"
            "display/lvgl_display/gif/lvgl_gif.cc"
            "display/lvgl_display/gif/gifdec.c"
            "display/lvgl_display/jpg/image_to_jpeg.cpp"
            "protocols/protocol.cc"
            "protocols/text_glyph_payload.cc"
            "protocols/mqtt_protocol.cc"
            "protocols/websocket_protocol.cc"
            "protocols/notify_channel.cc"
            "boards/common/board.cc"
            "boards/common/wifi_board.cc"
            "boards/common/backlight.cc"
            "boards/common/button.cc"
            "boards/common/i2c_device.cc"
            "mcp_server.cc"
            "system_info.cc"
            "application.cc"
            "ota.cc"
            "settings.cc"
            "location_service.cc"
            "device_state_machine.cc"
            "assets.cc"
            "main.cc"
            )

set(INCLUDE_DIRS "." "display" "display/lvgl_display" "display/lvgl_display/jpg" "audio" "audio/demuxer" "protocols"
                 "${CMAKE_CURRENT_SOURCE_DIR}/boards/common")

file(GLOB BOARD_SOURCES
    ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_DIR}/*.cc
    ${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_DIR}/*.c
)
list(APPEND SOURCES ${BOARD_SOURCES})

set(BOARD_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/boards/${BOARD_DIR}/config.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${BOARD_CONFIG_FILE}")
file(READ "${BOARD_CONFIG_FILE}" BOARD_CONFIG_JSON)
string(REGEX MATCH "\"type\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _match "${BOARD_CONFIG_JSON}")
set(BOARD_TYPE "${CMAKE_MATCH_1}")
string(REGEX MATCH "\"manufacturer\"[ \t\r\n]*:[ \t\r\n]*\"([^\"]+)\"" _match "${BOARD_CONFIG_JSON}")
set(BOARD_MANUFACTURER "${CMAKE_MATCH_1}")
if(NOT BOARD_NAME)
    set(BOARD_NAME ${BOARD_TYPE})
endif()

if(CONFIG_LANGUAGE_EN_US)
    set(LANG_DIR "en-US")
else()
    set(LANG_DIR "zh-TW")
endif()

set(LANG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/language.json")
set(LANG_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/assets/lang_config.h")

file(GLOB LANG_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/${LANG_DIR}/*.ogg)
if(NOT LANG_DIR STREQUAL "en-US")
    file(GLOB EN_US_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/en-US/*.ogg)
    set(EXISTING_NAMES "")
    foreach(SOUND_FILE ${LANG_SOUNDS})
        get_filename_component(FILENAME ${SOUND_FILE} NAME)
        list(APPEND EXISTING_NAMES ${FILENAME})
    endforeach()
    foreach(EN_SOUND ${EN_US_SOUNDS})
        get_filename_component(FILENAME ${EN_SOUND} NAME)
        if(NOT ${FILENAME} IN_LIST EXISTING_NAMES)
            list(APPEND LANG_SOUNDS ${EN_SOUND})
        endif()
    endforeach()
endif()

file(GLOB COMMON_SOUNDS ${CMAKE_CURRENT_SOURCE_DIR}/assets/common/*.ogg)

idf_component_register(SRCS ${SOURCES}
                    EMBED_FILES ${LANG_SOUNDS} ${COMMON_SOUNDS}
                    INCLUDE_DIRS ${INCLUDE_DIRS}
                    WHOLE_ARCHIVE
                    PRIV_REQUIRES
                        esp_pm
                        esp_psram
                        esp_netif
                        esp_driver_gpio
                        esp_driver_spi
                        esp_driver_i2c
                        esp_driver_i2s
                        esp_driver_ledc
                        esp_driver_tsens
                        esp_app_format
                        app_update
                        spi_flash
                        mbedtls
                        efuse
                        xiaozhi-fonts
                    )

string(REGEX MATCH "^font_noto_sans_basic_([0-9]+)_([0-9]+)$" _font_match "${BUILTIN_TEXT_FONT}")
set(TEXT_FONT_SIZE "${CMAKE_MATCH_1}")
set(TEXT_FONT_BPP "${CMAKE_MATCH_2}")

if(NOT CMAKE_BUILD_EARLY_EXPANSION)
    target_compile_options(${COMPONENT_LIB} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++23>)
    target_compile_definitions(${COMPONENT_LIB}
                        PRIVATE BOARD_TYPE=\"${BOARD_TYPE}\" BOARD_NAME=\"${BOARD_NAME}\"
                        PRIVATE BOARD_MANUFACTURER=\"${BOARD_MANUFACTURER}\"
                        PRIVATE BUILTIN_TEXT_FONT=${BUILTIN_TEXT_FONT} BUILTIN_ICON_FONT=${BUILTIN_ICON_FONT}
                        PRIVATE TEXT_FONT_SIZE=${TEXT_FONT_SIZE} TEXT_FONT_BPP=${TEXT_FONT_BPP}
                        )
    idf_component_get_property(lvgl_lib lvgl__lvgl COMPONENT_LIB)
    if(lvgl_lib)
        target_include_directories(${lvgl_lib} PRIVATE "${CMAKE_CURRENT_LIST_DIR}/display")
        if(CONFIG_SPIRAM)
            target_compile_definitions(${lvgl_lib} PRIVATE
                "LV_MEM_POOL_INCLUDE=\"lvgl_psram_pool.h\"")
        elseif(CONFIG_LV_USE_BUILTIN_MALLOC)
            target_compile_definitions(${lvgl_lib} PRIVATE
                "LV_MEM_POOL_INCLUDE=\"lvgl_no_psram_pool.h\"")
        endif()
    endif()
endif()

add_custom_command(
    OUTPUT ${LANG_HEADER}
    COMMAND python ${PROJECT_DIR}/scripts/gen_lang.py
            --language "${LANG_DIR}"
            --output "${LANG_HEADER}"
    DEPENDS
        ${LANG_JSON}
        ${CMAKE_CURRENT_SOURCE_DIR}/assets/locales/en-US/language.json
        ${PROJECT_DIR}/scripts/gen_lang.py
    COMMENT "Generating ${LANG_DIR} language config"
)
add_custom_target(lang_header ALL DEPENDS ${LANG_HEADER})

idf_build_get_property(build_components BUILD_COMPONENTS)
function(find_component_path PATTERN PATH_VAR)
    foreach(COMPONENT ${build_components})
        if(COMPONENT MATCHES "${PATTERN}")
            idf_component_get_property(COMPONENT_PATH ${COMPONENT} COMPONENT_DIR)
            set(${PATH_VAR} "${COMPONENT_PATH}" PARENT_SCOPE)
            break()
        endif()
    endforeach()
endfunction()

find_component_path("espressif__esp-sr" ESP_SR_COMPONENT_PATH)
set(ESP_SR_MODEL_PATH "${ESP_SR_COMPONENT_PATH}/model")
find_component_path("xiaozhi-fonts" NOTO_FONTS_PATH)

function(build_default_assets_bin)
    set(GENERATED_ASSETS_BIN "${CMAKE_BINARY_DIR}/generated_assets.bin")
    string(REPLACE "_basic_" "_common_" COMMON_TEXT_FONT "${BUILTIN_TEXT_FONT}")
    file(GLOB DEFAULT_EMOJI_FILES CONFIGURE_DEPENDS "${NOTO_FONTS_PATH}/png/${DEFAULT_EMOJI_COLLECTION}/*")
    set(DEFAULT_ASSETS_DEPENDENCIES
        "${SDKCONFIG}"
        "${PROJECT_DIR}/scripts/build_default_assets.py"
        ${DEFAULT_EMOJI_FILES}
    )
    foreach(DEP "${NOTO_FONTS_PATH}/cbin/${COMMON_TEXT_FONT}.bin" "${NOTO_FONTS_PATH}/manifest.json")
        if(EXISTS "${DEP}")
            list(APPEND DEFAULT_ASSETS_DEPENDENCIES "${DEP}")
        endif()
    endforeach()

    set(BUILD_ARGS
        "--sdkconfig" "${SDKCONFIG}"
        "--output" "${GENERATED_ASSETS_BIN}"
        "--builtin_text_font" "${BUILTIN_TEXT_FONT}"
        "--emoji_collection" "${DEFAULT_EMOJI_COLLECTION}"
        "--esp_sr_model_path" "${ESP_SR_MODEL_PATH}"
        "--noto_fonts_path" "${NOTO_FONTS_PATH}"
    )
    partition_table_get_partition_info(assets_partition_size "--partition-name assets" "size")
    if(assets_partition_size)
        list(APPEND BUILD_ARGS "--max_size" "${assets_partition_size}")
    endif()

    add_custom_command(
        OUTPUT ${GENERATED_ASSETS_BIN}
        COMMAND python ${PROJECT_DIR}/scripts/build_default_assets.py ${BUILD_ARGS}
        DEPENDS ${DEFAULT_ASSETS_DEPENDENCIES}
        COMMENT "Building default assets.bin based on configuration"
        VERBATIM
    )
    add_custom_target(generated_default_assets ALL DEPENDS ${GENERATED_ASSETS_BIN})
    set(GENERATED_ASSETS_LOCAL_FILE ${GENERATED_ASSETS_BIN} PARENT_SCOPE)
endfunction()

function(get_assets_local_file assets_source assets_local_file_var)
    if(assets_source MATCHES "^https?://")
        get_filename_component(ASSETS_FILENAME "${assets_source}" NAME)
        set(ASSETS_LOCAL_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}")
        set(ASSETS_TEMP_FILE "${CMAKE_BINARY_DIR}/${ASSETS_FILENAME}.tmp")
        if(NOT EXISTS ${ASSETS_LOCAL_FILE})
            file(REMOVE ${ASSETS_TEMP_FILE})
            file(DOWNLOAD ${assets_source} ${ASSETS_TEMP_FILE} STATUS DOWNLOAD_STATUS)
            list(GET DOWNLOAD_STATUS 0 STATUS_CODE)
            if(NOT STATUS_CODE EQUAL 0)
                file(REMOVE ${ASSETS_TEMP_FILE})
                message(FATAL_ERROR "Failed to download ${ASSETS_FILENAME} from ${assets_source}")
            endif()
            file(RENAME ${ASSETS_TEMP_FILE} ${ASSETS_LOCAL_FILE})
        endif()
    elseif(IS_ABSOLUTE "${assets_source}")
        set(ASSETS_LOCAL_FILE "${assets_source}")
    else()
        set(ASSETS_LOCAL_FILE "${CMAKE_CURRENT_SOURCE_DIR}/${assets_source}")
    endif()
    if(NOT EXISTS ${ASSETS_LOCAL_FILE})
        message(FATAL_ERROR "Assets file not found: ${ASSETS_LOCAL_FILE}")
    endif()
    set(${assets_local_file_var} ${ASSETS_LOCAL_FILE} PARENT_SCOPE)
endfunction()

function(configure_assets_models assets_source assets_output)
    set(MODEL_ARGS
        "--sdkconfig" "${SDKCONFIG}"
        "--base_assets" "${assets_source}"
        "--output" "${assets_output}"
        "--esp_sr_model_path" "${ESP_SR_MODEL_PATH}"
        "--name_length" "32"
    )
    partition_table_get_partition_info(assets_limit "--partition-name assets" "size")
    if(assets_limit)
        list(APPEND MODEL_ARGS "--max_size" "${assets_limit}")
    endif()
    add_custom_command(
        OUTPUT "${assets_output}"
        COMMAND python ${PROJECT_DIR}/scripts/build_default_assets.py ${MODEL_ARGS}
        DEPENDS "${assets_source}" "${SDKCONFIG}" "${PROJECT_DIR}/scripts/build_default_assets.py"
        COMMENT "Configuring speech models in custom assets"
        VERBATIM
    )
    add_custom_target(configured_assets ALL DEPENDS "${assets_output}")
    add_dependencies(flash configured_assets)
endfunction()

partition_table_get_partition_info(size "--partition-name assets" "size")
partition_table_get_partition_info(offset "--partition-name assets" "offset")
if("${size}" AND "${offset}")
    if(CONFIG_FLASH_DEFAULT_ASSETS)
        build_default_assets_bin()
        esptool_py_flash_to_partition(flash "assets" "${GENERATED_ASSETS_LOCAL_FILE}")
    elseif(CONFIG_FLASH_CUSTOM_ASSETS)
        get_assets_local_file("${CONFIG_CUSTOM_ASSETS_FILE}" ASSETS_LOCAL_FILE)
        set(CONFIGURED_ASSETS_FILE "${CMAKE_BINARY_DIR}/configured_assets.bin")
        configure_assets_models("${ASSETS_LOCAL_FILE}" "${CONFIGURED_ASSETS_FILE}")
        esptool_py_flash_to_partition(flash "assets" "${CONFIGURED_ASSETS_FILE}")
    endif()
endif()
