%define name     viewglob
%define version  0.8.3
%define release  1

Name:          %{name}
Summary:       Adjunct to the shell for graphical environments
Version:       %{version}
Release:       %{release}
Source0:       %{name}-%{version}.tar.gz
Icon:          icon_36x36.xpm
URL:           http://viewglob.sourceforge.net/
Group:         System Environment/Shells
Packager:      Stephen Bach <sjbach@users.sourceforge.net>
BuildRoot:     %{_tmppath}/%{name}-%{version}-%{release}-buildroot
License:       GPL
Requires:      bash, gtk2 >= 2.4.0
BuildRequires: gtk2-devel >= 2.4.0

%description
viewglob is a utility designed to complement the Unix shell in graphical
environments. It tracks changes in the command line as they are typed,
and shows globbing and expansion information in a display showing the
layout of relevant directories.

%prep
%setup -q -a 0

%build
%configure
make %{?_smp_mflags}

%install
rm -rf $RPM_BUILD_ROOT
%makeinstall

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc AUTHORS COPYING COPYING2 ChangeLog INSTALL NEWS README TODO
%{_mandir}/man1/viewglob.1*
%{_mandir}/man1/gviewglob.1*
%{_bindir}/viewglob
%{_bindir}/gviewglob
%{_datadir}/%{name}/getopt.sh
%{_datadir}/%{name}/init-viewglob.sh
%{_datadir}/%{name}/seer
%{_datadir}/%{name}/glob-expand
%{_datadir}/%{name}/gviewglob

%changelog
* Thu Aug 26 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.8.3-1
- Update to 0.8.3

* Mon Aug 23 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.8.2-1
- Creating initial package
