Name:       geofence-server
Summary:    Geofence Server for Tizen
Version:    0.4.3
Release:    1
Group:      Location/Service
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Source1:    geofence-server.service

%if "%{?profile}" == "tv"
ExcludeArch: %{arm} %ix86 x86_64
%endif

Requires(post):	sqlite
Requires(post):	lbs-server
BuildRequires:  cmake
BuildRequires:  pkgconfig(glib-2.0)
BuildRequires:  pkgconfig(network)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(geofence-dbus)
BuildRequires:  pkgconfig(gio-unix-2.0)
BuildRequires:  pkgconfig(sqlite3)
BuildRequires:  pkgconfig(db-util)
BuildRequires:  pkgconfig(alarm-service)
BuildRequires:  pkgconfig(deviced)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(vconf-internal-keys)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-location-manager)
BuildRequires:  pkgconfig(capi-network-wifi)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(libcore-context-manager)
BuildRequires:  pkgconfig(capi-system-device)
BuildRequires:  pkgconfig(capi-geofence-manager)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  capi-geofence-manager-plugin-devel

%description
Geofence Server for Tizen


%prep
%setup -q


%build
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"

MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DFULLVER=%{version} -DMAJORVER=${MAJORVER} \
	-DLIB_DIR=%{_libdir} -DSYSCONF_DIR=%{_sysconfdir} \

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

#service for systemd is not installed to support only DBus auto activation
#mkdir -p %{buildroot}%{_unitdir_user}/default.target.wants
#install -m 644 %{SOURCE1} %{buildroot}%{_unitdir_user}/geofence-server.service
#ln -s ../geofence-server.service %{buildroot}%{_unitdir_user}/default.target.wants/geofence-server.service

%clean
rm -rf %{buildroot}

%post

%postun -p /sbin/ldconfig

%files
%manifest geofence-server.manifest
%defattr(-,root,root,-)
/usr/bin/geofence-server

/usr/share/dbus-1/services/org.tizen.lbs.Providers.GeofenceServer.service
%config %{_sysconfdir}/dbus-1/session.d/geofence-server.conf

#service for systemd is not installed to support only DBus auto activation
#%{_unitdir_user}/geofence-server.service
#%{_unitdir_user}/default.target.wants/geofence-server.service


%package -n location-geofence-server
Summary:    Geofence Server for Tizen
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}

%description -n location-geofence-server
Geofence Server for Tizen

%post -n location-geofence-server
/sbin/ldconfig

%postun -n location-geofence-server
/sbin/ldconfig

%files -n location-geofence-server
%manifest location-geofence-server.manifest
%{_libdir}/geofence/module/libgeofence.so*
