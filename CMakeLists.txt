cmake_minimum_required(VERSION 3.10)
project(miniShell C)

set(CMAKE_C_STANDARD 11)

# Incluye el directorio con headers
include_directories(Main)

# Ejecutable principal: tu shell real
add_executable(miniShell
        Main/myshell.c   # contiene main()
        Main/parser.c    # implementación del parser
        Main/test.c
        Main/myshell.c      # si quieres compilarlo, opcional
)

# Si usas Homebrew (macOS ARM), incluye readline
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

# Enlazar readline
target_link_libraries(miniShell readline)
