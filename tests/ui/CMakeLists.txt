add_test(
    NAME rml_contract_guard_tests
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_rml_contracts.py"
)

mu_add_test(
    NAME rml_image_crop_tests
    SOURCES test_rml_image_crop.cpp
    LINK_LIBS RmlUi::Core
)

mu_add_test(
    NAME guard_guild_lists_tests
    SOURCES
        test_guard_guild_lists.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Combat/GuardGuildLists.cpp
)

mu_add_test(
    NAME jewel_unmix_selection_tests
    SOURCES
        test_jewel_unmix_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Items/JewelUnmixSelection.cpp
)

mu_add_test(
    NAME buy_option_selection_tests
    SOURCES
        test_buy_option_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameShop/BuyOptionSelection.cpp
)

mu_add_test(
    NAME purchase_batch_tests
    SOURCES test_purchase_batch.cpp
)

mu_add_test(
    NAME npc_counter_layout_tests
    SOURCES
        test_npc_counter_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(npc_counter_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME storage_item_selection_tests
    SOURCES
        test_storage_item_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameShop/StorageItemSelection.cpp
)

mu_add_test(
    NAME item_grid_geometry_tests
    SOURCES
        test_item_grid_geometry.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/ItemGridGeometry.cpp
)

mu_add_test(
    NAME tooltip_placement_tests
    SOURCES
        test_tooltip_placement.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/RmlBridge/RmlTooltipPlacement.cpp
)

mu_add_test(
    NAME socket_list_selection_tests
    SOURCES
        test_socket_list_selection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/SocketListSelection.cpp
)

mu_add_test(
    NAME experience_text_tests
    SOURCES test_experience_text.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME help_pages_tests
    SOURCES test_help_pages.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME guild_mark_palette_tests
    SOURCES
        test_guild_mark_palette.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Guild/GuildMarkPalette.cpp
)

mu_add_test(
    NAME rml_premultiply_tests
    SOURCES
        test_rml_premultiply.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Render/RmlUi/RmlPremultiply.cpp
)

mu_add_test(
    NAME mini_map_layout_tests
    SOURCES
        test_mini_map_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/MiniMapLayout.cpp
)

mu_add_test(
    NAME rml_stacking_order_tests
    SOURCES
        test_rml_stacking_order.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/RmlBridge/RmlStackingOrder.cpp
)
target_compile_definitions(rml_stacking_order_tests PRIVATE
    MU_CLIENT_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src/source"
)

mu_add_test(
    NAME party_list_layout_tests
    SOURCES
        test_party_list_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Party/PartyListLayout.cpp
)

mu_add_test(
    NAME text_search_tests
    SOURCES
        test_text_search.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Widgets/TextSearch.cpp
)

add_test(
    NAME no_legacy_text_measurement
    COMMAND ${CMAKE_COMMAND}
        -DSOURCE_DIR=${CMAKE_SOURCE_DIR}/src/source
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_no_legacy_text_measurement.cmake
)

add_test(
    NAME inventory_window_captures_hover
    COMMAND ${CMAKE_COMMAND}
        -DMU_INVENTORY_SOURCE=${CMAKE_SOURCE_DIR}/src/source/UI/Inventory/MyInventory.cpp
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_inventory_mouse_capture.cmake
)

add_test(
    NAME npc_name_assets
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/test_npc_name_assets.py"
            "${CMAKE_SOURCE_DIR}"
)

mu_add_test(
    NAME npc_name_storage_tests
    SOURCES
        test_npc_name_storage.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Core/Text/WideString.cpp
)

mu_add_test(
    NAME monster_health_bar_tests
    SOURCES
        test_monster_health_bar.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Combat/MonsterHealthBar.cpp
)

mu_add_test(
    NAME ui_scaling_tests
    SOURCES
        test_ui_scaling.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME mu_helper_tests
    SOURCES
        test_mu_helper.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

add_test(
    NAME display_settings_contract
    COMMAND "${CMAKE_COMMAND}"
        "-DMU_CONFIG_CONSTANTS=${CMAKE_SOURCE_DIR}/src/source/Data/GameConfig/GameConfigConstants.h"
        "-DMU_CONFIG_SOURCE=${CMAKE_SOURCE_DIR}/src/source/Data/GameConfig/GameConfig.cpp"
        "-DMU_WINMAIN_SOURCE=${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp"
        "-DMU_CONSOLE_SOURCE=${CMAKE_SOURCE_DIR}/src/source/Core/Utilities/Log/muConsoleDebug.cpp"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/test_display_settings_contract.cmake"
)

mu_add_test(
    NAME character_name_lifecycle_tests
    SOURCES test_character_name_lifecycle.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME window_geometry_tests
    SOURCES
        test_window_geometry.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Core/WindowGeometry.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME generic_menu_dialog_tests
    SOURCES
        test_generic_menu_dialog.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME friend_window_manager_tests
    SOURCES
        test_friend_window_manager.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME chat_input_focus_tests
    SOURCES
        test_chat_input_focus.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME master_skill_tree_layout_tests
    SOURCES
        test_master_skill_tree_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/MasterSkillTreeLayout.cpp
)

mu_add_test(
    NAME skill_icon_atlas_tests
    SOURCES
        test_skill_icon_atlas.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/SkillIconAtlas.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/MasterSkillTreeLayout.cpp
)

mu_add_test(
    NAME rml_draggable_tests
    SOURCES test_rml_draggable.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME window_placement_layout_tests
    SOURCES test_window_placement_layout.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(window_placement_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME window_group_state_tests
    SOURCES
        test_window_group_state.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/Placement/WindowGroupState.cpp
)

mu_add_test(
    NAME rml_model_binder_tests
    SOURCES test_rml_model_binder.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_themed_view_tests
    SOURCES
        test_rml_themed_view.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_counter_scale_tests
    SOURCES
        test_rml_counter_scale.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_slot_placement_tests
    SOURCES
        test_rml_slot_placement.cpp
        test_rml_element_projection.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_party_trade_layout_tests
    SOURCES
        test_rml_party_trade_layout.cpp
        RmlLayoutFixture.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(rml_party_trade_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
    MU_LOCALIZATION_DIR="${CMAKE_SOURCE_DIR}/src/Localization"
)

mu_add_test(
    NAME item_grid_px_tests
    SOURCES
        test_item_grid_px.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_scale_inputs_tests
    SOURCES
        test_rml_scale_inputs.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_runtime_scale_tests
    SOURCES
        test_rml_runtime_scale.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME theme_file_interface_tests
    SOURCES
        test_theme_file_interface.cpp
        test_theme_inheritance.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(theme_file_interface_tests PRIVATE
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
)

mu_add_test(
    NAME rml_friend_window_identity_tests
    SOURCES test_rml_friend_window_identity.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME generic_dialog_lifetime_tests
    SOURCES
        test_generic_dialog_lifetime.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME rml_document_scene_tests
    SOURCES
        test_rml_document_scene.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

add_executable(rml_text_layout_tests
    test_rml_text_layout.cpp
    RmlTextLayoutAudit.cpp
    RmlLayoutFixture.cpp
    ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
)
target_link_libraries(rml_text_layout_tests PRIVATE mu_test_main MuClient)
target_include_directories(rml_text_layout_tests PRIVATE ${CMAKE_SOURCE_DIR}/src/source)
if(MSVC)
    target_compile_options(rml_text_layout_tests PRIVATE /utf-8)
endif()
add_test(
    NAME rml_text_layout_baseline
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/prepare_rml_text_layout.py"
        --repo "${CMAKE_SOURCE_DIR}"
        --generated "${CMAKE_BINARY_DIR}/Generated"
        --executable "$<TARGET_FILE:rml_text_layout_tests>"
        --output "${CMAKE_BINARY_DIR}/text-layout-baseline"
)
target_compile_definitions(rml_text_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
)

mu_add_test(
    NAME rml_styling_regression_tests
    SOURCES
        test_rml_styling_regressions.cpp
        RmlLayoutFixture.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_include_directories(rml_styling_regression_tests PRIVATE "${MU_RMLUI_SOURCE}/Source/Core")
target_compile_definitions(rml_styling_regression_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
)
mu_add_test(
    NAME compact_hud_state_tests
    SOURCES
        test_compact_hud_state.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/BuffPresentation.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/ResourceMotion.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/ExperienceProgress.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/ActionBarProfile.cpp
        ${CMAKE_SOURCE_DIR}/src/source/UI/HUD/Skills/SkillAvailability.cpp
)

mu_add_test(
    NAME munolith_picker_layout_tests
    SOURCES test_munolith_picker_layout.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(munolith_picker_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME pet_item_options_tests
    SOURCES
        test_pet_item_options.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)

mu_add_test(
    NAME warehouse_layout_tests
    SOURCES
        test_warehouse_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(warehouse_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME trade_layout_tests
    SOURCES
        test_trade_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(trade_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME crafting_layout_tests
    SOURCES
        test_crafting_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(crafting_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME social_layout_tests
    SOURCES
        test_social_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(social_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME helper_library_layout_tests
    SOURCES
        test_helper_library_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(helper_library_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")

mu_add_test(
    NAME inventory_layout_tests
    SOURCES test_inventory_layout.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(inventory_layout_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
)

mu_add_test(
    NAME theme_model_migration_tests
    SOURCES test_theme_model_migration.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient
)
target_compile_definitions(theme_model_migration_tests PRIVATE
    MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
    MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts"
)

# Native boundary stubs use the Linux Itanium ABI, leaving the real control/owner input code linked.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    mu_add_test(
        NAME inventory_input_tests
        SOURCES test_inventory_input.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS MuClient
    )
    target_compile_definitions(inventory_input_tests PRIVATE
        MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi")
    target_link_options(inventory_input_tests PRIVATE
        "LINKER:--wrap=_Z10LoadBitmapPKwjjjbb"
        "LINKER:--wrap=_Z24SendRequestEquipmentItem12STORAGE_TYPEiP7tagITEMS_i")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    mu_add_test(
        NAME tooltip_helper_layout_tests
        SOURCES test_tooltip_helper_layout.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS MuClient
    )
    target_compile_definitions(tooltip_helper_layout_tests PRIVATE
        MU_RMLUI_DIR="${CMAKE_SOURCE_DIR}/src/bin/Data/Interface/RmlUi"
        MU_FONT_DIR="${CMAKE_SOURCE_DIR}/src/bin/fonts")
    # Native renderer fonts require a GPU. Supply the same line-height contract from owned TTF faces.
    target_link_options(tooltip_helper_layout_tests PRIVATE
        "LINKER:--wrap=_ZN19CUIRenderTextSDLTtf10LineHeightEN2UI7Scaling8FontRoleE"
        "LINKER:--wrap=_Z10LoadBitmapPKwjjjbb"
        "LINKER:--wrap=_ZN2mu11GetRendererEv"
        "LINKER:--wrap=_ZN2mu2ui6window11CPickedItem8Render3DEv"
        "LINKER:--wrap=_ZN10GameConfig17SetWindowPositionERKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEEii"
        "LINKER:--wrap=_ZNK10GameConfig17GetWindowPositionERKNSt7__cxx1112basic_stringIwSt11char_traitsIwESaIwEEERiS8_")
endif()

FetchContent_GetProperties(SDL3_ttf SOURCE_DIR MU_SDL_TTF_SOURCE)
add_test(NAME rmlui_transform_patch_reproducibility
    COMMAND ${CMAKE_COMMAND}
        -DMU_ROOT=${CMAKE_SOURCE_DIR}
        -DMU_OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/rmlui_patch_contract
        -DMU_CONFIGURED=${MU_RMLUI_SOURCE}
        -DMU_SDL_SOURCE=${MU_SDL_TTF_SOURCE}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/test_rmlui_patch.cmake)
