## Process this file with automake to produce Makefile.in

AUTOMAKE_OPTIONS     = 1.4 foreign

# A list of all the files in the current directory which can be regenerated
MAINTAINERCLEANFILES = Makefile.in cmail*.h cmail*.c cmail*~

CLEANFILES       = 

if NEED_FERITE_LIB
libferite = -L${top_builddir}/src -lferite
endif

LDFLAGS          = -L${libdir} @mail_LIBS@ $(libferite) -lssl
INCLUDES         = -I$(top_srcdir)/include -I$(prefix)/include -I$(prefix)/include/imap -I. @mail_CFLAGS@
DEFS             = @pthread_defs@

scripts_DATA     = cmail.fec cmime.fec 
scriptsdir       = @FE_XPLAT_LIBRARY_PATH@

modxml_DATA      = cmail.xml
modxmldir        = @FE_LIBRARY_PATH@/module-descriptions


EXTRA_DIST       = cmail.fec $(scripts_DATA)
pkgdir           = @FE_NATIVE_LIBRARY_PATH@
pkg_LTLIBRARIES  = cmail.la

cmail_la_SOURCES    = \
	cmail_header.h \
	cmail_core.c \
	cmail_misc.c \
	cmail_Mime.c \
	cmail_Mail.c \
	cmail_Encoding.c \
	cmail_Mail_Address.c \
	cmail_Mail_Message.c \
	cmail_Mail_MessagePart.c \
	cmail_Mail_AddressList.c \
	cmail_Mail_MessageHeader.c \
	cmail_Mail_Connection.c \
	cmail_Mail_MessageMultiPart.c \
	imap_utility.c \
	imap_utility.h \
	tingeling.c
cmail_la_LDFLAGS    = -no-undefined -module -avoid-version
cmail_la_LIBADD     = -lc-client -lcrypto -lpam @gss@

cmail_core.c: @MODULE_SRC_PREFIX@/mail/cmail.fec
	@BUILDER@ -m cmail @MODULE_SRC_PREFIX@/mail/cmail.fec

