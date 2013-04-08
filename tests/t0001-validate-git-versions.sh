#!/bin/sh

test_description='Check Git version is correct'
CGIT_TEST_NO_CREATE_REPOS=YesPlease
. ./setup.sh

test_expect_success 'extract Git version from Makefile' '
	sed -n -e "/^GIT_VER[ 	]*=/ {
		s/^GIT_VER[ 	]*=[ 	]*//
		p
	}" ../../Makefile >makefile_version
'

test_expect_success 'test Git version matches Makefile' '
	( cat ../../git/GIT-VERSION-FILE || echo "No GIT-VERSION-FILE" ) |
	sed -e "s/GIT_VERSION[ 	]*=[ 	]*//" >git_version &&
	test_cmp git_version makefile_version
'

test_expect_success 'test submodule version matches Makefile' '
	if ! test -e ../../git/.git
	then
		echo "git/ is not a Git repository" >&2
	else
		(
			cd ../.. &&
			sm_sha1=$(git ls-files --stage -- git |
				sed -e "s/^[0-9]* \\([0-9a-f]*\\) [0-9]	.*$/\\1/") &&
			cd git &&
			git describe --match "v[0-9]*" $sm_sha1
		) | sed -e "s/^v//" >sm_version &&
		test_cmp sm_version makefile_version
	fi
'

test_done