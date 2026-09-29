add_executable(tst_document tst_document.cpp)
target_link_libraries(tst_document PRIVATE dibujo_lib Qt6::Test)
add_test(NAME tst_document COMMAND tst_document)
