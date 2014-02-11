%bcond_with x
%bcond_with wayland

Name:       starter
Summary:    Starter
Version:    0.4.62
Release:    3
Group:      Base/Startup
License:    Flora
Source0:    starter-%{version}.tar.gz
Source1:    starter.service
Source2:    starter.path
Source1001: starter.manifest

BuildRequires:  cmake
BuildRequires:  pkgconfig(ail)
BuildRequires:  pkgconfig(appcore-efl)
BuildRequires:  pkgconfig(aul)
BuildRequires:  pkgconfig(capi-appfw-application)
BuildRequires:  pkgconfig(capi-system-media-key)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(ecore)
BuildRequires:  pkgconfig(edje)
BuildRequires:  pkgconfig(eet)
BuildRequires:  pkgconfig(eina)
BuildRequires:  pkgconfig(elementary)
BuildRequires:  pkgconfig(evas)
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(heynoti)
BuildRequires:  pkgconfig(sysman)
BuildRequires:  pkgconfig(syspopup-caller)
BuildRequires:  pkgconfig(tapi)
BuildRequires:  pkgconfig(ui-gadget-1)
%if %{with x}
BuildRequires:  pkgconfig(utilX)
BuildRequires:  pkgconfig(x11)
BuildRequires:  pkgconfig(xcomposite)
BuildRequires:  pkgconfig(xext)
BuildRequires:  pkgconfig(ecore-x)
%endif
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  pkgconfig(pkgmgr-info)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  cmake
BuildRequires:  edje-bin
BuildRequires:  gettext
BuildRequires:  gettext-tools
Requires(post): vconf
Requires:       tizen-platform-config-tools

%description
Description: Starter

%prep
%setup -q
cp %{SOURCE1001} .

%build
%cmake . \
%if %{with wayland} && !%{with x}
-Dwith_wayland=TRUE
%else
-Dwith_x=TRUE
%endif

make -j1
%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_libdir}/systemd/user/core-efl.target.wants
install -m 0644 %SOURCE1 %{buildroot}%{_libdir}/systemd/user/
install -m 0644 %SOURCE2 %{buildroot}%{_libdir}/systemd/user/
ln -s ../starter.path %{buildroot}%{_libdir}/systemd/user/core-efl.target.wants/starter.path
mkdir -p %{buildroot}%{TZ_SYS_SHARE}/license
cp -f LICENSE.Flora %{buildroot}%{TZ_SYS_SHARE}/license/%{name}
mkdir -p %{buildroot}%{TZ_SYS_DATA}/home-daemon

%find_lang ug-lockscreen-options

%post
/sbin/ldconfig
change_file_executable()
{
    chmod +x $@ 2>/dev/null
    if [ $? -ne 0 ]; then
        echo "Failed to change the perms of $@"
    fi
}

USER_GROUP_ID=$(getent group %{TZ_SYS_USER_GROUP} | cut -d: -f3)
GOPTION="-g $USER_GROUP_ID -f"

vconftool set -t int "memory/starter/sequence" 0 -i $GOPTION
vconftool set -t int "memory/starter/use_volume_key" 0 -i $GOPTION
vconftool set -t string file/private/lockscreen/pkgname "org.tizen.lockscreen" $GOPTION
vconftool set -t int memory/idle_lock/state "0" -i $GOPTION
vconftool set -t bool memory/lockscreen/phone_lock_verification 0 -i $GOPTION

vconftool set -t bool db/lockscreen/event_notification_display 1 $GOPTION
vconftool set -t bool db/lockscreen/clock_display 1 $GOPTION
vconftool set -t bool db/lockscreen/help_text_display 0 $GOPTION

vconftool set -t int memory/idle-screen/is_idle_screen_launched "0" -i $GOPTION
vconftool set -t int memory/idle-screen/top "0" -i -f
vconftool set -t int memory/idle-screen/safemode "0" -i -f

ln -sf /etc/init.d/rd4starter /etc/rc.d/rc4.d/S81starter
ln -sf /etc/init.d/rd3starter /etc/rc.d/rc3.d/S43starter

%postun -p /sbin/ldconfig

%files -f ug-lockscreen-options.lang
%manifest %{name}.manifest
%defattr(-,root,root,-)
%{_sysconfdir}/init.d/rd4starter
%{_sysconfdir}/init.d/rd3starter
%{_bindir}/starter
/usr/ug/lib/libug-lockscreen-options.so
/usr/ug/lib/libug-lockscreen-options.so.0.1.0
%{_libdir}/systemd/user/starter.path
%{_libdir}/systemd/user/starter.service
%{_libdir}/systemd/user/core-efl.target.wants/starter.path
%{TZ_SYS_SHARE}/license/%{name}
%{TZ_SYS_DATA}/home-daemon
