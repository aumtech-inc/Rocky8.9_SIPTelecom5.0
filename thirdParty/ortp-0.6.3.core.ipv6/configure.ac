dnl Process this file with autoconf to produce a configure script.
AC_INIT(src/rtpmod.c)

VERSION=0.6.3
PACKAGE=ortp

AM_INIT_AUTOMAKE($PACKAGE,$VERSION,no-define)
AM_CONFIG_HEADER(ortp-config.h)

AC_CANONICAL_HOST

dnl enable ipv6 support
AC_ARG_ENABLE(ipv6,
      [  --enable-ipv6    Turn on ipv6 support],
      [case "${enableval}" in
        yes)  ipv6=true;;
        no)   ipv6=false;;
        *) AC_MSG_ERROR(bad value ${enableval} for --enable-ipv6) ;;
      esac],[ipv6=true])
IPV6_CFLAGS=
if test x$ipv6 = xtrue ; then
	IPV6_CFLAGS=-DINET6
fi
AC_SUBST(IPV6_CFLAGS)

AC_ARG_ENABLE(mode64bit,
			[  --enable-mode64bit=[yes/no]   produce a 64-bit library. [default=no]],
			[case "${enableval}" in
				yes) mode64bit_enabled=yes;;
				no) mode64bit_enabled=no;;
				*) AC_MSG_ERROR("Bad value for --enable-mode64bit");;
			esac],
			[mode64bit_enabled=no])

AC_ARG_ENABLE(debug,
			[  --enable-debug=[yes/no]   enables the display of traces showing the execution of the library. [default=yes]],
			[case "${enableval}" in
				yes) debug_enabled=yes;;
				no) debug_enabled=no;;
				*) AC_MSG_ERROR("Bad value for --enable-debug");;
			esac],
			[debug_enabled=no])

dnl enable rtcp protocol
AC_ARG_ENABLE(rtcp,
			[  --enable-rtcp=[yes/no]   enables rtcp protocol. [default=no]],
			[case "${enableval}" in
				yes) rtcp_enabled=yes;;
				no) rtcp_enabled=no;;
				*) AC_MSG_ERROR("Bad value for --enable-rtcp");;
			esac],
			[rtcp_enabled=no])

AC_ARG_ENABLE(bigfdset,
			[  --enable-bigfdset=[yes/no]   enables big fd_set (hpux only). [default=no]],
			[case "${enableval}" in
				yes) AC_DEFINE(_USE_BIG_FDS,1,"Define when fd_set should be bigger (hpux only)") ;;
				no) ;;
				*) AC_MSG_ERROR("Bad value for --enable-bigfdset");;
			esac],
			)



hpux_host=no
posixtimer_interval=10000
PTHREAD_LDFLAGS=

case "$host_os" in
	*hpux*) 
		hpux_host=yes
		AC_DEFINE(HOST_IS_HPUX,1,[Defined if compiling on hpux])
		AC_DEFINE(NOCONNECT,1,[Defined if we should not use connect() on udp sockets])
		;;
	*freebsd*)
		AC_DEFINE(NOCONNECT,1,[Defined if we should not use connect() on udp sockets])
		PTHREAD_LDFLAGS="-pthread"
		;;
esac

dnl Checks for programs.
AC_PROG_CC
AM_PROG_LIBTOOL
AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(no)


dnl Checks for libraries.

AC_ARG_ENABLE(glib,
[  --disable-glib         do not compile with glib.],
use_glib=$enableval,use_glib="yes")

if test "x$use_glib" = "xyes"; then
	AM_PATH_GLIB_2_0(2.0.0, [AC_DEFINE(HAVE_GLIB,1,[Defined if we compile against the glib])],[use_glib=no] ,[gthread])
fi

if test "x$use_glib" != "xyes" ; then
	rtcp_enabled=no ;
fi
if test $rtcp_enabled = "yes"; then
	AC_DEFINE(RTCP,1,[Defined rtcp protocol])
fi


AM_CONDITIONAL(GLIB_SUPPORT,test "x$use_glib" = "xyes")

if test "$GCC" != "yes" ; then
	if test "$hpux_host" = "yes" ; then
		dnl we are probably using HPUX cc compiler, so add a +O2 to CFLAGS
			CFLAGS=" +O2 -g "
		if test x$mode64bit_enabled = xyes ; then
			CFLAGS="$CFLAGS +DA2.0W +DS2.0"
		fi
	fi
else
	CFLAGS="-O2 -g -Wall"
fi

build_scheduler=yes

dnl Check if we have seteuid system call
AC_CHECK_FUNCS(seteuid)

if test "$use_glib" = "no" ; then
	dnl check if we can use the pthread_library
	AC_CHECK_LIB(pthread, pthread_mutex_init, [pthread_enabled=yes], [pthread_enabled=no])
	if test $pthread_enabled = "no" ; then
		build_scheduler=no
	else
		PTHREAD_LIBS="-lpthread"
		PTHREAD_CFLAGS="-D_REENTRANT"
		AC_SUBST(PTHREAD_CFLAGS)
		AC_SUBST(PTHREAD_LIBS)
		AC_SUBST(PTHREAD_LDFLAGS)
	fi
fi



if test $debug_enabled = "yes"; then
	AC_DEFINE(RTP_DEBUG,1,[Defined if debug mode (lots of traces)])
fi

AC_ARG_ENABLE(memcheck,
				[  --enable-memcheck=[yes/no]   enables memory leak detection (HPUX only).],
				[case "${enableval}" in
				yes) memcheck_enabled=yes;;
				no) memcheck_enabled=no;;
				*) AC_MSG_ERROR("Bad value for --enable-memcheck");;
			esac],
				[memcheck_enabled=no])
				
if test "$memcheck_enabled" = "yes" ; then
	if test "$hpux_host" = "yes" ; then
		AC_DEFINE(ENABLE_MEMCHECK,1,[Defined when memory leak checking if enabled])
	else
		echo "WARNING ************ : the memory check option is only available for HPUX."
	fi
fi

AC_ARG_ENABLE(hpuxkernel,
			[  --enable-hpuxkernel   compile the library as kernel module for HP-UX],
			[case "${enableval}" in
				yes) target_is_hpuxkernel=yes;;
				no) target_is_hpuxkernel=no;;
				*) AC_MSG_ERROR("Bad value for --enable-hpuxkernel");;
			esac],
			[target_is_hpuxkernel=no])

if test $target_is_hpuxkernel = "yes" ; then
	AC_DEFINE(TARGET_IS_HPUXKERNEL,1,[Defined if we need oRTP as an HPUX kernel module])
	build_scheduler="no"
else
	ORTPLOG="-DG_LOG_DOMAIN=\"oRTP\""
fi

AM_CONDITIONAL(HPUXKERNEL,test "x$target_is_hpuxkernel" = "xyes") 


dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h sys/time.h unistd.h sys/audio.h linux/soundcard.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME
AC_WORDS_BIGENDIAN

dnl Checks for library functions.
AC_CHECK_FUNCS(select socket strerror)

if test $build_scheduler = "yes"; then
	AC_DEFINE(BUILD_SCHEDULER,1,[Defined if we can build the rtp scheduler])
	if test $hpux_host = "yes" ; then
dnl it seems 10 ms is too fast on hpux and it causes trouble 
		posixtimer_interval=20000
	fi
	
fi

AC_DEFINE_UNQUOTED(POSIXTIMER_INTERVAL,$posixtimer_interval,[Defines the periodicity of the rtp scheduler in microseconds])

dnl ##################################################
dnl # Check for gtk-doc.
dnl ##################################################

AC_ARG_WITH(html-dir, [  --with-html-dir=PATH path to installed docs ])

if test "x$with_html_dir" = "x" ; then
  HTML_DIR='${datadir}/gtk-doc/html'
else
  HTML_DIR=$with_html_dir
fi

AC_SUBST(HTML_DIR)

AC_CHECK_PROG(GTKDOC, gtkdoc-mkdb, true, false)

gtk_doc_min_version=0.6
if $GTKDOC ; then
    gtk_doc_version=`gtkdoc-mkdb --version`
    AC_MSG_CHECKING([gtk-doc version ($gtk_doc_version) >= $gtk_doc_min_version])
    if perl <<EOF ; then
      exit (("$gtk_doc_version" =~ /^[[0-9]]+\.[[0-9]]+$/) &&
            ("$gtk_doc_version" >= "$gtk_doc_min_version") ? 0 : 1);
EOF
      AC_MSG_RESULT(yes)
   else
      AC_MSG_RESULT(no)
      GTKDOC=false
   fi
fi

dnl Let people disable the gtk-doc stuff.
AC_ARG_ENABLE(gtk-doc, [  --enable-gtk-doc  Use gtk-doc to build documentation [default=auto]], enable_gtk_doc="$enableval", enable_gtk_doc=auto)

if test x$enable_gtk_doc = xauto ; then
  if test x$GTKDOC = xtrue ; then
    enable_gtk_doc=yes
  else
    enable_gtk_doc=no
  fi
fi

AM_CONDITIONAL(ENABLE_GTK_DOC, test x$enable_gtk_doc = xyes)



AC_OUTPUT(
Makefile
src/Makefile
docs/Makefile
build/Makefile
build/win32/Makefile
)
