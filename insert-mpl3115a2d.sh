#!/bin/sh

temperature=0
pressure=0
altitude=0

read temperature < /var/lib/mpl3115a2d/temperature
read pressure < /var/lib/mpl3115a2d/pressure
read altitude < /var/lib/mpl3115a2d/altitude

/usr/bin/sqlite3 /var/lib/mpl3115a2d/mpl3115a2d.db "insert into mpl3115a2d \
(temperature,pressure,altitude) values \
(\"$temperature\",\"$pressure\",\"$altitude\");"

