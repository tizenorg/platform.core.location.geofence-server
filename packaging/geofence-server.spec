Name:       geofence-server
Summary:    Geofence Server for Tizen
Version:    0.3.9
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
#BuildRequires:  pkgconfig(tapi)
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
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-location-manager)
#BuildRequires:  pkgconfig(capi-geofence-manager)
#BuildRequires: pkgconfig(capi-telephony-network-info)
BuildRequires: pkgconfig(capi-network-wifi)
BuildRequires: pkgconfig(capi-network-bluetooth)
#BuildRequires: pkgconfig(capi-context-manager)
BuildRequires: pkgconfig(secure-storage)
BuildRequires: pkgconfig(libcore-context-manager)
Requires:  sys-assert

%description
Geofence Server for Tizen

%package -n location-geofence-server
Summary:    Geofence Server for Tizen
Group:      Location/Libraries
Requires:   %{name} = %{version}-%{release}

%description -n location-geofence-server
Geofence Server for Tizen

%package -n geofence-server-devel
Summary:    Geofence Server for Tizen (Development)
Group:      Location/Development
Requires:   %{name} = %{version}-%{release}

%description -n geofence-server-devel
Geofence Server for Tizen (Development)

%prep
%setup -q


%build
export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"

MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`
cmake . -DCMAKE_INSTALL_PREFIX=%{_prefix} -DFULLVER=%{version} -DMAJORVER=${MAJORVER} -DLIB_DIR=%{_libdir} \

make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants
install -m 644 %{SOURCE1} %{buildroot}%{_libdir}/systemd/system/geofence-server.service
ln -s ../geofence-server.service %{buildroot}%{_libdir}/systemd/system/multi-user.target.wants/geofence-server.service

chmod 755 %{buildroot}/etc/rc.d/init.d/geofence-server
mkdir -p %{buildroot}/etc/rc.d/rc5.d
ln -sf ../init.d/geofence-server %{buildroot}/etc/rc.d/rc5.d/S91geofence-server

if [ ! -e "$GEOFENCE_SERVER_DB_PATH" ]
then

# create db
mkdir -p %{buildroot}/opt/dbspace
sqlite3 %{buildroot}/opt/dbspace/.geofence-server.db 'PRAGMA journal_mode = PERSIST;
	CREATE TABLE Places ( place_id INTEGER PRIMARY KEY AUTOINCREMENT, access_type INTEGER, place_name TEXT NOT NULL, app_id TEXT NOT NULL);
	CREATE TABLE GeoFence ( fence_id INTEGER PRIMARY KEY AUTOINCREMENT, place_id INTEGER, enable INTEGER, app_id TEXT NOT NULL, geofence_type INTEGER, access_type INTEGER, running_status INTEGER, FOREIGN KEY(place_id) REFERENCES Places(place_id) ON DELETE CASCADE);
	CREATE TABLE FenceGeocoordinate ( fence_id INTEGER , latitude TEXT NOT NULL, longitude TEXT NOT NULL, radius TEXT NOT NULL, address TEXT, FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE);
	CREATE TABLE FenceGeopointWifi ( fence_id INTEGER, bssid TEXT, ssid TEXT, FOREIGN KEY(fence_id) REFERENCES GeoFence(fence_id) ON DELETE CASCADE);
	CREATE TABLE FenceBssid ( fence_id INTEGER, bssid TEXT, ssid TEXT, FOREIGN KEY(fence_id) REFERENCES Geofence(fence_id) ON DELETE CASCADE);'
fi

%clean
rm -rf %{buildroot}

%post
GEOFENCE_SERVER_DB_PATH="/opt/dbspace/.geofence-server.db"

# geofence-server db file
chown system:system /opt/dbspace/.geofence-server.db
chown system:system /opt/dbspace/.geofence-server.db-journal
## Change geofence-server db file permissions
chmod 660 /opt/dbspace/.geofence-server.db
chmod 660 /opt/dbspace/.geofence-server.db-journal

%postun -p /sbin/ldconfig

%files
%manifest geofence-server.manifest
%defattr(-,system,system,-)
/usr/bin/geofence-server
/usr/share/dbus-1/system-services/org.tizen.lbs.Providers.GeofenceServer.service
/etc/rc.d/init.d/geofence-server
/etc/rc.d/rc5.d/S91geofence-server
%{_libdir}/systemd/system/geofence-server.service
%{_libdir}/systemd/system/multi-user.target.wants/geofence-server.service
/opt/dbspace/.*.db*

%files -n location-geofence-server
%manifest location-geofence-server.manifest
%{_libdir}/geofence/module/libgeofence.so*
