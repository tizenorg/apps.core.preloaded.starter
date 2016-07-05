%bcond_with wayland
%define __usrdir /usr/lib/systemd/user

Name:       starter
Summary:    starter
Version: 0.5.52
Release:    1
Group:      TO_BE/FILLED_IN
License:    Apache-2.0
Source0:    starter-%{version}.tar.gz
Source1:    starter.service
Source2:    starter.path

%if "%{profile}" == "tv" || "%{profile}" == "ivi"
ExcludeArch: %{arm} %ix86 x86_64
%endif

BuildRequires:  cmake
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-system-media-key)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(capi-system-system-settings)

%if "%{profile}" == "common"
BuildRequires:  tts
BuildRequires:  tts-devel
BuildRequires:  pkgconfig(capi-message-port)
BuildRequires:  pkgconfig(efl-extension)
%else if "%{profile}" == "mobile"
BuildRequires:  tts
BuildRequires:  tts-devel
BuildRequires:  pkgconfig(capi-message-port)
BuildRequires:  pkgconfig(capi-telephony)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  pkgconfig(efl-extension)
%endif

BuildRequires:  pkgconfig(feedback)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(ecore-wayland)
BuildRequires:  pkgconfig(edje)
BuildRequires:	pkgconfig(edbus)
BuildRequires:  pkgconfig(eina)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(syspopup-caller)
BuildRequires:  pkgconfig(ui-gadget-1)
BuildRequires:  pkgconfig(vconf)
BuildRequires:	pkgconfig(alarm-service)
BuildRequires:	pkgconfig(pkgmgr-info)
BuildRequires:	pkgconfig(deviced)
BuildRequires:	pkgconfig(edbus)
BuildRequires:  pkgconfig(dbus-1)
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  cmake
BuildRequires:  edje-bin
BuildRequires:  gettext
BuildRequires:  gettext-tools
Requires(post): /usr/bin/vconftool

%if !%{with wayland}
BuildRequires:  pkgconfig(utilX)
%endif

%description
Description: Starter


%prep
%setup -q

%build
%if 0%{?sec_build_binary_debug_enable}
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
%endif

%if "%{profile}" == "common"
%define TIZEN_PROFILE_NAME "COMMON"
export CFLAGS="$CFLAGS -DTIZEN_PROFILE_COMMON"
export CXXFLAGS="$CXXFLAGS -DTIZEN_PROFILE_COMMON"
%endif

%if "%{profile}" == "mobile"
%define TIZEN_PROFILE_NAME "MOBILE"
export CFLAGS="$CFLAGS -DTIZEN_PROFILE_MOBILE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_PROFILE_MOBILE"
%endif

%if "%{profile}" == "wearable"
%define TIZEN_PROFILE_NAME "WEARABLE"
export CFLAGS="$CFLAGS -DTIZEN_PROFILE_WEARABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_PROFILE_WEARABLE"
%endif

%ifarch %{arm}
export CFLAGS="$CFLAGS -DTIZEN_ARCH_ARM"
export CXXFLAGS="$CXXFLAGS -DTIZEN_ARCH_ARM"
%else
export CFLAGS="$CFLAGS -DTIZEN_ARCH_EMULATOR"
export CXXFLAGS="$CXXFLAGS -DTIZEN_ARHC_EMULATOR"
%endif

%ifarch aarch64
export CFLAGS="$CFLAGS -DTIZEN_ARCH_ARM64"
export CXXFLAGS="$CXXFLAGS -DTIZEN_ARCH_ARM64"
%endif

%if %{with wayland}
export WAYLAND_SUPPORT=On
export X11_SUPPORT=Off
%else
export WAYLAND_SUPPORT=Off
export X11_SUPPORT=On
%endif

cmake . -DTIZEN_PROFILE_NAME=%{TIZEN_PROFILE_NAME} -DCMAKE_INSTALL_PREFIX=%{_prefix} -DWAYLAND_SUPPORT=${WAYLAND_SUPPORT} -DX11_SUPPORT=${X11_SUPPORT}

make
make -j1

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{__usrdir}/default.target.wants
mkdir -p %{buildroot}%{_sysconfdir}/systemd/default-extra-dependencies/ignore-units.d/

install -m 0644 %SOURCE1 %{buildroot}%{__usrdir}/starter.service

install -m 0644 %SOURCE2 %{buildroot}%{__usrdir}/starter.path
ln -s ../starter.path %{buildroot}%{__usrdir}/default.target.wants/starter.path

mkdir -p %{buildroot}/usr/share/license
cp -f LICENSE %{buildroot}/usr/share/license/%{name}

%post
sync

%files
%manifest starter.manifest
%defattr(-,root,root,-)
%{_bindir}/starter
%{__usrdir}/starter.service
%{__usrdir}/starter.path
%{__usrdir}/default.target.wants/starter.path
/usr/share/license/%{name}
/usr/share/locale/*/LC_MESSAGES/*
