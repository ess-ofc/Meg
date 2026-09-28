add_executable(megc)
target_sources(
   megc
   PUBLIC
   FILE_SET HEADERS BASE_DIRS include
      FILES
      include/megc/cgen.h
      include/megc/tymap.h
      include/megc/sema.h
      include/megc/strpool.h
      include/megc/parser.h
      include/megc/lexer.h
      include/megc/token.h
      include/megc/ast.h
      include/megc/loc.h
      include/megc/diagno.h
      include/megc/main.h
   PRIVATE
   src/cgen.c
   src/tymap.c
   src/sema.c
   src/strpool.c
   src/parser.c
   src/lexer.c
   src/token.c
   src/ast.c
   src/diagno.c
   src/main.c
)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

target_link_libraries(megc PUBLIC LLVM)
