# Copyright (C) 2012 Jolla Ltd.
# Contact: Richard Braakman <richard.braakman@jollamobile.com>

Name:       etherpad-stresstest
Summary:    Launch fake clients to put load on an etherpad server
Version:    0.1
Release:    1
Group:      Development/Testing
License:    GPLv2+
URL:        https://bitbucket.org/rbraakman/etherpad-stresstest
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore) >= 4.8.0
BuildRequires:  pkgconfig(QJson)

%description
Test reliability of etherpad-lite by launching an army of fake clients.
The clients don't use etherpad's javascript. They implement the
JSON-based protocol directly and use it to make random changes to
a shared document.

%prep
%setup -q

%build
%qmake 
make %{?jobs:-j%jobs}

%install
rm -rf %{buildroot}
%qmake_install

%files
%defattr(-,root,root,-)
%{_bindir}/etherpad-stresstest
