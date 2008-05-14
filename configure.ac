dnl Process this file with autoconf to create configure.

AC_INIT
AC_CONFIG_SRCDIR([mail/cmail.fec])
AM_INIT_AUTOMAKE(cmail-ferite-module, 1.0.0)
AM_CONFIG_HEADER(config.h)

AC_C_BIGENDIAN	      

AC_PROG_CC
AM_PROG_CC_STDC
AC_C_CONST

AC_PATH_PROG(unamepath, uname)
if test "_$unamepath" = _; then
   system="unknown"
else
   AC_MSG_CHECKING(system type)
   system=`$unamepath -s`
   AC_MSG_RESULT($system)

   cygwin=`$unamepath -s | cut -c 1-5`
   if test "$cygwin" = "CYGWIN"; then
      AC_DEFINE([USING_CYGWIN], 1, [We Are Using CygWin])
          system="CYGWIN"
   fi
fi

AM_CONDITIONAL(NEED_FERITE_LIB, test "$system" = "Darwin")
AM_CONDITIONAL(NEED_FERITE_LIB, test "$system" = "CYGWIN")
AM_CONDITIONAL(NEED_FERITE_LIB, test "$system" = "SunOS")

gss=""
if test "$system" = "Linux"; then
	gss="-lcrypt"
fi
if test "$system" = "Darwin"; then
	gss="-lgssapi_krb5 -lpam"
fi
AC_SUBST(gss)

AC_PROG_INSTALL
AC_PROG_MAKE_SET
AM_PROG_LIBTOOL

AM_PATH_FERITE(1.1.0,,[
  echo "You should have ferite 1.1.0 installed for this to work.";
])

sinclude(mail/config.m4)

AC_ARG_WITH(uw-imap,
        [  --with-uw-imap=DIR         use uw-imap in <DIR>],
           CPPFLAGS="$CPPFLAGS -I$withval/include"
	   CFLAGS="$CFLAGS -I$withval/include"
	   LDFLAGS="$LDFLAGS -L$withval/lib"
	   export LD_LIBRARY_PATH="$withval/lib:$LD_LIBRARY_PATH")

AC_CHECK_HEADER(imap/c-client.h,,
     AC_MSG_ERROR(header not found) )

AC_CHECK_LIB(c-client, main,,  
     AC_MSG_ERROR(library not found), ) 

mail_CFLAGS="$mail_CFLAGS $CPPFLAGS $FERITE_CFLAGS"
mail_LIBS="$mail_LIBS $FERITE_LIBS"

MODULE_SRC_PREFIX="\${top_srcdir}"
AC_SUBST(MODULE_SRC_PREFIX)

#dnl we do this because the threading libs are already passed through ferite-config
pthread_defs=""
AC_SUBST(pthread_defs)

AC_CONFIG_FILES([
Makefile
mail/Makefile
])
AC_CONFIG_COMMANDS([default],[[
]],[[]])
 
AC_OUTPUT
