%define name     viewglob
%define version  1.0
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
%__strip $RPM_BUILD_ROOT/%{_libdir}/%{name}/* || :

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root,0755)
%doc AUTHORS COPYING COPYING2 ChangeLog HACKING INSTALL NEWS README TODO
%{_mandir}/man1/viewglob.1*
%{_mandir}/man1/gviewglob.1*
%{_bindir}/viewglob
%{_bindir}/gviewglob
%{_libdir}/%{name}/getopt.sh
%{_libdir}/%{name}/init-viewglob.bashrc
%{_libdir}/%{name}/.zshrc
%{_libdir}/%{name}/seer
%{_libdir}/%{name}/glob-expand
%{_libdir}/%{name}/gviewglob

%changelog
* Thu Sep 28 2004 Stephen Bach <sjbach@users.sourceforge.net> 1.0-1
- Update to 1.0

* Thu Sep 20 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.9.1-1
- Update to 0.9.1

* Thu Sep 09 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.9-1
- Update to 0.9

* Tue Aug 31 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.8.4-1
- Update to 0.8.4

* Thu Aug 26 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.8.3-1
- Update to 0.8.3

* Mon Aug 23 2004 Stephen Bach <sjbach@users.sourceforge.net> 0.8.2-1
- Creating initial package
