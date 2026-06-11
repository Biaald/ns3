build_lib(
  LIBNAME lora-combining
  SOURCE_FILES lora-signal-combining.cc
  HEADER_FILES lora-signal-combining.h
  LIBRARIES_TO_LINK
    ${libcore}
    ${libnetwork}
    ${libmobility}
)
