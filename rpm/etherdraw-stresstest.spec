# Copyright (C) 2012 Jolla Ltd.
# Copyright (C) 2013 John McLear
# Contact: Richard Braakman <richard.braakman@jollamobile.com>, or John McLear <john@mclear.co.uk>

Name:       etherdraw-stresstest
Summary:    Launch fake clients to put load on an etherdraw server
Version:    0.1
Release:    1
Group:      Development/Testing
License:    GPLv2+
URL:        https://github.com/JohnMcLear/etherdraw-stresstest
Source0:    %{name}-%{version}.tar.bz2
BuildRequires:  pkgconfig(QtCore) >= 4.8.0
BuildRequires:  pkgconfig(QJson)

%description
Test reliability of etherdraw by launching an army of fake clients.
The clients don't use etherdraw's javascript. They implement the
JSON-based protocol directly and use it to make random changes to
a shared drawing.

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
%{_bindir}/draw-stresstest
