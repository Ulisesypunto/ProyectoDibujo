# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "Debug")
  file(REMOVE_RECURSE
  [[CMakeFiles\ProyectoDibujo_autogen.dir\AutogenUsed.txt]]
  [[CMakeFiles\ProyectoDibujo_autogen.dir\ParseCache.txt]]
  "ProyectoDibujo_autogen"
  )
endif()
