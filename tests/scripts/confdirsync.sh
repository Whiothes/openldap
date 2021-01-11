#! /bin/sh
# $OpenLDAP$
## This work is part of OpenLDAP Software <http://www.openldap.org/>.
##
## Copyright 1998-2021 The OpenLDAP Foundation.
## All rights reserved.
##
## Redistribution and use in source and binary forms, with or without
## modification, are permitted only as authorized by the OpenLDAP
## Public License.
##
## A copy of this license is available in the file LICENSE in the
## top-level directory of the distribution or, alternatively, at
## <http://www.OpenLDAP.org/license.html>.
sed -e "s/@BASEDN@/${BASEDN}/"				\
	-e "s/@MSAD_ADMINDN@/${MSAD_ADMINDN}/"		\
	-e "s/@MSAD_ADMINPW@/${MSAD_ADMINPW}/"		\
	-e "s/@MSAD_SUFFIX@/${MSAD_SUFFIX}/"
