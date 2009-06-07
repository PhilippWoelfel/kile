#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 <version>"
    exit 1
fi

./createPackage.sh --svnroot "https://svn.kde.org/home/kde" -ab trunk/extragear/office -a kile --i18n-base trunk/l10n-kde4 \
--i18n-sub extragear-office --notoplevel --package --logfile $0.log -av $1