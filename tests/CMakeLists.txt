find_package(Qt6 REQUIRED COMPONENTS Test)

qt_add_executable(tst_systalia_engine
    tst_SystaliaEngine.cpp
)

target_link_libraries(tst_systalia_engine PRIVATE
    Qt6::Core
    Qt6::Test
    sge_core
    sge_shaders
)

add_test(NAME tst_systalia_engine COMMAND tst_systalia_engine)
