CMAKE_MINIMUM_REQUIRED(VERSION 2.6)
PROJECT(starter C)

SET(PACKAGE ${PROJECT_NAME})
SET(PKGNAME "org.tizen.${PACKAGE}")
SET(PREFIX ${CMAKE_INSTALL_PREFIX})
SET(BINDIR "${PREFIX}/bin")
SET(DATADIR "${PREFIX}/share")
SET(RESDIR "${PREFIX}/res")
SET(LOCALEDIR "${PREFIX}/share/locale")

IF("${TIZEN_PROFILE_NAME}" STREQUAL "COMMON")

INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include/common
)
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
	aul
	capi-system-media-key
	db-util
	dlog
	ecore
	ecore-wayland
	ecore-evas
	ecore-input
	edbus
	eina
	elementary
	evas
	syspopup-caller
	vconf
	appcore-efl
	glib-2.0
	ui-gadget-1
	bundle
	capi-appfw-application
	capi-appfw-app-manager
	capi-network-bluetooth
	capi-system-system-settings
	feedback
	alarm-service
	pkgmgr-info
	deviced
	edbus
	dbus-1
	dbus-glib-1
	tts
	capi-message-port
	security-manager
	efl-extension
)

ELSEIF("${TIZEN_PROFILE_NAME}" STREQUAL "MOBILE")

INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include/mobile
)
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
	aul
	capi-system-media-key
	db-util
	dlog
	ecore
	ecore-wayland
	ecore-evas
	ecore-input
	edbus
	eina
	elementary
	evas
	syspopup-caller
	vconf
	appcore-efl
	glib-2.0
	ui-gadget-1
	bundle
	capi-appfw-application
	capi-appfw-app-manager
	capi-network-bluetooth
	capi-system-system-settings
	feedback
	alarm-service
	pkgmgr-info
	deviced
	edbus
	dbus-1
	dbus-glib-1
	tts
	capi-message-port
	security-manager
	efl-extension
)

ELSEIF("${TIZEN_PROFILE_NAME}" STREQUAL "WEARABLE")

INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include/wearable
)
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
	aul
	capi-system-media-key
	db-util
	dlog
	ecore
	ecore-evas
	ecore-input
	ecore-wayland
	edbus
	eina
	elementary
	evas
	syspopup-caller
	vconf
	appcore-efl
	glib-2.0
	ui-gadget-1
	bundle
	capi-appfw-application
	capi-appfw-app-manager
	capi-network-bluetooth
	feedback
	alarm-service
	pkgmgr-info
	deviced
	edbus
	dbus-1
	dbus-glib-1
)

ELSEIF("${TIZEN_PROFILE_NAME}" STREQUAL "IVI")

INCLUDE_DIRECTORIES(
	${CMAKE_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/include/ivi
)
INCLUDE(FindPkgConfig)
pkg_check_modules(pkgs REQUIRED
	aul
	capi-system-media-key
	db-util
	dlog
	ecore
	ecore-wayland
	ecore-evas
	ecore-input
	edbus
	eina
	elementary
	evas
	syspopup-caller
	vconf
	appcore-efl
	glib-2.0
	ui-gadget-1
	bundle
	capi-appfw-application
	capi-appfw-app-manager
	capi-network-bluetooth
	capi-system-system-settings
	feedback
	alarm-service
	pkgmgr-info
	deviced
	edbus
	dbus-1
	dbus-glib-1
	tts
	capi-message-port
	security-manager
)

ENDIF()

IF(X11_SUPPORT)
pkg_check_modules(pkgs REQUIRED
	ecore-x
	utilX
	)
ENDIF(X11_SUPPORT)

FOREACH(flag ${pkgs_CFLAGS})
	SET(EXTRA_CFLAGS "${EXTRA_CFLAGS} ${flag}")
ENDFOREACH(flag)

SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EXTRA_CFLAGS} -Wall -fPIE")
SET(CMAKE_C_FLAGS_PROFILING    "-O0 -g -pg -fPIE")
SET(CMAKE_CXX_FLAGS_PROFILING  "-O0 -std=c++0x -g -pg -fPIE")
SET(CMAKE_C_FLAGS_DEBUG        "-O0 -g -fPIE")
SET(CMAKE_CXX_FLAGS_DEBUG      "-O0 -std=c++0x -g -fPIE")
SET(CMAKE_C_FLAGS_RELEASE      "-O2 -g -fPIE")
SET(CMAKE_CXX_FLAGS_RELEASE    "-O2 -std=c++0x -g -fPIE")
SET(CMAKE_C_FLAGS_CCOV         "-O2 -g --coverage -fPIE")
SET(CMAKE_CXX_FLAGS_CCOV       "-O2 -std=c++0x -g --coverage -fPIE")

SET(CMAKE_EXE_LINKER_FLAGS "-Wl,--as-needed -Wl,--rpath=${PREFIX}/lib -pie")

FIND_PROGRAM(UNAME NAMES uname)
EXEC_PROGRAM("${UNAME}" ARGS "-m" OUTPUT_VARIABLE "ARCH")
IF("${ARCH}" STREQUAL "arm")
	ADD_DEFINITIONS("-DTARGET")
	MESSAGE("add -DTARGET")
ENDIF("${ARCH}" STREQUAL "arm")

ADD_DEFINITIONS("-DPACKAGE=\"${PACKAGE}\"")
ADD_DEFINITIONS("-DPACKAGE_NAME=\"${PKGNAME}\"")
ADD_DEFINITIONS("-DPREFIX=\"${PREFIX}\"")
ADD_DEFINITIONS("-DLOCALEDIR=\"${LOCALEDIR}\"")
ADD_DEFINITIONS("-D_GNU_SOURCE")

IF(X11_SUPPORT)
	ADD_DEFINITIONS("-DHAVE_X11")
ENDIF(X11_SUPPORT)

IF(WAYLAND_SUPPORT)
	ADD_DEFINITIONS("-DHAVE_WAYLAND")
ENDIF(WAYLAND_SUPPORT)

IF("${TIZEN_PROFILE_NAME}" STREQUAL "COMMON")

SET(BUILD_SOURCE
	src/dbus_util.c
	src/package_mgr.c
	src/process_mgr.c
	src/status.c

	src/common/home_mgr.c
	src/common/lock_mgr.c
	src/common/starter.c
	src/common/window_mgr.c
	src/common/popup.c
	src/common/hw_key.c
)

ELSEIF("${TIZEN_PROFILE_NAME}" STREQUAL "MOBILE")

SET(BUILD_SOURCE
	src/dbus_util.c
	src/package_mgr.c
	src/process_mgr.c
	src/status.c

	src/mobile/home_mgr.c
	src/mobile/starter.c
	src/mobile/lock_mgr.c
	src/mobile/lock_pwd_util.c
	src/mobile/lock_pwd_simple.c
	src/mobile/lock_pwd_complex.c
	src/mobile/lock_pwd_control_panel.c
	src/mobile/lock_pwd_verification.c
	src/mobile/window_mgr.c
	src/mobile/popup.c
	src/mobile/hw_key.c
)

ELSEIF("${TIZEN_PROFILE_NAME}" STREQUAL "WEARABLE")

SET(BUILD_SOURCE
	src/dbus_util.c
	src/package_mgr.c
	src/process_mgr.c
	src/status.c

	src/wearable/clock_mgr.c
	src/wearable/home_mgr.c
	src/wearable/hourly_alert.c
	src/wearable/hw_key.c
	src/wearable/starter.c
)

ELSEIF("${TIZEN_PROFILE_NAME}" STREQUAL "IVI")

SET(BUILD_SOURCE
	src/dbus_util.c
	src/package_mgr.c
	src/process_mgr.c
	src/status.c

	src/ivi/home_mgr.c
	src/ivi/lock_mgr.c
	src/ivi/starter.c
	src/ivi/window_mgr.c
	src/ivi/popup.c
	src/ivi/hw_key.c
)

ENDIF()

ADD_EXECUTABLE(${PROJECT_NAME} ${BUILD_SOURCE})
TARGET_LINK_LIBRARIES(${PROJECT_NAME} ${pkgs_LDFLAGS})
INSTALL(TARGETS ${PROJECT_NAME} DESTINATION ${BINDIR})
INSTALL(DIRECTORY DESTINATION ${DATADIR})

INSTALL(FILES ${CMAKE_SOURCE_DIR}/rd4starter DESTINATION /etc/init.d
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
INSTALL(FILES ${CMAKE_SOURCE_DIR}/rd3starter DESTINATION /etc/init.d
		PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
		GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
#INSTALL to /etc/init.d

ADD_SUBDIRECTORY(res)
ADD_SUBDIRECTORY(po)
