set(AUCTION_PACKET_SOURCES
    ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHousePacketFormat.cpp
    ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouseMarketPackets.cpp
    ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouseAccountPackets.cpp
    ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouseOutcomePackets.cpp
)

mu_add_test(NAME test_auction_house_packets
    SOURCES test_auction_house_packets.cpp ${AUCTION_PACKET_SOURCES})
mu_add_test(NAME test_auction_house_state
    SOURCES test_auction_house_state.cpp ${AUCTION_PACKET_SOURCES}
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouse.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouseController.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouseNameSearch.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHouseFees.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/AuctionHouse/AuctionHousePrices.cpp)
foreach(auction_test IN ITEMS test_auction_house_packets test_auction_house_state)
    target_compile_definitions(${auction_test} PRIVATE
        AUCTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/auction")
endforeach()

mu_add_test(NAME test_auction_house_items
    SOURCES test_auction_house_items.cpp
        ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
    LINK_LIBS MuClient)
target_include_directories(test_auction_house_items PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)
target_compile_definitions(test_auction_house_items PRIVATE
    AUCTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/fixtures/auction")

mu_add_test(
    NAME test_server_config_validation
    SOURCES
        test_server_config_validation.cpp
        ${CMAKE_SOURCE_DIR}/src/source/Data/GameConfig/GameConfigValidation.cpp
)

mu_add_test(
    NAME test_connection_library_load
    SOURCES
        test_connection_library_load.cpp
    LINK_LIBS
        MuClient
)

# Control socket tests only exist in builds that enable it. Without it, the
# control_socket_leak test proves that none of its sources were compiled into
# the client (same mechanics as tests/editor's editor_leak).
if(ENABLE_CONTROL_SOCKET)
    mu_add_test(
        NAME test_control_ui_lifecycle
        SOURCES test_control_ui_lifecycle.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS MuClient
    )
    target_include_directories(test_control_ui_lifecycle PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_quick_peer_settle
        SOURCES test_quick_peer_settle.cpp
        LINK_LIBS MuClient
    )
    target_include_directories(test_quick_peer_settle PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_quick_peer
        SOURCES test_control_quick_peer.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS MuClient
    )
    target_include_directories(test_control_quick_peer PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_ui_guarded_handler
        SOURCES
            test_control_ui_guarded_handler.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS MuClient
    )
    target_include_directories(test_control_ui_guarded_handler PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_modal_fixture
        SOURCES test_control_modal_fixture.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Platform/Windows/Winmain.cpp
        LINK_LIBS MuClient
    )
    target_include_directories(test_control_modal_fixture PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_ui_replay
        SOURCES test_control_ui_replay.cpp
        LINK_LIBS MuClient
    )
    target_include_directories(test_control_ui_replay PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_ui_observability
        SOURCES
            test_control_ui_observability.cpp
    )

    mu_add_test(
        NAME test_control_character_scene
        SOURCES
            test_control_character_scene.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Control/ControlCharacterSceneReport.cpp
    )
    target_include_directories(test_control_character_scene PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_login_scene
        SOURCES
            test_control_login_scene.cpp
            ${CMAKE_SOURCE_DIR}/src/source/App/Control/ControlLoginSceneReport.cpp
    )
    target_include_directories(test_control_login_scene PRIVATE ${CMAKE_SOURCE_DIR}/src/ThirdParty)

    mu_add_test(
        NAME test_control_protocol
        SOURCES
            test_control_protocol.cpp
        LINK_LIBS
            MuClient
    )

    mu_add_test(
        NAME test_control_events
        SOURCES
            test_control_events.cpp
        LINK_LIBS
            MuClient
    )
else()
    file(GENERATE
        OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/muclient_sources.txt"
        CONTENT "$<JOIN:$<TARGET_PROPERTY:MuClient,SOURCES>,\n>")

    add_test(
        NAME control_socket_leak
        COMMAND ${CMAKE_COMMAND}
            "-DSOURCES_FILE=${CMAKE_CURRENT_BINARY_DIR}/muclient_sources.txt"
            "-DCONTROL_DIR=${PROJECT_SOURCE_DIR}/src/source/App/Control"
            "-DTRANSPORT_FILE=${PROJECT_SOURCE_DIR}/src/source/Core/Platform/LocalSocket.cpp"
            "-DINJECTOR_FILE=${PROJECT_SOURCE_DIR}/src/source/Core/Input/SyntheticInput.cpp"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/test_control_leak.cmake")
endif()

mu_add_test(
    NAME test_chat_command_catalog
    SOURCES
        test_chat_command_catalog.cpp
        ${CMAKE_SOURCE_DIR}/src/source/GameLogic/Commands/ChatCommandCatalog.cpp
)

if(DOTNET_EXECUTABLE)
    add_test(
        NAME login_diagnostics_contract
        COMMAND "${CMAKE_COMMAND}"
            "-DMU_LOGIN_SOURCE=${CMAKE_SOURCE_DIR}/ClientLibrary/ConnectionManager.ClientToServer.Custom.cs"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/test_login_diagnostics.cmake"
    )

    add_test(
        NAME dotnet_binding_generator
        COMMAND "${CMAKE_COMMAND}" -E env --unset=Platform
            "${DOTNET_EXECUTABLE}" run
            --project ${CMAKE_CURRENT_SOURCE_DIR}/binding_generator_self_check/BindingGeneratorSelfCheck.csproj
            --configuration Release
            -- ${CMAKE_SOURCE_DIR}/ClientLibrary/GenerateBindingsHeader.xslt
               ${CMAKE_SOURCE_DIR}/ClientLibrary/GenerateFunctions.xslt
               ${CMAKE_SOURCE_DIR}/src/source/Dotnet
    )

    add_test(
        NAME managed_log_callback
        COMMAND "${CMAKE_COMMAND}" -E env --unset=Platform
            "${DOTNET_EXECUTABLE}" run
            --project ${CMAKE_CURRENT_SOURCE_DIR}/managed_log_self_check/ManagedLogSelfCheck.csproj
            --configuration Release
    )
    set_tests_properties(managed_log_callback PROPERTIES TIMEOUT 30)

    add_test(
        NAME outbound_flush_loop
        COMMAND "${CMAKE_COMMAND}" -E env --unset=Platform
            "${DOTNET_EXECUTABLE}" run
            --project ${CMAKE_CURRENT_SOURCE_DIR}/outbound_sender_self_check/OutboundSenderSelfCheck.csproj
            --configuration Release
    )
    set_tests_properties(outbound_flush_loop PROPERTIES TIMEOUT 30)
endif()
