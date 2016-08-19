ExternalProject_Add(libuv
  GIT_REPOSITORY https://github.com/libuv/libuv.git
  GIT_TAG v1.9.1
  CONFIGURE_COMMAND <SOURCE_DIR>/configure --prefix=<INSTALL_DIR>
  BUILD_COMMAND ${MAKE}
  )
