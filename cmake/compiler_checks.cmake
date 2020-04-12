include(CheckCXXCompilerFlag)

# Check if compiler supports flag, if yes add to variable
macro(EnableIfCompilerSupports variable flag)
  string(REGEX REPLACE "^-" "" flagName "${flag}")
  check_cxx_compiler_flag("${flag}" CompilerHasFlag_${flagName})
  if(CompilerHasFlag_${flagName})
    set(${variable} "${${variable}} ${flag}")
  endif()
endmacro()
