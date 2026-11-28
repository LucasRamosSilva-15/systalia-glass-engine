# Geração determinística de ValoTokens.hpp e ValoTheme.qml a partir de valo.tokens.json (Single Source of Truth)
find_package(Python3 REQUIRED COMPONENTS Interpreter)

set(SGE_VALO_TOKENS_JSON "${CMAKE_CURRENT_SOURCE_DIR}/src/theme/tokens/valo.tokens.json")
set(SGE_VALO_TOKENS_HPP "${CMAKE_CURRENT_SOURCE_DIR}/src/theme/tokens/ValoTokens.hpp")
set(SGE_VALO_THEME_QML "${CMAKE_CURRENT_SOURCE_DIR}/src/theme/qml/ValoTheme.qml")
set(SGE_VALO_TOKENS_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/tools/codegen/generate_valo_tokens.py")

execute_process(
    COMMAND "${Python3_EXECUTABLE}" "${SGE_VALO_TOKENS_SCRIPT}"
            --input "${SGE_VALO_TOKENS_JSON}"
            --hpp "${SGE_VALO_TOKENS_HPP}"
            --qml "${SGE_VALO_THEME_QML}"
    RESULT_VARIABLE _sge_tokens_rc
)
if(NOT _sge_tokens_rc EQUAL 0)
    message(FATAL_ERROR "Falha ao gerar tokens VALO a partir de ${SGE_VALO_TOKENS_JSON}")
endif()

add_custom_target(sge_tokens
    COMMAND "${Python3_EXECUTABLE}" "${SGE_VALO_TOKENS_SCRIPT}"
            --input "${SGE_VALO_TOKENS_JSON}"
            --hpp "${SGE_VALO_TOKENS_HPP}"
            --qml "${SGE_VALO_THEME_QML}"
    DEPENDS "${SGE_VALO_TOKENS_JSON}" "${SGE_VALO_TOKENS_SCRIPT}"
    COMMENT "Regenerando ValoTokens.hpp e ValoTheme.qml a partir de valo.tokens.json"
)
