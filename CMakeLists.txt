CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(starter C)

SET(VENDOR "tizen")
SET(PACKAGE ${PROJECT_NAME})
SET(PKGNAME "org.${VENDOR}.${PACKAGE}")
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(BINDIR "${PREFIX}/bin")
SET(DATADIR "${PREFIX}/share")
SET(LOCALEDIR "${DATADIR}/locale")

INCLUDE_DIRECTORIES(${CMAKE_SOURCE_DIR}/include)

INCLUDE(FindPkgConfig)
SET(PKGS_CHECK_MODULES
        ail
        aul
        capi-system-media-key
        db-util
        dlog
        ecore
        ecore-evas
        ecore-input
        eet
        eina
        elementary
        evas
        heynoti
        sysman
        syspopup-caller
        vconf
        appcore-efl
        glib-2.0
        ui-gadget-1
        bundle
        capi-appfw-application
        capi-system-info
        pkgmgr-info
        libtzplatform-config)

IF (with_x)
        pkg_check_modules(pkgs REQUIRED ${PKGS_CHECK_MODULES} ecore-x utilX x11 xcomposite xext)
ENDIF(with_x)

IF (with_wayland)
        ADD_DEFINITIONS("-DWAYLAND")
        pkg_check_modules(pkgs REQUIRED ${PKGS_CHECK_MODULES})
ENDIF(with_wayland)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Wall")
SET(CMAKE_C_FLAGS_DEBUG "-O0 -g")
SET(CMAKE_C_FLAGS_RELEASE "-O2")

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -Wl,--rpath=${PREFIX}/lib -pie")

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF("${ARCH}" STREQUAL "arm")
	ADD_DEFINITIONS("-DTARGET")
	MESSAGE("add -DTARGET")
ENDIF("${ARCH}" STREQUAL "arm")

ADD_DEFINITIONS("-DVENDOR=\"${VENDOR}\"")
ADD_DEFINITIONS("-DPACKAGE=\"${PACKAGE}\"")
ADD_DEFINITIONS("-DPACKAGE_NAME=\"${PKGNAME}\"")
ADD_DEFINITIONS("-DPREFIX=\"${PREFIX}\"")
ADD_DEFINITIONS("-DLOCALEDIR=\"${LOCALEDIR}\"")
ADD_DEFINITIONS("-D_GNU_SOURCE")

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed")

ADD_EXECUTABLE(${PROJECT_NAME}
	src/hw_key.c
	src/pkg_event.c
	src/menu_daemon.c
	src/starter.c
	src/x11.c
	src/xmonitor.c
	src/lock-daemon.c
	src/lockd-debug.c
	src/lockd-process-mgr.c
	src/lockd-window-mgr.c
)
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS})
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BINDIR})

SET(LOCK_SETTING lock-setting)

ADD_SUBDIRECTORY(${CMAKE_SOURCE_DIR}/${LOCK_SETTING})

# install desktop file & icon
#CONFIGURE_FILE(${PROJECT_NAME}.desktop.in ${PROJECT_NAME}.desktop)
#INSTALL(FILES ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.desktop DESTINATION share/applications)

INSTALL(FILES ${CMAKE_SOURCE_DIR}/rd4starter DESTINATION /etc/init.d
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/rd3starter DESTINATION /etc/init.d
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
#INSTALL to /etc/init.d
