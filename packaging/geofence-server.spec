Name:       geofence-server
Summary:    Geofence Server for Tizen
Version:    0.4.1
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
BuildRequires:  pkgconfig(capi-appfw-app-manager)
BuildRequires:  pkgconfig(capi-location-manager)
BuildRequires:  pkgconfig(capi-network-wifi)
BuildRequires:  pkgconfig(capi-network-bluetooth)
BuildRequires:  pkgconfig(secure-storage)
BuildRequires:  pkgconfig(libcore-context-manager)
#BuildRequires:  pkgconfig(tapi)
#BuildRequires:  pkgconfig(capi-telephony-network-info)
#BuildRequires:  pkgconfig(capi-context-manager)
BuildRequires:  pkgconfig(capi-geofence-manager)
BuildRequires:  pkgconfig(libtzplatform-config)
BuildRequires:  capi-geofence-manager-plugin-devel
Requires:  sys-assert

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

#[Workaround] create service file for systemd
mkdir -p %{buildroot}%{_unitdir_user}/multi-user.target.wants
install -m 644 %{SOURCE1} %{buildroot}%{_unitdir_user}/geofence-server.service
ln -s ../geofence-server.service %{buildroot}%{_unitdir_user}/multi-user.target.wants/geofence-server.service

%if 0
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
%endif

%clean
rm -rf %{buildroot}

%post

%if 0
GEOFENCE_SERVER_DB_PATH="/opt/dbspace/.geofence-server.db"

# geofence-server db file
chown system:system /opt/dbspace/.geofence-server.db
chown system:system /opt/dbspace/.geofence-server.db-journal
# Change geofence-server db file permissions
chmod 660 /opt/dbspace/.geofence-server.db
chmod 660 /opt/dbspace/.geofence-server.db-journal
%endif

%postun -p /sbin/ldconfig

%files
%manifest geofence-server.manifest
%defattr(-,root,root,-)
/usr/bin/geofence-server

/usr/share/dbus-1/services/org.tizen.lbs.Providers.GeofenceServer.service
#/opt/dbspace/.*.db*
%config %{_sysconfdir}/dbus-1/system.d/geofence-server.conf

#[Workaround] create service file for systemd
%{_unitdir_user}/geofence-server.service
%{_unitdir_user}/multi-user.target.wants/geofence-server.service


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
