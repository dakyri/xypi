#!/bin/bash
# /etc/init.d/octo_cnx.sh
### BEGIN INIT INFO
# Provides:          octo_cnx.sh
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Manually connect to octo pi
# Description:       Manually connect to octo pi
### END INIT INFO
if ! ( grep octo /proc/asound/cards >& /dev/null ) ; then
	echo Octo not found. Manual setup ...
	sudo modprobe -r snd_soc_audioinjector_octo_soundcard
	sudo modprobe -r snd_soc_cs42xx8_i2c
	sudo modprobe -r snd_soc_cs42xx8
	echo Trying to install octo ...
	sudo modprobe snd_soc_cs42xx8
	sudo modprobe snd_soc_cs42xx8_i2c
	sudo modprobe snd_soc_audioinjector_octo_soundcard
else
	echo Octo already recognised
fi
