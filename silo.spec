Summary: The SILO boot loader for SPARCs.
Name: silo
Version: 1.4.11
Release: 1
Copyright: GPL
ExclusiveArch: sparc
Group: System Environment/Base
Source: http://www.sparc-boot.org/pub/silo/silo-%{version}.tar.gz

BuildRoot: /var/tmp/%{name}-root

%description
The silo package installs the SILO (Sparc Improved LOader) boot
loader, which you'll need to boot Red Hat Linux on a SPARC.  SILO
installs onto your system's boot block and can be configured to boot
Linux, Solaris and SunOS.

%prep 
%setup -q -n silo-%{version}

%build
make

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-,root,root)
%doc docs COPYING ChangeLog
/sbin/silo
/usr/bin/tilo
/usr/bin/maketilo
/boot/first.b
/boot/ultra.b
/boot/isofs.b
/boot/fd.b
/boot/ieee32.b
/boot/silotftp.b
/boot/second.b
/usr/sbin/silocheck
/usr/share/man/man1/tilo.1*
/usr/share/man/man1/maketilo.1*
/usr/share/man/man5/silo.conf.5*
/usr/share/man/man8/silo.8*
 
%changelog
* Mon May 28 2001 Pieter Krul <pkrul@auxio.org>
- Updated manpath and added tilo

* Tue May 22 2001 Ben Collins <bcollins@debian.org>
- Updated for new build

* Fri Jul 14 2000 Jakub Jelinek <jakub@redhat.com>
- Pete Zaitcev's JavaStation flash patch
- work around 2.4 kernel headers

* Tue Mar  7 2000 Jakub Jelinek <jakub@redhat.com>
- Set partat0 correctly

* Tue Feb 15 2000 Jakub Jelinek <jakub@redhat.com>
- RAID1 fixes
- package ieee32 bootblock

* Mon Feb 14 2000 Jakub Jelinek <jakub@redhat.com>
- fix -J operation (hopefully)

* Fri Feb  4 2000 Jakub Jelinek <jakub@redhat.com>
- RAID1 support
- man pages are compressed

* Thu Nov 11 1999 Jakub Jelinek <jakub@redhat.com>
- fix reading above 4GB

* Wed Nov  3 1999 Jakub Jelinek <jakub@redhat.com>
- for new kernels, put initial ramdisk right after
  their _end if it fits, should fix many problems with
  initrd loading.
- removed bootfile patch - it was not working anyway
  and AFAIK noone used it

* Tue Oct 12 1999 Jakub Jelinek <jakub@redhat.com>
- fixed a horrible bug introduced in the last release.
  Thanks DaveM.

* Mon Oct 11 1999 Jakub Jelinek <jakub@redhat.com>
- fix support for less buggy PROMs (those which are able
  to read above 1GB, but not 2GB).

* Fri Oct  8 1999 Jakub Jelinek <jakub@redhat.com>
- use some partition device and not whole device if
  possible for masterboot installs
- remove a lot of old cruft, including -d, -o and -O
  options
- no longer do any heuristic about where to put
  the bootblock, simply if -t is not given, it goes
  to bootblock at cylinder 0, otherwise to the
  partition bootblock
- make silotftp.b work

* Wed Oct  6 1999 Jakub Jelinek <jakub@redhat.com>
- brown paper bag bugfix to get cd booting of
  gzipped images working

* Mon Oct  4 1999 Jakub Jelinek <jakub@redhat.com>
- fix bug on traversing non-absolute symlinks

* Tue Sep 28 1999 Jakub Jelinek <jakub@redhat.com>
- don't install silo in %post.

* Thu Sep 23 1999 Jakub Jelinek <jakub@redhat.com>
- actually install the new man page.

* Mon Sep 20 1999 Jakub Jelinek <jakub@redhat.com>
- update to 0.8.8.

* Fri Sep 17 1999 Jakub Jelinek <jakub@redhat.com>
- update to 0.8.7.

* Tue Apr 20 1999 Jakub Jelinek <jj@ultra.linux.cz>
- update to 0.8.6.

* Tue Apr 13 1999 Jeff Johnson <jbj@redhat.com>
- update to pre0.8.6-1.

* Mon Mar 22 1999 Bill Nottingham <notting@redhat.com>
- fix password checking (bug #1054)

* Sun Mar 21 1999 Cristian Gafton <gafton@redhat.com> 
- auto rebuild in the new build environment (release 4)

* Fri Mar 12 1999 Jeff Johnson <jbj@redhat.com>
- add -lc to work around mis-built libext2fs.so

* Thu Dec 17 1998 Jeff Johnson <jbj@redhat.com>
- update to 0.8.5.

* Thu Nov  5 1998 Jeff Johnson <jbj@redhat.com>
- update to 0.8.2.

* Mon Oct 19 1998 Jeff Johnson <jbj@redhat.com>
- update to 0.8.1.

* Wed Sep 30 1998 Jeff Johnson <jbj@redhat.com>
- update to 0.8.
- run silo automagically on install.

* Wed Sep 23 1998 Jeff Johnson <jbj@redhat.com>
- update to pre-0.7.3-3.

* Tue Aug  4 1998 Jeff Johnson <jbj@redhat.com>
- build root
