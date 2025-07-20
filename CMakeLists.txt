# 1v1 Arena Tournament System Module

AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/npc_arena1v1.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/TournamentSystem.cpp")
AC_ADD_SCRIPT("${CMAKE_CURRENT_LIST_DIR}/src/tournament_commands.cpp")

AC_ADD_SCRIPT_LOADER("npc_1v1arena" "${CMAKE_CURRENT_LIST_DIR}/src/npc_arena1v1.cpp")
AC_ADD_SCRIPT_LOADER("TournamentSystem" "${CMAKE_CURRENT_LIST_DIR}/src/TournamentSystem.cpp")
AC_ADD_SCRIPT_LOADER("tournament_commandscript" "${CMAKE_CURRENT_LIST_DIR}/src/tournament_commands.cpp")

message(">> Loaded 1v1 Arena Tournament System module.")
