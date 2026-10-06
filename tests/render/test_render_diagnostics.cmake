cmake_minimum_required(VERSION 3.25)

file(READ "${MU_SCENE_MANAGER_SOURCE}" scene_source)

foreach(required_text IN ITEMS
    "[RENDER diag] requested={} submitted={} pipeline_binds={} sampler_binds={}"
    "glyph_uploads={} skin_gpu={} skin_cpu_ineligible={} skin_failed={}"
)
    string(FIND "${scene_source}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing rendering parity diagnostic: ${required_text}")
    endif()
endforeach()

message(STATUS "Rendering parity diagnostics: OK")

file(READ "${MU_DIAGNOSTICS_RML}" diagnostics_markup)
foreach(required_text IN ITEMS "{{requested}}" "{{submitted}}" "{{textures_uploaded}}" "{{glyphs_uploaded}}" "pass.draws" "pass.merged")
    string(FIND "${diagnostics_markup}" "${required_text}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "missing themed rendering diagnostic: ${required_text}")
    endif()
endforeach()
